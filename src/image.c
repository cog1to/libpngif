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

void gif_draw_subimage(
  unsigned char *rgba,
  gif_decoded_image_t *image,
  u_int32_t width, u_int32_t height
);

void gif_draw_frame(
  image_frame_t *frame,
  unsigned char *canvas,
  u_int32_t width,
  u_int32_t height,
  gif_color_t *background_color,
  gif_decoded_image_t *image,
  int ignore_background,
  int *error
);

void png_draw_subimage(
  unsigned char *rgba,
  unsigned char *data,
  u_int32_t width, u_int32_t height,
  u_int32_t x_offset, u_int32_t y_offset,
  u_int32_t sub_width, u_int32_t sub_height,
  unsigned short blend_type
);

void png_draw_frame(
  image_frame_t *frame,
  unsigned char *canvas,
  u_int32_t width,
  u_int32_t height,
  png_frame_t *image,
  int *error
);

/** Public **/

animated_image_t *image_from_decoded_gif(gif_decoded_t *gif, int ignore_background, int *error) {
  if (gif == NULL)
    return NULL;

  animated_image_t *output = malloc(sizeof(animated_image_t));
  if (output == NULL) {
    *error = GIF_ERR_MEMIO;
    return NULL;
  }

  unsigned char *canvas = calloc(gif->width * gif->height * 4, 1);
  if (canvas == NULL) {
    free(output);
    *error = GIF_ERR_MEMIO;
    return NULL;
  }

  if (!ignore_background && gif->background_color != NULL) {
    unsigned char back[] = {
      gif->background_color->red,
      gif->background_color->green,
      gif->background_color->blue,
      255
    };

    for (int idx = 0; idx < (gif->width * gif->height * 4); idx += 4) {
      memcpy(canvas + idx, back, 4);
    }
  }

  if (gif->animated) {
    output->frames = malloc(sizeof(image_frame_t) * gif->image_count);
    if (output->frames == NULL) {
      *error = GIF_ERR_MEMIO;
      free(output);
      return NULL;
    }

    for (int idx = 0; idx < gif->image_count; idx++) {
      gif_draw_frame(
        output->frames + idx,
        canvas,
        gif->width,
        gif->height,
        gif->background_color,
        gif->images + idx,
        ignore_background,
        error
      );

      if (*error != 0)
        break;
    }

    output->frame_count = gif->image_count;
    free(canvas);
  } else {
    output->frames = malloc(sizeof(image_frame_t));
    if (output->frames == NULL) {
      *error = GIF_ERR_MEMIO;
      free(output);
      return NULL;
    }

    for (int idx = 0; idx < gif->image_count; idx++) {
      gif_decoded_image_t *image = gif->images + idx;
      gif_draw_subimage(canvas, image, gif->width, gif->height);
    }

    output->frame_count = 1;
    output->frames->rgba = canvas;
    output->frames->duration_ms = 0;
  }

  output->width = gif->width;
  output->height = gif->height;
  return output;
}

animated_image_t *image_from_decoded_png(png_t *png, int *error) {
  if (png == NULL)
    return NULL;

  animated_image_t *output = malloc(sizeof(animated_image_t));
  if (output == NULL) {
    *error = PNG_ERR_MEMIO;
    return NULL;
  }

  unsigned char *canvas = calloc(png->width * png->height * 4, 1);
  if (canvas == NULL) {
    free(output);
    *error = PNG_ERR_MEMIO;
    return NULL;
  }

  // TODO: Background color?
  if (png->frames != NULL && png->frames->length > 0) {
    output->frames = malloc(sizeof(image_frame_t) * png->frames->length);
    if (output->frames == NULL) {
      *error = PNG_ERR_MEMIO;
      free(output);
      return NULL;
    }

    for (int idx = 0; idx < png->frames->length; idx++) {
      png_draw_frame(
        output->frames + idx,
        canvas,
        png->width,
        png->height,
        png->frames->frames + idx,
        error
      );

      if (*error != 0)
        break;
    }

    output->frame_count = png->frames->length;
    free(canvas);
  } else {
    output->frames = malloc(sizeof(image_frame_t));
    if (output->frames == NULL) {
      *error = PNG_ERR_MEMIO;
      free(output);
      return NULL;
    }

    png_draw_subimage(
      canvas,
      png->data,
      png->width, png->height,
      0, 0,
      png->width, png->height,
      APNG_BLEND_TYPE_SOURCE
    );

    output->frame_count = 1;
    output->frames->rgba = canvas;
    output->frames->duration_ms = 0;
  }

  output->width = png->width;
  output->height = png->height;
  return output;
}

/** Convenience API **/

animated_image_t *image_from_gif_data(
  unsigned char *data,
  size_t size,
  int ignore_background,
  int *error
) {
  gif_decoded_t *decoded = gif_decoded_from_data(data, size, error);
  if (*error != 0 || decoded == NULL) {
    return NULL;
  }

  animated_image_t *image = image_from_decoded_gif(decoded, ignore_background, error);
  free(decoded);
  return image;
}

animated_image_t *image_from_gif_file(FILE *file, int ignore_background, int *error) {
  gif_decoded_t *decoded = gif_decoded_from_file(file, error);
  if (*error != 0 || decoded == NULL) {
    return NULL;
  }

  animated_image_t *image = image_from_decoded_gif(decoded, ignore_background, error);
  free(decoded);
  return image;
}

animated_image_t *image_from_gif_path(char *path, int ignore_background, int *error) {
  gif_decoded_t *decoded = gif_decoded_from_path(path, error);
  if (*error != 0 || decoded == NULL) {
    return NULL;
  }

  animated_image_t *image = image_from_decoded_gif(decoded, ignore_background, error);
  free(decoded);
  return image;
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

/**
 * Frees the memory occupied by an image frame.
 *
 * @param frame Image frame data to deallocate.
 */
void image_frame_free(image_frame_t *frame) {
  if (frame == NULL)
    return;

  if (frame->rgba != NULL)
    free(frame->rgba);

  free(frame);
}

/**
 * Draws a decoded image block into overall image "canvas".
 *
 * @param rgba Full image canvas container. It has to be at least as large as
 *   the image being drawn into it.
 * @param image Image data to draw into canvas.
 * @param width Width of the canvas.
 * @param height Height of the canvas.
 */
void gif_draw_subimage(
  unsigned char *rgba,
  gif_decoded_image_t *image,
  u_int32_t width,
  u_int32_t height
) {
  for (int line = 0; line < image->height; line++) {
    for (int pixel = 0; pixel < image->width; pixel++) {
      unsigned char *colors = image->rgba + (image->width * line + pixel) * 4;
      // Ignore transparent pixel. Overwrite every other pixel.
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

/**
 * Draws a decoded image block as an image frame. Takes into account the
 * previous canvas state and frame's disposal method to update canvas state
 * for next frame drawing.
 *
 * @param frame Target frame container. It will hold the drawn frame data.
 * @param canvas Image canvas in a state before drawing the target frame.
 * @param width Canvas width.
 * @param height Canvas height.
 * @param background_color Background color value (from Logical Screen
 *   Descriptor).
 * @param image Image block to draw into the frame.
 * @param ignore_background Flag indicating whether we should ignore provided
 *   background color value (for better compliance with modern browser
 *   rendering).
 * @param error Return error value.
 */
void gif_draw_frame(
  image_frame_t *frame,
  unsigned char *canvas,
  u_int32_t width,
  u_int32_t height,
  gif_color_t *background_color,
  gif_decoded_image_t *image,
  int ignore_background,
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
    if (!ignore_background && background_color != NULL) {
      for (int pixel = 0; pixel < (width * height); pixel++) {
        canvas[pixel * 4 + 0] = background_color->red;
        canvas[pixel * 4 + 1] = background_color->green;
        canvas[pixel * 4 + 2] = background_color->blue;
        canvas[pixel * 4 + 3] = 255;
      }
    } else {
      memset(canvas, 0, (width * height * 4));
    }
    break;
  case DISPOSE_RESTORE:
    // Canvas is left at previous state.
    break;
  }
}

void png_draw_subimage(
  unsigned char *rgba,
  unsigned char *data,
  u_int32_t width, u_int32_t height,
  u_int32_t x_offset, u_int32_t y_offset,
  u_int32_t sub_width, u_int32_t sub_height,
  unsigned short blend_type
) {
  for (int line = 0; line < sub_height; line++) {
    for (int pixel = 0; pixel < sub_width; pixel++) {
      unsigned char *colors = data + (sub_width * line + pixel) * 4;
      if (blend_type == APNG_BLEND_TYPE_SOURCE) {
          memcpy(
            rgba + (width * (y_offset + line) * 4) + (x_offset + pixel) * 4,
            colors,
            4
          );
      } else {
        if (colors[3] == 0) {
          continue;
        } else if (colors[3] == 255) {
          memcpy(
            rgba + (width * (y_offset + line) * 4) + (x_offset + pixel) * 4,
            colors,
            4
          );
        } else {
          // TODO: Seems like a good place for some SIMD commands.
          float source_alpha = (float)(colors[3]) / (float)255;
          float comp_alpha = 1.0 - source_alpha;
          u_int32_t offset = (width * (y_offset + line) * 4) + (x_offset + pixel) * 4;
          for (u_int32_t idx = 0; idx < 3; idx++, offset++) {
            *(rgba + offset) = ((float)colors[idx] * source_alpha)
              + ((float)rgba[offset] * comp_alpha);
          }
        }
      }
    }
  }
}

/**
 * Draws a decoded image block as an image frame. Takes into account the
 * previous canvas state and frame's disposal method to update canvas state
 * for next frame drawing.
 *
 * @param frame Target frame container. It will hold the drawn frame data.
 * @param canvas Image canvas in a state before drawing the target frame.
 * @param width Canvas width.
 * @param height Canvas height.
 * @param image Image block to draw into the frame.
 * @param error Return error value.
 */
void png_draw_frame(
  image_frame_t *frame,
  unsigned char *canvas,
  u_int32_t width,
  u_int32_t height,
  png_frame_t *png,
  int *error
) {
  unsigned char *rgba = malloc(width * height * 4);
  if (rgba == NULL) {
    *error = PNG_ERR_MEMIO;
    return;
  }

  // Paint previous state into frame.
  memcpy(rgba, canvas, width * height * 4);

  // Paint image into frame.
  png_draw_subimage(
    rgba,
    png->data,
    width, height,
    png->x_offset, png->y_offset,
    png->width, png->height,
    png->blend_type
  );

  frame->rgba = rgba;
  frame->duration_ms = png->delay * 1000;

  switch (png->dispose_type) {
  case APNG_DISPOSE_TYPE_NONE:
    // Canvas is set to the current frame.
    memcpy(canvas, rgba, width * height * 4);
    break;
  case APNG_DISPOSE_TYPE_BACKGROUND:
    // Canvas is set to background color.
    memset(canvas, 0, (width * height * 4));
    break;
  case APNG_DISPOSE_TYPE_PREVIOUS:
    // Canvas is left at previous state.
    break;
  }
}

