/**
 * Takes a PNG file, decodes it into animated_image_t, and displays
 * each frame. Requires a window system to work, since it creates a
 * window to show the final result.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <errors.h>
#include <png_raw.h>
#include <png_parser.h>
#include <png_decoder.h>
#include <image.h>

#include "image_viewer.h"

int main(int argc, char **argv) {
  int error = 0;

  if (argc < 2) {
    printf("Usage: %s [-b] <filepath>\n", argv[0]);
    return 0;
  }

  png_raw_t *raw = png_raw_from_path(argv[1], 0, &error);
  if (raw == NULL || error != 0) {
    printf("Failed to parse file: %d.\n", error);
    return 0;
  }

  png_parsed_t *parsed = png_create_from_raw(raw, &error);
  if (parsed == NULL || error != 0) {
    printf("Failed to decode PNG chunks: %d.\n", error);
    return 0;
  }

  png_t *png = png_create_from_parsed(parsed, 0, &error);
  if (parsed == NULL || error != 0) {
    printf("Failed to decode PNG data: %d.\n", error);
    return 0;
  }

  animated_image_t *image = image_from_decoded_png(png, &error);

  if (error != 0 || image == NULL) {
    printf("Image error: %d\n", error);
    return -1;
  }

  show_image(image);

  animated_image_free(image);
  png_raw_free(raw);
  png_parsed_free(parsed);
  png_free(png);

  return 0;
}
