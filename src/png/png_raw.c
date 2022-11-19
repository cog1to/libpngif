#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "utils.h"
#include "png_raw.h"
#include "png_util.h"

/** Private **/

png_raw_t *png_raw_create() {
  png_raw_t *png = malloc(sizeof(png_raw_t));
  if (png != NULL) {
    png->chunk_count = 0;
    png->chunks = 0;
  }

  return png;
}

void png_chunk_free(png_chunk_raw_t *chunk) {
  if (chunk->length > 0 && chunk->data != NULL) {
    free(chunk->data);
  }

  free(chunk);
}

png_chunk_raw_t *read_chunk(
  unsigned char *data,
  u_int32_t size,
  u_int32_t offset,
  int fail_on_crc,
  u_int32_t *new_offset,
  int *error
) {
  uint32_t length = 0, crc = 0;
  char type[4] = { 0 };
  unsigned char *body = NULL;
  unsigned char *type_with_body = NULL;

  // Length.
  memcpy(&length, data + offset, 4);
  length = __builtin_bswap32(length);

  type_with_body = malloc(length + 4);
  if (type_with_body == NULL) {
    *error = PNG_ERR_MEMIO;
    return NULL;
  }

  // Read type + body to a single array. We do this because CRC checksum is
  // calculated for [type+data] instead of just [data].
  memcpy(type_with_body, data + offset + 4, length + 4);

  // Type.
  memcpy(type, type_with_body, 4);

  // Body.
  if (length > 0) {
    body = malloc(length);
    if (body == NULL) {
      *error = PNG_ERR_MEMIO;
      return NULL;
    }

    memcpy(body, type_with_body + 4, length);
  }

  // CRC.
  memcpy(&crc, data + offset + length + 8, 4);
  crc = __builtin_bswap32(crc);

  // Update offset.
  *new_offset = offset + length + 12;

  // Verify the CRC checksum.
  if (fail_on_crc) {
    uint32_t calc_crc = u_crc32(type_with_body, length + 4);
    if (crc != calc_crc) {
      *error = PNG_ERR_CRC;
      free(body);
      return NULL;
    }
  }

  // No need for raw data array below this point.
  free(type_with_body);

  // Make chunk.
  png_chunk_raw_t *chunk = malloc(sizeof(png_chunk_raw_t));
  if (chunk == NULL) {
    *error = PNG_ERR_FILEIO;
    free(body);
    return NULL;
  }

  chunk->length = length;
  chunk->crc = crc;
  chunk->data = body;
  memcpy(chunk->type, type, 4);
  return chunk;
}

int append_chunk(png_raw_t *png, png_chunk_raw_t *chunk) {
  if (png == NULL || chunk == NULL) {
    return 1;
  }

  png->chunks = realloc(png->chunks, sizeof(void *) * (png->chunk_count + 1));
  if (png->chunks == NULL) {
    return 1;
  }

  png->chunks[png->chunk_count] = chunk;
  png->chunk_count += 1;
  return 0;
}

/** Public **/

void png_raw_free(png_raw_t *png) {
  if (png == NULL) {
    return;
  }

  for (int idx = 0; idx < png->chunk_count; idx++) {
    png_chunk_free(png->chunks[idx]);
  }

  free(png);
}

png_raw_t *png_raw_from_data(unsigned char *data, size_t size, int fail_on_crc, int *error) {
  char header[9] = { 0 };
  memcpy(header, data, 8);

  // Verify header.
  if (strcmp(PNG_HEADER, header) != 0) {
    *error = PNG_ERR_WRONG_HEADER;
    return NULL;
  }

  // Create container.
  png_raw_t *png = png_raw_create();
  if (png == NULL) {
    *error = PNG_ERR_MEMIO;
    return NULL;
  }

  // Read chunks.
  int parse_error = 0;
  png_chunk_raw_t *chunk = NULL;
  u_int32_t offset = 8;
  while ((chunk = read_chunk(data, size, offset, fail_on_crc, &offset, &parse_error)) != NULL) {
    if (parse_error != 0) {
      break;
    }

    parse_error = append_chunk(png, chunk);
    if (parse_error != 0) {
      break;
    }

    // End the parsing if we got final chunk.
    if (strcmp("IEND", chunk->type) == 0) {
      break;
    }
  }

  // Check for error.
  if (parse_error != 0) {
    *error = PNG_ERR_MEMIO;
  }

  // Clean up and return.
  return png;
}

png_raw_t *png_raw_from_file(FILE *file, int fail_on_crc, int *error) {
  unsigned char *data = NULL;

  size_t size = pngif_read_file(file, &data, error);
  if (*error != 0) {
    return NULL;
  }

  png_raw_t *raw = png_raw_from_data(data, size, fail_on_crc, error);
  free(data);
  return raw;
}

png_raw_t *png_raw_from_path(const char *filename, int fail_on_crc, int *error) {
  FILE *file = fopen(filename, "r");
  if (file == NULL) {
    *error = PNG_ERR_FILEIO;
    return 0;
  }

  png_raw_t *raw = png_raw_from_file(file, fail_on_crc, error);
  fclose(file);
  return raw;
}

