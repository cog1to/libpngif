#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <errors.h>
#include <gif_decoded.h>

/** Utils **/

size_t max_size(size_t a, size_t b) {
  if (a > b) {
    return a;
  } else {
    return b;
  }
}

int8_t max_int8(int8_t a, int8_t b) {
  if (a > b) {
    return a;
  } else {
    return b;
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
  // Element storage:
  // First two bytes of each element is it's length, next LENGTH bytes are
  // color table indexes. The code table guarantees that there's enough space
  // allocated for all elements at all times.
  unsigned char *elements;
} gif_lzw_code_table;

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

void gif_lzw_code_table_free(gif_lzw_code_table *table) {
  if (table == NULL)
    return;

  if (table->elements != NULL)
    free(table->elements);

  free(table);
}

gif_lzw_code_table *gif_lzw_code_table_resize(
  gif_lzw_code_table *old,
  size_t new_size,
  size_t new_stride
) {
  // Allocate new storage.
  unsigned char *new_elements = calloc(new_size, new_stride);
  if (new_elements == NULL) {
    return NULL;
  }

  // If element size isn't changed, just copy the old storage to the new one.
  // Otherwise, we have to copy each element separately because of the stride
  // difference between two storages.
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
  return old;
}

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
      ? max_size(table->stride * 2, *elsize + 2)
      : table->stride;

    gif_lzw_code_table *new_table = gif_lzw_code_table_resize(
      table,
      new_size,
      new_stride
    );

    if (new_table == NULL) {
      *error = GIF_ERR_MEMIO;
      return table->element_count;
    }
  }

  unsigned char *target = table->elements + (table->element_count * table->stride);
  memcpy(target, element, *elsize + 2);

  table->element_count = table->element_count + 1;
  return table->element_count;
}

/** Index list **/

typedef struct {
  size_t count;
  unsigned char *elements;
} gif_index_list_t;

void gif_index_list_add_seq(gif_index_list_t *list, unsigned char *seq) {
  u_int16_t *seq_length = (u_int16_t *)seq;
  memcpy(list->elements + list->count, seq + 2, *seq_length);
  list->count = list->count + *seq_length;
}

gif_index_list_t *gif_index_list_init(size_t size) {
  gif_index_list_t *list = calloc(size, 1);

  if (list != NULL) {
    list->elements = malloc(size);
    if (list->elements != NULL) {
      list->count = 0;
    } else {
      free(list);
      return NULL;
    }
  }

  return list;
}

void gif_index_list_free(gif_index_list_t *list) {
  if (list == NULL)
    return;

  if (list->elements != NULL)
    free(list->elements);

  free(list);
}

/** Private **/

unsigned char bit_mask(size_t bits) {
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

u_int64_t gif_read_next_code(
  u_int16_t *output,
  unsigned char *data,
  u_int64_t offset,
  unsigned char code_size
) {
  int current_offset = offset;
  int bits_left = code_size;
  int byte_value, result = 0;

  while (bits_left > 0) {
    int current_byte = current_offset / 8;
    int shift_right = current_offset - (current_byte * 8);

    byte_value = data[current_byte];
    byte_value = byte_value >> shift_right;
    byte_value = byte_value & (bit_mask(bits_left));

    result = result + (byte_value << (code_size - bits_left));

    current_offset += (8 - shift_right);
    bits_left -= (8 - shift_right);
  }
  *output = result;
  return (offset + code_size);
}

gif_index_list_t *gif_decode_image_data(
  unsigned char *data,
  unsigned char min_code_size,
  size_t color_table_size,
  u_int32_t width,
  u_int32_t height,
  int *error
) {
  int code_size = min_code_size + 1;
  u_int64_t bit_offset = 0;
  u_int16_t current_code = 0;
  unsigned char *sequence = NULL;
  int is_end = 0, is_reset = 0;
  unsigned char buffer[2048] = { 0 };
  int max_code_count = 1 << code_size;

  gif_lzw_code_table *table = gif_lzw_code_table_init(color_table_size);
  if (table == NULL) {
    *error = GIF_ERR_MEMIO;
    return NULL;
  }

  // Allocate space for all pixel indexes.
  u_int32_t total_size = width * height;
  gif_index_list_t *indexes = gif_index_list_init(total_size);
  if (indexes == NULL) {
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

      // Reset code size.
      code_size = min_code_size + 1;
      max_code_count = 1 << code_size;
      expect_reset = 0;
      sequence = NULL;

      // Read first index after reset and initialize code buffer.
      bit_offset = gif_read_next_code(&current_code, data, bit_offset, code_size);
      sequence = gif_lzw_code_table_element_at(table, current_code, &is_reset, &is_end);
      gif_index_list_add_seq(indexes, sequence);
      seq_size = (u_int16_t *)sequence;
      memcpy(buffer, sequence, *seq_size + 2);
    } else {
      if (expect_reset) {
        *error = GIF_ERR_NO_RESET;
        break;
      } else if (sequence == NULL) {
        // New code entry: prev sequence + first element of prev sequence
        buf_size = (u_int16_t *)buffer;
        buffer[*buf_size + 2] = buffer[2];
        *buf_size = *buf_size + 1;
        // Output new sequence.
        gif_index_list_add_seq(indexes, buffer);
        // Append new entry to the code table.
        gif_lzw_code_table_append_element(table, buffer, error);
      } else {
        // Output current sequence to the index stream.
        gif_index_list_add_seq(indexes, sequence);
        seq_size = (u_int16_t *)sequence;
        // New code entry: previous sequence + first element in new one
        buf_size = (u_int16_t *)buffer;
        buffer[*buf_size + 2] = sequence[2];
        *buf_size = *buf_size + 1;
        // Append new entry to the code table.
        gif_lzw_code_table_append_element(table, buffer, error);

        // Replace buffer with current sequence.
        memcpy(buffer, sequence, *seq_size + 2);
        sequence = NULL;
      }

      // Check if need to increase code size.
      if (table->element_count == max_code_count) {
        // Do not increase code size beyond 12, it's max value defined in GIF
        // standard. Instead set the `expect_reset` flag on, becase the next
        // code MUST be a reset code.
        if (max_code_count == 4096) {
          expect_reset = 1;
        } else {
          code_size += 1;
          max_code_count = 1 << code_size;
        }
      }
    }
  }

  return indexes;
}

void gif_decode_image_block(
  gif_decoded_image_t *decoded,
  gif_image_block_t *image,
  size_t global_color_table_size,
  gif_color_t *global_color_table,
  int *error
) {
  gif_color_t *color_table;
  size_t color_table_size;

  // The color table might be embedded in the image block. Otherwise use global
  // one.
  if (image->color_table != NULL && image->descriptor.color_table_size > 0) {
    color_table = image->color_table;
    color_table_size = image->descriptor.color_table_size;
  } else {
    color_table = global_color_table;
    color_table_size = global_color_table_size;
  }

  // Decode image data into index list.
  gif_index_list_t *indexes = gif_decode_image_data(
    image->data,
    image->minimum_code_size,
    color_table_size,
    image->descriptor.width,
    image->descriptor.height,
    error
  );

  if (*error != 0) {
    if (indexes != NULL)
      gif_index_list_free(indexes);
    return;
  }

  // Decode index list into colors.
  unsigned char *rgba = malloc(indexes->count * 4);
  if (rgba == NULL) {
    *error = GIF_ERR_MEMIO;
    gif_index_list_free(indexes);
    return;
  }

  for (int idx = 0, offset = 0; idx < indexes->count; idx++, offset += 4) {
    unsigned char color_index = indexes->elements[idx];

    // TODO: Support interlacing.
    if (
      image->gc != NULL &&
      image->gc->transparency_flag &&
      color_index == image->gc->transparent_color_index
    ) {
      memset(rgba + offset, 0, 4);
    } else {
      rgba[offset + 0] = color_table[color_index].red;
      rgba[offset + 1] = color_table[color_index].green;
      rgba[offset + 2] = color_table[color_index].blue;
      rgba[offset + 3] = 255;
    }
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

    // Application extension block. The only one we support now is the Netscape
    // extension that specifies the animation repeat count.
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
