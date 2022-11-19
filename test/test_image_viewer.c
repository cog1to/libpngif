/**
 * Takes a GIF or PNG file, decodes it into animated_image_t, and displays
 * each frame. Requires a window system to work, since it creates a
 * window to show the final result.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <errors.h>
#include "image.h"
#include "image_viewer.h"

int main(int argc, char **argv) {
  int error = 0;

  if (argc < 2) {
    printf("Usage: %s <filepath>\n", argv[0]);
    return 0;
  }

  animated_image_t *image = image_from_path(argv[1], 1, &error);

  if (error != 0 || image == NULL) {
    printf("Decoding error: %d\n", error);
    return -1;
  }

  show_image(image);
  animated_image_free(image);

  return 0;
}

