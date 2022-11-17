#ifndef _PNG_DECODER_INCLUDE
#define _PNG_DECODER_INCLUDE

#include "errors.h"
#include "png_parser.h"

/** Data types **/

typedef struct {
  u_int32_t width;
  u_int32_t height;
  u_int32_t x_offset;
  u_int32_t y_offset;
  unsigned short dispose_type;
  unsigned short blend_type;
  float delay;
  unsigned char *data;
} png_frame_t;

typedef struct {
  u_int32_t plays;
  u_int32_t length;
  png_frame_t *frames;
} png_frame_list_t;

typedef struct {
  // Dimensions.
  u_int32_t width;
  u_int32_t height;

  // Image data.
  unsigned char *data;

  // Additional data.
  png_frame_list_t *frames;
} png_t;

/** Interface **/

/**
 * Frees the memory occupied by PNG image.
 *
 * @param png Image to free.
 */
void png_free(png_t *png);

/**
 * Creates a PNG image from a parsed PNG data.
 *
 * @param parsed Parsed PNG data.
 * @param error Error output.
 *
 * @return Decoded PNG image, or NULL in case an error occurred.
 */
png_t *png_decoded_from_parsed(png_parsed_t *parsed, int *error);

/**
 * Creates a parsed PNG struct out of raw PNG data.
 *
 * @param data PNG data array.
 * @param size Size of the data array.
 * @param error Error output.
 *
 * @return Decoded PNG data struct or NULL in case of an error.
 */
png_t *png_decoded_from_data(unsigned char *data, size_t size, int *error);

/**
 * Creates a parsed PNG struct out of data from file handle.
 *
 * @param file File handle.
 * @param error Error output.
 *
 * @return Decoded PNG data struct or NULL in case of an error.
 */
png_t *png_decoded_from_file(FILE *file, int *error);

/**
 * Creates a decoded PNG struct out of data from file at given path.
 *
 * @param path File path to the PNG file.
 * @param error Error output.
 *
 * @return Decoded PNG data struct or NULL in case of an error.
 */
png_t *png_decoded_from_path(char *path, int *error);

#endif
