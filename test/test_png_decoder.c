#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "png_util.h"
#include "png_raw.h"
#include "png_parser.h"
#include "png_decoder.h"
#include "image_viewer.h"

int main(int argc, char **argv) {
  if (argc < 2) {
    printf("Usage: %s <filename.png>\n", argv[0]);
    return 0;
  }

  int error = 0;
  png_raw_t *raw = png_raw_read_file(argv[1], 0, &error);
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

  show_decoded_png(png);

  png_raw_free(raw);
  png_parsed_free(parsed);
  png_free(png);
}

