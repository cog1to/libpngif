#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <errors.h>
#include <gif_decoded.h>

/** Utils **/

size_t gif_max_size(size_t a, size_t b) {
  if (a > b) {
    return a;
  } else {
    return b;
  }
}

unsigned char gif_bit_mask(size_t bits) {
  switch (bits) {
  case 0:
    return 0;
  case 1:
    return 1;
  case 2:
    return 3;
  case 3:
    return 7;
  case 4:
    return 15;
  case 5:
    return 31;
  case 6:
    return 63;
  case 7:
    return 127;
  default:
    return 255;
  }
}

/** LZW **/

typedef struct {
  // Size of the color table. This is needed to detect CLEAR and END codes.
  size_t color_table_size;
  // Max number of elements.
  size_t size;
  // Current number of elements.
  size_t element_count;
  // Number of bytes that each element can occupy.
  size_t stride;
  /*
   * Element storage:
   * First two bytes of each element is it's length, next LENGTH bytes are
   * color table indexes. The code table guarantees that there's enough space
   * allocated for all elements at all times.
   */
  unsigned char *elements;
} gif_lzw_code_table;

/**
 * Creates a new code table, initializes it with codes corresponding to color
 * table.
 *
 * @param color_table_size Number of colors in a color table.
 *
 * @return New instance of a code table.
 */
gif_lzw_code_table *gif_lzw_code_table_init(size_t color_table_size) {
  gif_lzw_code_table *table = calloc(1, sizeof(gif_lzw_code_table));
  if (table == NULL) {
    return NULL;
  }

  table->color_table_size = color_table_size;
  table->size = color_table_size * 2;
  table->stride = 6;
  table->elements = calloc(table->size, table->stride);
  table->element_count = color_table_size + 2;

  // Initial color table.
  for (
    size_t idx = 0, offset = 0;
    idx < color_table_size;
    idx++, offset += table->stride
  ) {
    u_int16_t *length = (u_int16_t *)(table->elements + offset);
    *length = 1;
    table->elements[offset + 2] = idx;
  }

  return table;
}

/**
 * Deallocates code table.
 *
 * @param table Code table to deallocate.
 */
void gif_lzw_code_table_free(gif_lzw_code_table *table) {
  if (table == NULL)
    return;

  if (table->elements != NULL)
    free(table->elements);

  free(table);
}

/**
 * Increases the size of the given code table
 *
 * @param old Code table to expand.
 * @param new_size New max number of elements.
 * @param new_stride New max length of a color index sequence for a code.
 * @param error Error output.
 */
void gif_lzw_code_table_resize(
  gif_lzw_code_table *old,
  size_t new_size,
  size_t new_stride,
  int *error
) {
  // Allocate new storage.
  unsigned char *new_elements = calloc(new_size, new_stride);
  if (new_elements == NULL) {
    *error = GIF_ERR_MEMIO;
    return;
  }

  /*
   * If element size isn't changed, just copy the old storage to the new one.
   * Otherwise, we have to copy each element separately because of the stride
   * difference between two storages.
   */
  if (new_stride == old->stride) {
    memcpy(new_elements, old->elements, old->stride * old->size);
  } else {
    for (
      size_t idx = 0, old_off = 0, new_off = 0;
      idx < old->element_count;
      idx++, old_off += old->stride, new_off += new_stride
    ) {
      memcpy(new_elements + new_off, old->elements + old_off, old->stride);
    }
  }

  // Switch the storage.
  free(old->elements);
  old->elements = new_elements;
  old->size = new_size;
  old->stride = new_stride;
}

/**
 * Returns an index sequence for a given code.
 *
 * @param table The code will be searched in this code table.
 * @param index Code value.
 * @param is_clear Output flag indicating whether the code is a clear code.
 * @param is_end Output flag indicating whether the code is an end of data code.
 *
 * @return A pointer to the beginning of the index sequence, or NULL in case of
 *   a special code (CLEAR or END) or if code is not yet in the table.
 */
unsigned char *gif_lzw_code_table_element_at(
  gif_lzw_code_table *table,
  size_t index,
  int *is_clear,
  int *is_end
) {
  if (is_clear != NULL) {
    if (index == table->color_table_size) {
      *is_clear = 1;
    } else {
      *is_clear = 0;
    }
  }

  if (is_end != NULL) {
    if (index == (table->color_table_size + 1)) {
      *is_end = 1;
    } else {
      *is_end = 0;
    }
  }

  if (index >= table->element_count) {
    return NULL;
  }

  return table->elements + (index * table->stride);
}

/**
 * Adds new index sequence to the code table.
 *
 * @param table Table to append new index sequence into.
 * @param element Index sequence to add.
 * @param error Error output.
 *
 * @return New element count after adding the sequence.
 */
size_t gif_lzw_code_table_append_element(
  gif_lzw_code_table *table,
  unsigned char *element,
  int *error
) {
  u_int16_t *elsize = (u_int16_t *)element;

  // Resize code table if needed.
  if (table->element_count == table->size || *elsize > (table->stride - 2)) {
    size_t new_size = (table->element_count == table->size)
      ? (table->size * 2)
      : table->size;
    size_t new_stride = (*elsize > (table->stride - 2))
      ? gif_max_size(table->stride * 2, *elsize + 2)
      : table->stride;

    gif_lzw_code_table_resize(
      table,
      new_size,
      new_stride,
      error
    );

    if (*error != 0) {
      return table->element_count;
    }
  }

  unsigned char *target = table->elements + (table->element_count * table->stride);
  memcpy(target, element, *elsize + 2);

  table->element_count = table->element_count + 1;
  return table->element_count;
}

/** Private **/

/**
 * LZW compression packs "codes" into bytes on bit-by-bit basis, i. e.
 * the data stream is treated as bit stream, not byte stream. After a code
 * is put into the stream, the next code starts at the exact BIT position
 * where the last one ended. For example, if we have code size of 3, the first
 * few bytes would look like this:
 *
 * byte 1    byte 2    byte 3
 * 33222111  65554443  88877766
 *
 * Here individual digits are code indexes. You can see that code 3 has first 2
 * bits stored in the first byte, and the remaning bit stored in the second
 * byte, code 6 starts at the last bit of byte 2, and spans into first 2 bits
 * of byte 3, and so on.
 *
 * This makes the decoding a little tricky, as you'll see in the code below.
 */

/**
 * Reads next code from the code data stream.
 *
 * @param output Pointer to the output value.
 * @param data Data stream.
 * @param offset Current bit offset in the data stream.
 * @param code_size Code size, i. e. number of bits to read.
 *
 * @return Bit offset after reading the code.
 */
u_int64_t gif_read_next_code(
  u_int16_t *output,
  unsigned char *data,
  u_int64_t offset,
  unsigned char code_size
) {
  int current_offset = offset;
  int bits_left = code_size;
  int byte_value, result = 0;

  // This could probably be heavily optimised, but I'm not smort enough.
  while (bits_left > 0) {
    // Get what byte we're in right now.
    int current_byte = current_offset / 8;
    // Get the number of bits to skip till the first code bit.
    int shift_right = current_offset - (current_byte * 8);

    // Read the byte.
    byte_value = data[current_byte];
    // Shift right to erase previous code data.
    byte_value = byte_value >> shift_right;
    // Erase everything past current code's end.
    byte_value = byte_value & (gif_bit_mask(bits_left));

    // Add what's left to the resulting code value, in a proper position.
    result = result + (byte_value << (code_size - bits_left));

    // Advance the bit offset past the data we've just read.
    current_offset += (8 - shift_right);
    // Substract the number of bits read from the total expected code size.
    bits_left -= (8 - shift_right);
  }

  *output = result;
  return (offset + code_size);
}

/**
 * Converts color index sequence into colors and adds it to the color data
 * storage.
 *
 * @param rgba Color data storage to append to.
 * @param offset Offset to the next unfilled pixel in the storage.
 * @param sequence Color index sequence to add to the storage.
 * @param color_table Color table to look up colors from color indices.
 * @param transparent_color_index Optional color index for transparent pixels.
 *
 * @return Offset to the next unfilled pixel after adding the sequence.
 */
int gif_rgba_add_sequence(
  unsigned char *rgba,
  u_int32_t offset,
  unsigned char *sequence,
  gif_color_t *color_table,
  unsigned char *transparent_color_index
) {
  u_int16_t *seq_length = (u_int16_t *)sequence;
  int new_offset = offset;

  for (int idx = 0; idx < *seq_length; idx++) {
    if (transparent_color_index != NULL && sequence[idx + 2] == *transparent_color_index) {
      memset(rgba + new_offset, 0, 4);
    } else {
      gif_color_t color = color_table[sequence[idx + 2]];
      rgba[new_offset + 0] = color.red;
      rgba[new_offset + 1] = color.green;
      rgba[new_offset + 2] = color.blue;
      rgba[new_offset + 3] = 255;
    }
    new_offset += 4;
  }

  return new_offset;
}

/**
 * Decodes LZW-encoded image data into RGBA color data.
 *
 * @param data Data to decode.
 * @param min_code_size Minimum code size (from image block data).
 * @param color_table_size Number of colors in the color table.
 * @param width Image width.
 * @param height Image height.
 * @param color_table Color table.
 * @param transparent_color_index Optional color index of a color that should
 *   be treated as full transparency.
 * @param interlaced Flag indicating whether the image is interlaced.
 * @param error Output error code.
 *
 * @return Decoded image data in RGBA format.
 */
unsigned char *gif_decode_image_data(
  unsigned char *data,
  unsigned char min_code_size,
  size_t color_table_size,
  u_int32_t width,
  u_int32_t height,
  gif_color_t *color_table,
  unsigned char *transparent_color_index,
  int interlaced,
  int *error
) {
  int code_size = min_code_size + 1;
  u_int64_t bit_offset = 0;
  u_int16_t current_code = 0;
  unsigned char *sequence = NULL;
  int is_end = 0, is_reset = 0;
  unsigned char buffer[2048] = { 0 };
  unsigned char seqtmp[2048] = { 0 };
  int max_code_count = 1 << code_size;

  gif_lzw_code_table *table = gif_lzw_code_table_init(color_table_size);
  if (table == NULL) {
    *error = GIF_ERR_MEMIO;
    return NULL;
  }

  // Allocate space for all pixel indexes.
  u_int32_t total_size = width * height * 4;
  int rgba_offset = 0;
  unsigned char *rgba = malloc(total_size);
  if (rgba == NULL) {
    gif_lzw_code_table_free(table);
    *error = GIF_ERR_MEMIO;
    return NULL;
  }

  /** Main decode loop **/

  // Current sequence and buffer lengths storage.
  u_int16_t *seq_size = 0;
  u_int16_t *buf_size = 0;
  // First code that is read has to be a RESET/CLEAR code.
  int expect_reset = 1;

  while (1) {
    bit_offset = gif_read_next_code(&current_code, data, bit_offset, code_size);
    sequence = gif_lzw_code_table_element_at(table, current_code, &is_reset, &is_end);
    if (is_end) {
      sequence = NULL;
      break;
    } else if (is_reset) {
      // Reset table.
      gif_lzw_code_table_free(table);
      table = gif_lzw_code_table_init(color_table_size);

      // Reset to start of data stream state.
      code_size = min_code_size + 1;
      max_code_count = 1 << code_size;
      expect_reset = 0;

      // Output first index after reset and initialize code buffer.
      bit_offset = gif_read_next_code(&current_code, data, bit_offset, code_size);
      sequence = gif_lzw_code_table_element_at(table, current_code, &is_reset, &is_end);
      rgba_offset = gif_rgba_add_sequence(
        rgba,
        rgba_offset,
        sequence,
        color_table,
        transparent_color_index
      );

      // Initialize code buffer.
      seq_size = (u_int16_t *)sequence;
      memcpy(buffer, sequence, *seq_size + 2);
    } else if (expect_reset && bit_offset == code_size) {
      /*
       * Why this weird condition? Because some GIFs ignore standard's
       * recommendation to put CLEAR code as a first code in the data stream.
       * We have to treat it as a normal situation and do initialization as if
       * the clear code was there.
       */
      expect_reset = 0;

      // Output the code.
      rgba_offset = gif_rgba_add_sequence(
        rgba,
        rgba_offset,
        sequence,
        color_table,
        transparent_color_index
      );

      // Initialize code buffer.
      seq_size = (u_int16_t *)sequence;
      memcpy(buffer, sequence, *seq_size + 2);
    } else {
      // We were expecting code table reset, but got a normal code.
      if (expect_reset) {
        *error = GIF_ERR_NO_RESET;
        break;
      } else if (sequence == NULL) {
        // New code entry: prev sequence + first element of prev sequence
        buf_size = (u_int16_t *)buffer;
        buffer[*buf_size + 2] = buffer[2];
        *buf_size = *buf_size + 1;
        // Output new sequence.
        rgba_offset = gif_rgba_add_sequence(
          rgba,
          rgba_offset,
          buffer,
          color_table,
          transparent_color_index
        );
        // Append new entry to the code table.
        gif_lzw_code_table_append_element(table, buffer, error);
      } else {
        // Output current sequence to the index stream.
        rgba_offset = gif_rgba_add_sequence(
          rgba,
          rgba_offset,
          sequence,
          color_table,
          transparent_color_index
        );
        /* New code entry: previous sequence + first element in new one */
        // Copy sequence to temp store, because code table append can move the
        // data to a different memory block.
        seq_size = (u_int16_t *)sequence;
        memcpy(seqtmp, sequence, *seq_size + 2);

        // Append first element to the buffer.
        seq_size = (u_int16_t *)seqtmp;
        buf_size = (u_int16_t *)buffer;
        buffer[*buf_size + 2] = sequence[2];
        *buf_size = *buf_size + 1;

        // Append new entry to the code table.
        gif_lzw_code_table_append_element(table, buffer, error);

        // Replace buffer with current sequence.
        memcpy(buffer, seqtmp, *seq_size + 2);
        sequence = NULL;
      }

      // Check if need to increase code size.
      if (table->element_count == max_code_count) {
        /*
         * Do not increase code size beyond 12, it's max value defined in GIF
         * standard. Instead set the `expect_reset` flag on, becase the next
         * code MUST be a reset code.
         */
        if (max_code_count == 4096) {
          expect_reset = 1;
        } else {
          code_size += 1;
          max_code_count = 1 << code_size;
        }
      }
    }
  }

  if (interlaced) {
    unsigned char *deinterlaced = malloc(width * height * 4);
    if (deinterlaced == NULL) {
      free(rgba);
      *error = GIF_ERR_MEMIO;
      return NULL;
    }

    // Interweave decoded data into output according to interlacing rules.
    static int pass_offset[] = { 0, 4, 2, 1 };
    static int pass_stride[] = { 8, 8, 4, 2 };
    u_int32_t line_in = 0;
    for (int pass = 0; pass < 4; pass++) {
      for (
        u_int32_t line_out = pass_offset[pass];
        line_out < height;
        line_out += pass_stride[pass], line_in++
      ) {
        memcpy(deinterlaced + (width * line_out * 4), rgba + (width * line_in * 4), width * 4);
      }
    }

    // Swap output with deinterlaced data.
    free(rgba);
    rgba = deinterlaced;
  }

  return rgba;
}

/**
 * Decoded a single image block.
 *
 * @param decoded Decoded data container. Will be filled with decoded data.
 * @param image Image block to decode.
 * @param global_color_table_size Global color table size, if present.
 * @param global_color_table A pointer to a global color table, if present.
 * @param error Output error code.
 */
void gif_decode_image_block(
  gif_decoded_image_t *decoded,
  gif_image_block_t *image,
  size_t global_color_table_size,
  gif_color_t *global_color_table,
  int *error
) {
  gif_color_t *color_table;
  size_t color_table_size;
  unsigned char *transparent_color_index = NULL;

  /*
   * The color table might be embedded in the image block. Otherwise use global
   * one.
   */
  if (image->color_table != NULL && image->descriptor.color_table_size > 0) {
    color_table = image->color_table;
    color_table_size = image->descriptor.color_table_size;
  } else {
    color_table = global_color_table;
    color_table_size = global_color_table_size;
  }

  // Check for transparency support.
  if (
    image->gc != NULL &&
    image->gc->transparency_flag
  ) {
    transparent_color_index = &(image->gc->transparent_color_index);
  }

  // Decode image data into RGBA.
  unsigned char *rgba = gif_decode_image_data(
    image->data,
    image->minimum_code_size,
    color_table_size,
    image->descriptor.width,
    image->descriptor.height,
    color_table,
    transparent_color_index,
    image->descriptor.interlace,
    error
  );

  if (*error != 0) {
    return;
  }

  decoded->rgba = rgba;
  decoded->top = image->descriptor.top;
  decoded->left = image->descriptor.left;
  decoded->width = image->descriptor.width;
  decoded->height = image->descriptor.height;
  if (image->gc != NULL) {
    decoded->dispose_method = image->gc->dispose_method;
    decoded->delay_cs = image->gc->delay_cs;
  }
}

/** Public **/

gif_decoded_t *gif_decoded_from_parsed(gif_parsed_t *parsed, int *error) {
  if (parsed == NULL) {
    return NULL;
  }

  gif_decoded_t *decoded = calloc(1, sizeof(gif_decoded_t));
  if (decoded == NULL) {
    *error = GIF_ERR_MEMIO;
    return NULL;
  }

  decoded->width = parsed->screen.width;
  decoded->height = parsed->screen.height;
  decoded->pixel_ratio = parsed->screen.pixel_aspect_ratio;

  if (parsed->screen.background_color_index > 0 && parsed->global_color_table != NULL) {
    gif_color_t color = parsed->global_color_table[parsed->screen.background_color_index];
    decoded->background_color = malloc(sizeof(gif_color_t));
    if (decoded->background_color == NULL) {
      free(decoded);
      *error = GIF_ERR_MEMIO;
      return NULL;
    }
    memcpy(decoded->background_color, &color, 3);
  }

  // Count images first.
  int image_count = 0;
  for (int idx = 0; idx < parsed->block_count; idx++) {
    gif_block_t *block = parsed->blocks[idx];
    if (block->type == GIF_BLOCK_IMAGE) {
      image_count += 1;
    }
  }

  // Allocate space for images.
  gif_decoded_image_t *images = malloc(sizeof(gif_decoded_image_t) * image_count);
  if (images == NULL) {
    *error = GIF_ERR_MEMIO;
    gif_decoded_free(decoded);
    return NULL;
  }

  int image_idx = 0;
  for (int idx = 0; idx < parsed->block_count; idx++) {
    gif_block_t *block = parsed->blocks[idx];

    /*
     * Application extension block. The only one we support now is the Netscape
     * extension that specifies the animation repeat count.
     */
    if (block->type == GIF_BLOCK_APPLICATION) {
      gif_application_block_t *app = (gif_application_block_t *)block;
      if (strcmp(app->identifier, "NETSCAPE") == 0 && strcmp(app->auth_code, "2.0") == 0) {
        decoded->animated = 1;
        u_int16_t repeat_count = *(u_int16_t*)(app->data + 1);
        decoded->repeat_count = repeat_count;
      }
    } else if (block->type == GIF_BLOCK_IMAGE) {
      gif_image_block_t *image_block = (gif_image_block_t *)block;
      gif_decode_image_block(
        images + image_idx,
        image_block,
        parsed->screen.color_table_size,
        parsed->global_color_table,
        error
      );

      if (*error != 0) {
        break;
      } else {
        decoded->image_count += 1;
        image_idx += 1;
      }
    } else {
      // Ignore everything else, including comment and plain text, for now.
    }
  }

  if (decoded->image_count > 0) {
    decoded->images = images;
  }
  return decoded;
}

gif_decoded_t *gif_decoded_from_data(unsigned char *data, size_t size, int *error) {
  if (data == NULL) {
    *error = GIF_ERR_NO_DATA;
    return NULL;
  }

  gif_parsed_t *parsed = gif_parsed_from_data(data, size, error);
  if (*error != 0) {
    return NULL;
  }

  gif_decoded_t *decoded = gif_decoded_from_parsed(parsed, error);
  free(parsed);
  return decoded;
}

gif_decoded_t *gif_decoded_from_file(FILE *file, int *error) {
  gif_parsed_t *parsed = gif_parsed_from_file(file, error);
  if (*error != 0) {
    return NULL;
  }

  gif_decoded_t *decoded = gif_decoded_from_parsed(parsed, error);
  free(parsed);
  return decoded;
}

gif_decoded_t *gif_decoded_from_path(char *path, int *error) {
  gif_parsed_t *parsed = gif_parsed_from_path(path, error);
  if (*error != 0) {
    return NULL;
  }

  gif_decoded_t *decoded = gif_decoded_from_parsed(parsed, error);
  free(parsed);
  return decoded;
}

void gif_decoded_free(gif_decoded_t *gif) {
  if (gif->background_color != NULL) {
    free(gif->background_color);
  }

  if (gif->images != NULL && gif->image_count > 0) {
    for (int idx = 0; idx < gif->image_count; idx++) {
      gif_decoded_image_t image = gif->images[idx];
      free(image.rgba);
    }

    free(gif->images);
  }
}
