#ifndef _PNG_CHUNKS_INCLUDE
#define _PNG_CHUNKS_INCLUDE

#include <stdlib.h>

#include "errors.h"
#include "png_raw.h"

/** Data types **/

typedef struct {
  u_int32_t width;
  u_int32_t height;
  unsigned short depth;
  unsigned short color_type;
  unsigned short compression;
  unsigned short filter;
  unsigned short interlace;
} png_header_t;

typedef struct {
  u_int32_t gamma;
} png_gamma_t;

typedef struct {
  u_int32_t length;
  void *data;
} png_data_t;

typedef struct {
  unsigned char red;
  unsigned char green;
  unsigned char blue;
} png_color_index_t;

typedef struct {
  size_t length;
  png_color_index_t *entries;
} png_palette_t;

/** Transparency **/

typedef struct {
  // Yes, very inefficient use of memory. But I'm trading 262 bytes of memory
  // for one less malloc call and one less chance to forget to free it later.
  u_int16_t grayscale;
  u_int16_t red;
  u_int16_t green;
  u_int16_t blue;
  unsigned char entries[256];
} png_transparency_t;

/** Animation **/

#define APNG_DISPOSE_TYPE_NONE 0
#define APNG_DISPOSE_TYPE_BACKGROUND 1
#define APNG_DISPOSE_TYPE_PREVIOUS 2

#define APNG_BLEND_TYPE_SOURCE 0
#define APNG_BLEND TYPE_OVER 1

typedef struct {
  u_int32_t num_frames;
  u_int32_t num_plays;
} png_animation_control_t;

typedef struct {
  u_int32_t width;
  u_int32_t height;
  u_int32_t x_offset;
  u_int32_t y_offset;
  unsigned short delay_num;
  unsigned short delay_den;
  unsigned short dispose_type;
  unsigned short blend_type;
} png_frame_control_t;

/** Full parsed PNG/APNG **/

typedef struct {
  // Required data.
  png_header_t header;
  png_data_t data;
  // Additional data.
  png_gamma_t *gamma;
  png_palette_t *palette;
  png_transparency_t *transparency;
  unsigned char *sbits;
  // Animation.
  int is_data_first_frame; // Should be either 0 or 1.
  png_animation_control_t *anim_control;
  png_frame_control_t *frame_controls;
  png_data_t *frames;
} png_parsed_t;

/** Interface **/

/**
 * Frees parsed PNG struct.
 *
 * @param png PNG data struct.
 */
void png_parsed_free(png_parsed_t *png);

/**
 * Creates a parsed PNG struct out of raw PNG data.
 *
 * @param raw Raw chunk data.
 * @param error Output error.
 *
 * @return New instance of PNG or NULL in case of an error.
 */
png_parsed_t *png_parsed_from_raw(png_raw_t *raw, int *error);

/**
 * Creates a parsed PNG struct out of raw PNG data.
 *
 * @param data PNG data array.
 * @param size Size of the data array.
 * @param error Error output.
 *
 * @return Parsed PNG data struct or NULL in case of an error.
 */
png_parsed_t *png_parsed_from_data(unsigned char *data, size_t size, int *error);

/**
 * Creates a parsed PNG struct out of data from file handle.
 *
 * @param file File handle.
 * @param error Error output.
 *
 * @return Parsed PNG data struct or NULL in case of an error.
 */
png_parsed_t *png_parsed_from_file(FILE *file, int *error);

/**
 * Creates a parsed PNG struct out of data from file at given path.
 *
 * @param path File path to the PNG file.
 * @param error Error output.
 *
 * @return Parsed PNG data struct or NULL in case of an error.
 */
png_parsed_t *png_parsed_from_path(char *path, int *error);

#endif

