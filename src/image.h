#ifndef IMAGE_FRAME_HEADER
#define IMAGE_FRAME_HEADER

#include "gif_decoded.h"

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

/**
 * Creates animated image from decoded GIF data.
 *
 * @param gif Decoded gif data.
 *
 * @return Animated image data or NULL in case of any errors.
 */
animated_image_t *image_from_decoded_gif(gif_decoded_t *gif, int *error);

/**
 * Frees the memory allocated for animated image data.
 *
 * @param image Image to deallocate.
 */
void animated_image_free(animated_image_t *image);

#endif
