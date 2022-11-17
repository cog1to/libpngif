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
 * Parses raw PNG data stream into a png_raw_t struct.
 *
 * @param data PNG data.
 * @param fail_on_crc Stop reading and return error of chunk's CRC is wrong.
 * @param error Error output.
 *
 * @return Raw PNG data, or NULL in case of fatal errors.
 */
png_raw_t *png_raw_from_data(unsigned char *data, size_t size, int fail_on_crc, int *error);

/**
 * Loads and splits PNG data from a file handle into png_raw_t struct.
 *
 * @param file File to read.
 * @param fail_on_crc Stop reading and return error of chunk's CRC is wrong.
 * @param error Error output.
 *
 * @return Raw PNG data, or NULL in case of fatal errors.
 */
png_raw_t *png_raw_from_file(FILE *file, int fail_on_crc, int *error);

/**
 * Loads and splits PNG data from a file into png_raw_t struct.
 *
 * @param filename Path to the file to read.
 * @param fail_on_crc Stop reading and return error of chunk's CRC is wrong.
 * @param error Error output.
 *
 * @return Parsed PNG data, or NULL in case of fatal errors.
 **/
png_raw_t *png_raw_from_path(const char *filename, int fail_on_crc, int *error);

/**
 * Frees the memory occupied by the raw data container.
 *
 * @param png Previously created png_raw_t instance.
 */
void png_raw_free(png_raw_t *png);

#endif
