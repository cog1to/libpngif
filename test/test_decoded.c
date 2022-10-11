#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <errors.h>
#include <gif_parsed.h>
#include <gif_decoded.h>

int main(int argc, char **argv) {
  if (argc < 2) {
    printf("Usage: %s <filepath>\n", argv[0]);
    return 0;
  }

  int error = 0;
  gif_parsed_t *raw = gif_parsed_from_file(argv[1], &error);

  if (error != 0 || raw == NULL) {
    printf("File read error: %d\n", error);
    return -1;
  }

  gif_decoded_t *dec = gif_decoded_from_parsed(raw, &error);

  if (error != 0 || dec == NULL) {
    printf("Decoding error: %d\n", error);
    return -1;
  }

  return 0;
}

