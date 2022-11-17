#ifndef _PNG_RAW_INCLUDE
#define _PNG_RAW_INCLUDE

#include "errors.h"

typedef u_int32_t uint32_t;

/** Raw parsing - data types **/

/**
 * Raw PNG chunk.
 */
typedef struct {
  char type[4];
  uint32_t length;
  uint32_t crc;
  unsigned char *data;
} png_chunk_raw_t;

/**
 * Raw PNG chunks container.
 */
typedef struct {
  unsigned int chunk_count;
  png_chunk_raw_t **chunks;
} png_raw_t;

/** Raw parsing - API **/

/**
 * Reads and parses a PNG file into a raw chunks list.
 *
 * @param path Path to a file to read.
 * @param fail_on_crc Flag indicating whether parsing should stop when
 *   CRC mismatch is encountered for one of the chunks.
 * @param error Error output.
 *
 * @return A pointer to parsed data container, or NULL in case an error
 *   occured.
 */
png_raw_t *png_raw_read_file(const char *path, int fail_on_crc, int *error);

/**
 * Frees the memory occupied by the raw data container.
 *
 * @param png Previously created png_raw_t instance.
 */
void png_raw_free(png_raw_t *png);

#endif
