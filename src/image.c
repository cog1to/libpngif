#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "errors.h"
#include "image.h"

/** Private declarations **/

#define DISPOSE_APPEND 1
#define DISPOSE_BACKGROUND 2
#define DISPOSE_RESTORE 3

void image_frame_free(image_frame_t *frame);
void gif_draw_subimage(unsigned char *rgba, gif_decoded_image_t *image, u_int32_t width, u_int32_t height);
void gif_draw_frame(image_frame_t *frame, unsigned char *canvas, u_int32_t width, u_int32_t height, gif_color_t *background_color, gif_decoded_image_t *image, int *error);

/** Public **/

animated_image_t *image_from_decoded_gif(gif_decoded_t *gif, int *error) {
  if (gif == NULL)
    return NULL;

  animated_image_t *output = malloc(sizeof(animated_image_t));
  if (output == NULL) {
    *error = GIF_ERR_MEMIO;
    return NULL;
  }

  if (gif->animated) {
    output->frames = malloc(sizeof(image_frame_t) * gif->image_count);
    if (output->frames == NULL) {
      *error = GIF_ERR_MEMIO;
      free(output);
      return NULL;
    }

    unsigned char *canvas = calloc(gif->width * gif->height, 4);

    for (int idx = 0; idx < gif->image_count; idx++) {
      gif_draw_frame(
        output->frames + idx,
        canvas,
        gif->width,
        gif->height,
        gif->background_color,
        gif->images + idx,
        error
      );

      if (*error != 0)
        break;
    }

    output->frame_count = gif->image_count;
  } else {
    output->frames = malloc(sizeof(image_frame_t));
    if (output->frames == NULL) {
      *error = GIF_ERR_MEMIO;
      free(output);
      return NULL;
    }

    unsigned char *rgba = malloc(gif->width * gif->height * 4);
    for (int idx = 0; idx < gif->image_count; idx++) {
      gif_decoded_image_t *image = gif->images + idx;
      gif_draw_subimage(rgba, image, gif->width, gif->height);
    }

    output->frame_count = 1;
    output->frames->rgba = rgba;
    output->frames->duration_ms = 0;
  }

  output->width = gif->width;
  output->height = gif->height;
  return output;
}

void animated_image_free(animated_image_t *image) {
  if (image == NULL)
    return;

  if (image->frames != NULL && image->frame_count > 0) {
    for (int idx = 0; idx < image->frame_count; idx++) {
      image_frame_free(image->frames + idx);
    }
  }

  free(image);
}

/** Private **/

void image_frame_free(image_frame_t *frame) {
  if (frame == NULL)
    return;

  if (frame->rgba != NULL)
    free(frame->rgba);

  free(frame);
}

void gif_draw_subimage(
  unsigned char *rgba,
  gif_decoded_image_t *image,
  u_int32_t width,
  u_int32_t height
) {
  for (int line = 0; line < image->height; line++) {
    for (int pixel = 0; pixel < image->width; pixel++) {
      unsigned char *colors = image->rgba + (image->width * line + pixel) * 4;
      if (colors[3] != 0) {
        memcpy(
          rgba + (width * (image->top + line) * 4) + (image->left + pixel) * 4,
          colors,
          4
        );
      }
    }
  }
}

void gif_draw_frame(
  image_frame_t *frame,
  unsigned char *canvas,
  u_int32_t width,
  u_int32_t height,
  gif_color_t *background_color,
  gif_decoded_image_t *image,
  int *error
) {
  unsigned char *rgba = malloc(width * height * 4);
  if (rgba == NULL) {
    *error = GIF_ERR_MEMIO;
    return;
  }

  // Paint previous state into frame.
  memcpy(rgba, canvas, width * height * 4);

  // Paint image into frame.
  gif_draw_subimage(rgba, image, width, height);

  frame->rgba = rgba;
  frame->duration_ms = image->delay_cs * 10;

  switch (image->dispose_method) {
  case DISPOSE_APPEND:
    // Canvas is set to the current frame.
    memcpy(canvas, rgba, width * height * 4);
    break;
  case DISPOSE_BACKGROUND:
    // Canvas is set to background color.
    if (background_color != NULL) {
      for (int pixel = 0; pixel < (width * height); pixel++) {
        canvas[pixel * 4 + 0] = background_color->red;
        canvas[pixel * 4 + 1] = background_color->green;
        canvas[pixel * 4 + 2] = background_color->blue;
        canvas[pixel * 4 + 3] = 255;
      }
    }
    break;
  case DISPOSE_RESTORE:
    // Canvas is left at previous state.
    break;
  }
}
