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
  unsigned char color_table_size;
  // Max number of elements.
  size_t size;
  // Current number of elements.
  size_t element_count;
  // Number of bytes that each element can occupy.
  size_t stride;
  // Element storage:
  // First byte of each element is it's length, next LENGTH bytes are color
  // table indexes. The code table guarantees that there's enough space
  // allocated for all elements at all times.
  unsigned char *elements;
} gif_lzw_code_table;

gif_lzw_code_table *gif_lzw_code_table_init(unsigned char color_table_size) {
  gif_lzw_code_table *table = calloc(1, sizeof(gif_lzw_code_table));
  if (table == NULL) {
    return NULL;
  }

  table->color_table_size = color_table_size;
  table->size = color_table_size * 2;
  table->stride = 4;
  table->elements = calloc(table->size, table->stride);
  table->element_count = color_table_size + 2;

  // Initial color table.
  for (
    size_t idx = 0, offset = 0;
    idx < color_table_size;
    idx++, offset += table->stride
  ) {
    table->elements[offset] = 1;
    table->elements[offset + 1] = idx;
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
  if (index >= table->element_count) {
    return NULL;
  }

  if (index == table->color_table_size && is_clear != NULL) {
    *is_clear = 1;
  } else {
    *is_clear = 0;
  }

  if (index == (table->color_table_size + 1) && is_end != NULL) {
    *is_end = 1;
  } else {
    *is_end = 0;
  }

  return table->elements + (index * table->stride);
}

size_t gif_lzw_code_table_append_element(
  gif_lzw_code_table *table,
  unsigned char *element,
  int *error
) {
  unsigned char elsize = element[0];

  // Resize code table if needed.
  if (table->element_count == table->size || elsize >= table->stride) {
    size_t new_size = (table->element_count == table->size)
      ? (table->size * 2)
      : table->size;
    size_t new_stride = (elsize >= table->stride)
      ? max_size(table->stride * 2, elsize + 1)
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
  target[0] = elsize;
  memcpy(target + 1, element + 1, elsize);

  table->element_count = table->element_count + 1;
  return table->element_count;
}

/** Index list **/

typedef struct {
  size_t count;
  unsigned char *elements;
} gif_index_list_t;

void gif_index_list_add_seq(gif_index_list_t *list, unsigned char *seq) {
  int seq_length = seq[0];
  memcpy(list->elements + list->count, seq + 1, seq_length);
  list->count = list->count + seq_length;
}

gif_index_list_t *gif_index_list_init(size_t size) {
  gif_index_list_t *list = malloc(size);

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

size_t gif_read_next_code(
  u_int16_t *output,
  unsigned char *data,
  size_t offset,
  unsigned char code_size
) {
  size_t offset_end = offset + code_size;
  int8_t bits_left = code_size;
  size_t start_byte = offset / 8;
  u_int16_t byte_value, result = 0;

  while (bits_left > 0) {
    size_t current_byte = (offset_end + 7) / 8 - 1;
    size_t current_byte_bit_offset = current_byte * 8;
    size_t current_byte_bits = offset_end - current_byte_bit_offset;
    size_t current_byte_target_offset = (current_byte > start_byte) ? 0 : (offset % 8);
    size_t shift_left = max_int8(bits_left - current_byte_bits, 0);
    byte_value = data[current_byte];

    result = result | (((byte_value & bit_mask(current_byte_bits)) >> current_byte_target_offset) << shift_left);

    // Shift back.
    bits_left -= current_byte_bits;
    offset_end -= current_byte_bits;
  }

  *output = result;
  return offset + code_size;
}

gif_decoded_image_t *gif_decode_image_block(
  gif_image_block_t *image,
  unsigned char global_color_table_size,
  gif_color_t *global_color_table,
  int *error
) {
  gif_color_t *color_table;
  unsigned char color_table_size;

  // The color table might be embedded in the image block. Otherwise use global
  // one.
  if (image->color_table != NULL && image->descriptor.color_table_size > 0) {
    color_table = image->color_table;
    color_table_size = image->descriptor.color_table_size;
  } else {
    color_table = global_color_table;
    color_table_size = global_color_table_size;
  }

  unsigned char code_size = image->minimum_code_size + 1;
  size_t bit_offset = 0;
  u_int16_t current_code = 0;
  unsigned char *sequence = NULL;
  int is_end = 0, is_reset = 0;
  unsigned char buffer[64] = { 0 };
  int max_code_count = (1 << code_size);

  gif_lzw_code_table *table = gif_lzw_code_table_init(color_table_size);
  if (table == NULL) {
    *error = GIF_ERR_MEMIO;
    return NULL;
  }

  // First code must be RESET.
  bit_offset = gif_read_next_code(&current_code, image->data, bit_offset, code_size);
  sequence = gif_lzw_code_table_element_at(table, current_code, &is_reset, &is_end);
  if (!is_reset) {
    gif_lzw_code_table_free(table);
    *error = GIF_ERR_BAD_ENCODING;
    return NULL;
  }

  // Allocate space for all pixel indexes.
  size_t total_size = image->descriptor.width * image->descriptor.height;
  gif_index_list_t *indexes = gif_index_list_init(total_size);
  if (indexes == NULL) {
    gif_lzw_code_table_free(table);
    *error = GIF_ERR_MEMIO;
    return NULL;
  }

  // Decoding begins here.

  // Read first index.
  bit_offset = gif_read_next_code(&current_code, image->data, bit_offset, code_size);
  sequence = gif_lzw_code_table_element_at(table, current_code, &is_reset, &is_end);
  gif_index_list_add_seq(indexes, sequence);
  memcpy(buffer, sequence, sequence[0] + 1);

  // Main loop.
  while (1) {
    bit_offset = gif_read_next_code(&current_code, image->data, bit_offset, code_size);
    sequence = gif_lzw_code_table_element_at(table, current_code, &is_reset, &is_end);
    if (is_end) {
      break;
    } else if (is_reset) {
      // Reset table.
      gif_lzw_code_table_free(table);
      table = gif_lzw_code_table_init(color_table_size);
    } else {
      if (sequence == NULL) {
        // New code entry: prev sequence + first element of prev sequence
        buffer[buffer[0] + 1] = buffer[1];
        buffer[0] = buffer[0] + 1;
        // Output new sequence.
        gif_index_list_add_seq(indexes, buffer);
        // Append new entry to the code table.
        gif_lzw_code_table_append_element(table, buffer, error);

        printf("code %d not found, new sequence: ", current_code);
        for (int idx = 0; idx < buffer[0] + 1; idx++) {
          printf("%d ", buffer[idx]);
        }
        printf("\n");
      } else {
        // Output current sequence to the index stream.
        gif_index_list_add_seq(indexes, sequence);
        // New code entry: previous sequence + first element in new one
        buffer[buffer[0] + 1] = sequence[1];
        buffer[0] = buffer[0] + 1;
        // Append new entry to the code table.
        gif_lzw_code_table_append_element(table, buffer, error);

        printf("code %d, sequence '", current_code);
        for (int idx = 0; idx < sequence[0] + 1; idx++) {
          printf("%d ", sequence[idx]);
        }
        printf("' found, new sequence: ");
        for (int idx = 0; idx < buffer[0] + 1; idx++) {
          printf("%d ", buffer[idx]);
        }
        printf("\n");

        // Replace buffer with current sequence.
        memcpy(buffer, sequence, sequence[0] + 1);
        sequence = NULL;
      }

      // Check if need to increase code size.
      if (table->element_count == max_code_count) {
        printf("code count %d, increasing code size to %d\n", max_code_count, code_size + 1);
        code_size += 1;
        max_code_count = (1 << code_size);
      }
    }
  }

  // TODO: Replace debug code with proper logic.
  for (int idx = 0; idx < indexes->count; idx++) {
    printf("%d ", indexes->elements[idx]);
  }
  printf("\n");
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
      gif_decoded_image_t *image = gif_decode_image_block(
        image_block,
        parsed->screen.color_table_size,
        parsed->global_color_table,
        error
      );

      if (image != NULL) {
        // TODO: Saving
      } else {
        break;
      }
    } else {
      // Ignore comment and plain text ones for now.
    }
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
