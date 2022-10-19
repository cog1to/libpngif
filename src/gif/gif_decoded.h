#ifndef GIF_DECODED_HEADER
#define GIF_DECODED_HEADER

#include <stdlib.h>

#include "gif_parsed.h"

/** Data types **/

typedef struct {
  // Dimensions.
  u_int32_t top;
  u_int32_t left;
  u_int32_t width;
  u_int32_t height;

  // Frame settings
  u_int8_t dispose_method;
  u_int32_t delay_cs;

  // Image data.
  unsigned char *rgba;
} gif_decoded_image_t;

typedef struct {
  // Dimensions.
  u_int32_t width;
  u_int32_t height;

  // Optional data.
  gif_color_t *background_color;
  u_int32_t pixel_ratio;

  // Animation settings.
  unsigned char animated;
  u_int32_t repeat_count;

  // Sub-images.
  size_t image_count;
  gif_decoded_image_t *images;
} gif_decoded_t;

/** Interface **/

/**
 * Decodes parsed GIF data into a gif_decoded_t container.
 *
 * @param parsed Parsed GIF data.
 * @param error Error output.
 *
 * @return Decoded GIF data, or NULL in case of errors.
 */
gif_decoded_t *gif_decoded_from_parsed(gif_parsed_t *parsed, int *error);

/**
 * Decodes given data into a gif_decoded_t struct.
 *
 * @param data GIF data to decode.
 * @param size Data size.
 * @param error Error output.
 *
 * @return Decoded GIF data, or NULL in case of errors.
 */
gif_decoded_t *gif_decoded_from_data(unsigned char *data, size_t size, int *error);

/**
 * Reads and decodes given file into a gif_decoded_t struct.
 *
 * @param file File handle to GIF file.
 * @param error Error output.
 *
 * @return Decoded GIF data, or NULL in case of errors.
 */
gif_decoded_t *gif_decoded_from_file(FILE *file, int *error);

/**
 * Reads and decodes a file at given path into a gif_decoded_t struct.
 *
 * @param path Path to the GIF file.
 * @param error Error output.
 *
 * @return Decoded GIF data, or NULL in case of errors.
 */
gif_decoded_t *gif_decoded_from_path(char *path, int *error);

/**
 * Frees memory occupied by a decoded GIF data struct.
 *
 * @param gif Decoded gif data.
 */
void gif_decoded_free(gif_decoded_t *gif);

#endif
