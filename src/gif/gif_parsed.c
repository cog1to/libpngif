#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <pngif/utils.h>
#include <pngif/errors.h>
#include <pngif/gif_parsed.h>

/** Private **/

static const unsigned char TRAILER = 0x3B;
static const unsigned char INTRO_EXT = 0x21;
static const unsigned char INTRO_IMAGE_DESC = 0x2C;

static const unsigned char EXT_GRAPHIC_CONTROL = 0xF9;
static const unsigned char EXT_PLAIN_TEXT = 0x01;
static const unsigned char EXT_APPLICATION = 0xFF;
static const unsigned char EXT_COMMENT = 0xFE;

/**
 * Frees the GIF data block memory. Does a proper data deallocation depending
 * on the block type.
 *
 * @param block Block to deallocate.
 */
void gif_free_block(gif_block_t *block) {
  if (block == NULL)
    return;

  if (block->type == GIF_BLOCK_APPLICATION) {
    gif_application_block_t *app_block = (gif_application_block_t *)block;
    if (app_block->data != NULL)
      free(app_block->data);
  } else if (block->type == GIF_BLOCK_COMMENT || block->type == GIF_BLOCK_PLAIN_TEXT) {
    gif_text_block_t *text_block = (gif_text_block_t *)block;
    if (text_block->data != NULL && text_block->length > 0)
      free(text_block->data);
  } else if (block->type == GIF_BLOCK_IMAGE) {
    gif_image_block_t *image = (gif_image_block_t *)block;
    if (image->gc != NULL) {
      free(image->gc);
    }
    if (image->color_table != NULL) {
      free(image->color_table);
    }
    if (image->data != NULL && image->data_length > 0) {
      free(image->data);
    }
  }

  free(block);
}

/**
 * Appends data block to the block list.
 *
 * @param list List of block pointers to append to.
 * @param block Block to append to the list.
 * @param count Size of the list. Will be updated with the current count after
 *   append.
 * @param error Error output.
 *
 * @return Pointer to the list holding all previous blocks plus the new one.
 */
gif_block_t **append_block(gif_block_t **list, gif_block_t *block, size_t *count, int *error) {
  list = realloc(list, sizeof(gif_block_t *) * (*count + 1));
  if (list != NULL) {
    list[*count] = block;
    *count = *count + 1;
    return list;
  } else {
    *error = GIF_ERR_MEMIO;
    return NULL;
  }
}

/**
 * Frees the memory occupied by a block list and its elements.
 *
 * @param list Block list to deallocate.
 * @param count Number of elements in the list.
 */
void gif_free_block_list(gif_block_t **list, size_t count) {
  for (int idx = 0; idx < count; idx++) {
    gif_free_block(list[idx]);
  }
  free(list);
}

/**
 * Reads consecutive data blocks from the data stream, starting from given
 * offset until a terminator is encountered, and concatenates them into a
 * single data array.
 *
 * @param output An array pointer that will hold the resulting data.
 * @param data Input data stream.
 * @param length Total length of the data stream.
 * @param offset Offset in the data, from which to start reading data blocks.
 * @param new_offset Output value to hold offset after reading all data blocks.
 * @param Error output.
 *
 * @return Total number of bytes read.
 */
size_t concat_data_blocks(
  unsigned char **output,
  unsigned char *data,
  size_t length,
  size_t offset,
  size_t *new_offset,
  int *error
) {
  size_t block_count = 0, byte_count = 0;
  size_t tmp = offset, data_offset = 0;

  // Gather data total size.
  while (tmp < length) {
    unsigned char block_length = data[tmp];
    if (block_length > 0) {
      byte_count += block_length;
      if (tmp + 1 + byte_count > length) {
        *error = GIF_ERR_BAD_BLOCK_SIZE;
        break;
      }
      block_count += 1;
      tmp += (block_length + 1);
    } else {
      break;
    }
  }

  unsigned char *out = malloc(byte_count);
  if (out == NULL) {
    *error = GIF_ERR_MEMIO;
    return -1;
  }

  tmp = offset;
  for (int idx = 0; idx < block_count; idx++) {
    unsigned char block_length = data[tmp];
    memcpy(out + data_offset, data + tmp + 1, block_length);
    data_offset += block_length;
    tmp += (block_length + 1);
  }

  /*
   * Offset to the next section would be last the data point + 1 for the
   * terminator byte '00'.
   */
  *new_offset = tmp + 1;
  *output = out;

  return byte_count;
}

/**
 * Reads text block from the data stream.
 *
 * @param type Text block type. Should be either Plain Text or Comment type.
 * @param data Input data stream.
 * @param data_length Total length of the data stream.
 * @param offset Offset to the beginning of the text data blocks section.
 * @param new_offset Output value to hold new offset after reading all data
 *   blocks.
 * @param error Error output.
 *
 * @return Text block struct holding the data that was read.
 */
gif_text_block_t *gif_read_text_block(
  unsigned char type,
  unsigned char *data,
  size_t data_length,
  size_t offset,
  size_t *new_offset,
  int *error
) {
  gif_text_block_t *text = malloc(sizeof(gif_text_block_t));
  if (text == NULL) {
    *error = GIF_ERR_MEMIO;
    return NULL;
  }

  unsigned char *text_data = NULL;
  size_t bytes_read = concat_data_blocks(&text_data, data, data_length, offset, new_offset, error);

  if (error != 0) {
    if (text_data != NULL) {
      free(text_data);
    }
    free(text);
    return NULL;
  }

  text_data = realloc(text_data, bytes_read + 1);
  if (text_data == NULL) {
    free(text);
    return NULL;
  } else {
    text_data[bytes_read] = '\0';
  }

  text->type = type;
  text->length = bytes_read;
  text->data = (char *)text_data;

  return text;
}

/**
 * Reads application extension block from the data stream.
 *
 * @param data Input data stream.
 * @param length Total length of the data stream.
 * @param offset Offset to the beginning of the application data blocks
 *   section.
 * @param new_offset Output value to hold new offset after reading all data
 *   blocks.
 * @param error Error output.
 *
 * @return Application extension block struct holding the data that was read.
 */
gif_application_block_t *gif_read_application_block(
  unsigned char *data,
  size_t length,
  size_t offset,
  size_t *new_offset,
  int *error
) {
  unsigned char signature_size = data[offset];
  if (signature_size != 11) {
    *error = GIF_ERR_BAD_FORMAT;
    return NULL;
  }

  gif_application_block_t *block = calloc(1, sizeof(gif_application_block_t));
  if (block == NULL) {
    *error = GIF_ERR_MEMIO;
    return NULL;
  }

  // Application signature data.
  memcpy(block->identifier, data + offset + 1, 8);
  memcpy(block->auth_code, data + offset + 9, 3);
  block->length = concat_data_blocks(&block->data, data, length, offset + 12, new_offset, error);

  if (block->data == NULL || *error != 0) {
    free(block);
    return NULL;
  }

  block->type = GIF_BLOCK_APPLICATION;

  return block;
}

/**
 * Reads Graphics Context extension block from the data stream.
 *
 * @param data Input data stream.
 * @param length Total length of the data stream.
 * @param start Offset to the beginning of the application data blocks
 *   section.
 * @param out_offset Output value to hold new offset after reading all data
 *   blocks.
 * @param error Error output.
 *
 * @return Graphics Context extension block struct.
 */
gif_gc_block_t *gif_read_gc_block(
  unsigned char *data,
  size_t length,
  size_t start,
  size_t *out_offset,
  int *error
) {
  unsigned char size = data[start];
  if (size != 4) {
    *error = GIF_ERR_BAD_BLOCK_SIZE;
    return NULL;
  }

  // Allocate new GC block.
  gif_gc_block_t *out = calloc(1, sizeof(gif_gc_block_t));
  if (out == NULL) {
    *error = GIF_ERR_MEMIO;
    return NULL;
  }

  /*
   * Packed settings byte: 000|ddd|u|t
   * 000 - reserverd for future use
   * ddd - disposal method
   * u - user input flag
   * t - trasparency flag
   */
  unsigned char settings_byte = data[start + 1];
  out->dispose_method = (settings_byte & 0x1C) >> 2;
  out->input_flag = (settings_byte & 0x2) >> 1;
  out->transparency_flag = (settings_byte & 0x1);

  // Frame delay time. If zero, assume 100 milliseconds.
  u_int16_t delay_time = *(u_int16_t*)(data + start + 2);
  out->delay_cs = (delay_time > 0) ? delay_time : 10;

  // Transparent color index.
  out->transparent_color_index = data[start + 4];

  // Skip block terminator and return.
  *out_offset = start + 6;
  return out;
}

/**
 * Reads an image block from the data stream.
 *
 * @param data Input data stream.
 * @param length Total length of the data stream.
 * @param start Offset to the beginning of the application data blocks
 *   section.
 * @param gc Graphics Context block that was preceding the image block.
 * @param out_offset Output value to hold new offset after reading all data
 *   sub-blocks.
 * @param error Error output.
 *
 * @return Image block struct holding the parsed data.
 */
gif_image_block_t *gif_read_image_block(
  unsigned char *data,
  size_t length,
  size_t start,
  gif_gc_block_t *gc,
  size_t *out_offset,
  int *error
) {
  gif_image_block_t *image = calloc(1, sizeof(gif_image_block_t));
  if (image == NULL) {
    *error = GIF_ERR_MEMIO;
    return NULL;
  }

  u_int16_t left = *(u_int16_t *)(data + start);
  u_int16_t top = *(u_int16_t *)(data + start + 2);
  u_int16_t width = *(u_int16_t *)(data + start + 4);
  u_int16_t height = *(u_int16_t *)(data + start + 6);

  unsigned char settings = data[start + 8];
  unsigned char interlace = (settings & 0x40) > 0 ? 1 : 0;
  unsigned char color_table_flag = settings & 128;
  size_t color_table_size = 0;

  image->descriptor.left = left;
  image->descriptor.top = top;
  image->descriptor.width = width;
  image->descriptor.height = height;
  image->descriptor.interlace = interlace;
  image->descriptor.color_table_size = color_table_size;

  if (gc != NULL) {
    image->gc = gc;
  }

  if (color_table_flag) {
    color_table_size = 1 << ((settings & 0x07) + 1);
    gif_color_t *table = malloc(sizeof(gif_color_t) * color_table_size);

    if (table == NULL) {
      free(image);
      *error = GIF_ERR_MEMIO;
      return NULL;
    }

    for (int idx = 0; idx < color_table_size; idx++) {
      size_t offset = idx * 3;
      table[idx].red = data[start + 9 + offset + 0];
      table[idx].green = data[start + 9 + offset + 1];
      table[idx].blue = data[start + 9 + offset + 2];
    }

    image->color_table = table;
    image->descriptor.color_table_size = color_table_size;
  }

  // Concatenate all image blocks.
  size_t offset = start + 9 + color_table_size * 3, block_size = 0, data_size = 0;
  unsigned char *image_data = NULL;

  image->minimum_code_size = data[offset];
  offset += 1;

  while ((block_size = data[offset]) > 0) {
    // Read the block.
    image_data = realloc(image_data, data_size + block_size);
    if (image_data == NULL) {
      *error = GIF_ERR_MEMIO;
      break;
    } else {
      memcpy(image_data + data_size, data + offset + 1, block_size);
      data_size += block_size;
    }

    // Advance the offset to the next block.
    offset += (block_size + 1);
  }

  if (*error == 0) {
    image->data_length = data_size;
    image->data = image_data;
  } else {
    free(image_data);
  }

  image->type = GIF_BLOCK_IMAGE;
  *out_offset = offset + 1;
  return image;
}

/** Public **/

gif_parsed_t *gif_parsed_from_data(unsigned char *data, size_t size, int *error) {
  // Check the header.
  if (data[0] != 'G' || data[1] != 'I' || data[2] != 'F') {
    *error = GIF_ERR_BAD_HEADER;
    return NULL;
  }

  // Allocate the GIF struct to hold parsed data.
  gif_parsed_t *gif = calloc(1, sizeof(gif_parsed_t));
  if (gif == NULL) {
    *error = GIF_ERR_MEMIO;
    return NULL;
  }

  // Copy the version.
  memcpy(gif->version, data + 3, 3);

  /** Logical Screen Descriptor Start **/

  u_int16_t width = *(u_int16_t*)(data + 6);
  u_int16_t height = *(u_int16_t*)(data + 8);
  gif->screen.width = width;
  gif->screen.height = height;

  /*
   * Color table info, packed into a byte: f|rrr|s|ccc
   * f - global color table flag
   * rrr - color resolution
   * s - sort flag
   * ccc - color table resolution
   *
   * "Resolution" specifies the number of bits required to store all colors.
   * The color table size equals 2^(resolution+1).
   */
  unsigned char color_table_info = data[10];
  if (color_table_info & 128) {
    unsigned char color_table_res = (color_table_info & 0x70) >> 4;
    gif->screen.color_resolution = color_table_res + 1;

    size_t color_table_size = (color_table_info & 0x07);
    gif->screen.color_table_size = 1 << (color_table_size + 1);
  }

  gif->screen.background_color_index = data[11];
  gif->screen.pixel_aspect_ratio = data[12];

  /** Logical Screen Descriptor End **/

  // Global color table.
  size_t table_size = gif->screen.color_table_size;
  if (table_size > 0) {
    gif_color_t *table = malloc(sizeof(gif_color_t) * table_size);
    if (table == NULL) {
      free(data);
      free(gif);
      *error = GIF_ERR_MEMIO;
      return NULL;
    }

    for (int idx = 0; idx < table_size; idx++) {
      int offset = idx * 3;
      table[idx].red = data[13 + offset + 0];
      table[idx].green = data[13 + offset + 1];
      table[idx].blue = data[13 + offset + 2];
    }

    gif->global_color_table = table;
  }

  // Data blocks start here.
  size_t offset = 13 + table_size * 3;
  gif_gc_block_t *gc = NULL;
  gif_block_t *block = NULL;
  size_t block_count = 0;
  gif_block_t **blocks = NULL;

  /*
   * Each data block starts with a type byte, optinally followed by an
   * extension type byte. We check those and branch the parser accordingly.
   */
  while (offset < size && data[offset] != TRAILER && *error == 0) {
    if (data[offset] == INTRO_EXT && data[offset + 1] == EXT_PLAIN_TEXT) {
      block = (gif_block_t *)gif_read_text_block(
        GIF_BLOCK_PLAIN_TEXT,
        data,
        size,
        offset + 2,
        &offset,
        error
      );
    } else if (data[offset] == INTRO_EXT && data[offset + 1] == EXT_APPLICATION) {
      block = (gif_block_t *)gif_read_application_block(
        data,
        size,
        offset + 2,
        &offset,
        error
      );
    } else if (data[offset] == INTRO_EXT && data[offset + 1] == EXT_COMMENT) {
      block = (gif_block_t *)gif_read_text_block(
        GIF_BLOCK_COMMENT,
        data,
        size,
        offset + 2,
        &offset,
        error
      );
    } else if (data[offset] == INTRO_EXT && data[offset + 1] == EXT_GRAPHIC_CONTROL) {
      /*
       * Graphical Control Extension block is always preceding the image block.
       * We don't save it to the block list, instead we parse it and add it to the
       * image block that's coming next.
       */
      gc = gif_read_gc_block(
        data,
        size,
        offset + 2,
        &offset,
        error
      );
    } else if (data[offset] == INTRO_IMAGE_DESC) {
      block = (gif_block_t *)gif_read_image_block(
        data,
        size,
        offset + 1,
        gc,
        &offset,
        error
      );
      gc = NULL;
    } else if (data[offset] == INTRO_EXT) {
      // Unknown extension block type. Skip through each sub-block.
      offset += 2;
      unsigned char block_size = 0;
      while ((block_size = data[offset]) > 0) {
        offset += (block_size + 1);
      }
      offset += 1; // Skip block terminator.
      block = NULL;
    } else {
      *error = GIF_ERR_UNKNOWN_BLOCK;
      break;
    }

    if (block != NULL) {
      blocks = append_block(blocks, block, &block_count, error);
      block = NULL;
    }
  }

  if (*error != 0) {
    gif_free_block_list(blocks, block_count);
  } else {
    gif->block_count = block_count;
    gif->blocks = blocks;
  }

  // Clean up and return.
  return gif;
}

gif_parsed_t *gif_parsed_from_file(FILE *file, int *error) {
  unsigned char *data = NULL;

  size_t size = pngif_read_file(file, &data, error);
  if (*error != 0) {
    return NULL;
  }

  gif_parsed_t *parsed = gif_parsed_from_data(data, size, error);
  free(data);
  return parsed;
}

gif_parsed_t *gif_parsed_from_path(const char *filename, int *error) {
  FILE *file = fopen(filename, "r");
  if (file == NULL) {
    *error = GIF_ERR_FILEIO_CANT_OPEN;
    return 0;
  }

  gif_parsed_t *parsed = gif_parsed_from_file(file, error);
  fclose(file);
  return parsed;
}

void gif_parsed_free(gif_parsed_t *gif) {
  if (gif->global_color_table != NULL && gif->screen.color_table_size > 0) {
    free(gif->global_color_table);
  }

  if (gif->blocks != NULL && gif->block_count > 0) {
    gif_free_block_list(gif->blocks, gif->block_count);
  }

  free(gif);
}

