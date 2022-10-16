/**
 * Takes a GIF file, decodes it into animated_image_t, and displays
 * each frame. Requires a window system to work, since it creates a
 * window to show the final result.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <errors.h>
#include <gif_parsed.h>
#include <gif_decoded.h>
#include <image.h>

#include "image_viewer.h"

void show_image(animated_image_t *gif);

int main(int argc, char **argv) {
  if (argc < 2) {
    printf("Usage: %s [-b] <filepath>\n", argv[0]);
    printf("Options:\n  -b: Ignore background color\n");
    return 0;
  }

  int ignore_background = 0;
  if (argc > 2) {
    if (strcmp(argv[1], "-b") == 0) {
      ignore_background = 1;
    } else {
      printf("Unknown option: %s", argv[1]);
    }
  }

  int error = 0;
  gif_parsed_t *raw = gif_parsed_from_file(argv[argc - 1], &error);

  if (error != 0 || raw == NULL) {
    printf("File read error: %d\n", error);
    return -1;
  }

  gif_decoded_t *dec = gif_decoded_from_parsed(raw, &error);

  if (error != 0 || dec == NULL) {
    printf("Decoding error: %d\n", error);
    return -1;
  }

  animated_image_t *image = image_from_decoded_gif(dec, ignore_background, &error);

  if (error != 0 || image == NULL) {
    printf("Image drawing error: %d\n", error);
    return -1;
  }

  show_image(image);

  gif_parsed_free(raw);
  gif_decoded_free(dec);
  animated_image_free(image);

  return 0;
}

/** Private **/

void show_image(animated_image_t *image) {
#ifdef OS_MAC
  show_image_mac(image);
#else
  show_image_linux(image);
#endif
}

