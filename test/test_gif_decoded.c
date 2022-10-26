/**
 * Takes a GIF file, decodes it into gif_decoded_t, and displays
 * each frame. Requires a window system to work, since it creates a
 * window to show the final result.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <errors.h>
#include <gif_parsed.h>
#include <gif_decoded.h>

#include "image_viewer.h"

int main(int argc, char **argv) {
  int error = 0;

  if (argc < 2) {
    printf("Usage: %s <filepath>\n", argv[0]);
    return 0;
  }

  gif_decoded_t *dec = gif_decoded_from_path(argv[1], &error);

  if (dec == NULL) {
    printf("No dec\n");
  }

  if (error != 0 || dec == NULL) {
    printf("Decoding error: %d\n", error);
    return -1;
  }

  show_decoded_gif(dec);
  gif_decoded_free(dec);

  return 0;
}

