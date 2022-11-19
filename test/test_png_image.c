/**
 * Takes a PNG file, decodes it into animated_image_t, and displays
 * each frame. Requires a window system to work, since it creates a
 * window to show the final result.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <errors.h>
#include <png_decoded.h>
#include <image.h>

#include "image_viewer.h"

int main(int argc, char **argv) {
  int error = 0;

  if (argc < 2) {
    printf("Usage: %s [-b] <filepath>\n", argv[0]);
    return 0;
  }

  png_decoded_t *png = png_decoded_from_path(argv[1], &error);
  if (png == NULL || error != 0) {
    printf("Failed to decode PNG data: %d.\n", error);
    return 1;
  }

  animated_image_t *image = image_from_decoded_png(png, &error);

  if (error != 0 || image == NULL) {
    printf("Image error: %d\n", error);
    return 1;
  }

  show_image(image);

  animated_image_free(image);
  png_decoded_free(png);

  return 0;
}
