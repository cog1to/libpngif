#ifndef IMAGE_FRAME_HEADER
#define IMAGE_FRAME_HEADER

#include <pngif/gif_decoded.h>
#include <pngif/png_decoded.h>

/** Data types **/

typedef struct {
  unsigned char *rgba;
  u_int32_t duration_ms;
} image_frame_t;

typedef struct {
  // Image size.
  u_int32_t width;
  u_int32_t height;

  // Animation data.
  u_int32_t repeat_count;

  // Frames.
  size_t frame_count;
  image_frame_t *frames;
} animated_image_t;

/** Interface **/

/**
 * Disclaimer to all methods:
 *
 * `ignore_background` flag will make the decoder behave more like a browser:
 * 1. first frame is not initialized with background color.
 * 2. when encountering Background Fill disposal in a frame, it clears the
 * canvas with full black transparent color instead of a background color.
 * This goes kind of against the specification in GIF87 standard, but it's
 * how modern browsers treat it.
 */

/**
 * Creates animated image from decoded GIF data.
 *
 * @param gif Decoded gif data.
 * @param ignore_background Don't use "background color index" values from the
 *   Logical Screen Description.
 * @param error Return error value.
 *
 * @return Animated image data or NULL in case of any errors.
 */
animated_image_t *image_from_decoded_gif(
  gif_decoded_t *gif,
  int ignore_background,
  int *error
);

/**
 * Creates animated image from decoded PNG data.
 *
 * @param gif Decoded PNG data.
 * @param error Return error value.
 *
 * @return Animated image data or NULL in case of any errors.
 */
animated_image_t *image_from_decoded_png(png_decoded_t *png, int *error);

/**
 * Creates animated image from raw GIF or PNG data.
 *
 * @param data GIF/PNG data array.
 * @param size Data size.
 * @param ignore_background Don't use "background color index" values from the
 *   Logical Screen Descriptor in GIF.
 * @param error Return error value.
 *
 * @return Animated image data or NULL in case of any errors.
 */
animated_image_t *image_from_data(
  unsigned char *data,
  size_t size,
  int ignore_background,
  int *error
);

/**
 * Creates animated image from data read from given file handle.
 *
 * @param file File handle to GIF or PNG file.
 * @param ignore_background Don't use "background color index" values from the
 *   Logical Screen Descriptor in GIF file.
 * @param error Return error value.
 *
 * @return Animated image data or NULL in case of any errors.
 */
animated_image_t *image_from_file(FILE *file, int ignore_background, int *error);

/**
 * Creates animated image from file at given path.
 *
 * @param path Path to GIF or PNG file.
 * @param ignore_background Don't use "background color index" values from the
 *   Logical Screen Descriptor in GIF file.
 * @param error Return error value.
 *
 * @return Animated image data or NULL in case of any errors.
 */
animated_image_t *image_from_path(char *path, int ignore_background, int *error);

/**
 * Frees the memory allocated for animated image data.
 *
 * @param image Image to deallocate.
 */
void animated_image_free(animated_image_t *image);

#endif
