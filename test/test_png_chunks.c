#include <stdlib.h>
#include <stdio.h>

#include "png_raw.h"

int main(int argc, char **argv) {
  if (argc < 2) {
    printf("Usage: %s <filename.png>\n", argv[0]);
    return 0;
  }

  int error = 0;
  png_raw_t *png = png_raw_read_file(argv[1], 0, &error);
  if (png == NULL || error != 0) {
    printf("Failed to parse file: %d.\n", error);
    return 0;
  }

  for (int idx = 0; idx < png->chunk_count; idx++) {
    printf("Chunk %d:\n", idx);
    png_chunk_raw_t *chunk = png->chunks[idx];
    printf("  Length: %d\n", chunk->length);
    printf("  Type: %c%c%c%c\n", chunk->type[0], chunk->type[1], chunk->type[2], chunk->type[3]);
    printf("  CRC: 0x%08x\n", chunk->crc);
  }

  png_raw_free(png);
  return 1;
}
