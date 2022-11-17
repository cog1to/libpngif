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
 * @param convert_to_argb Flag indicating whether data should be converted from
 *   RGBA to ARGB format.
 * @param error Error output.
 *
 * @return Decoded PNG image, or NULL in case an error occurred.
 */
png_t *png_create_from_parsed(png_parsed_t *parsed, int convert_to_argb, int *error);

#endif
