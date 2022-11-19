#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pngif/errors.h>

size_t pngif_read_file(FILE *file, unsigned char **output, int *error) {
  // Get the size.
  fseek(file, 0, SEEK_END);
  size_t size = ftell(file);
  fseek(file, 0, SEEK_SET);

  if (size <= 0) {
    *error = GIF_ERR_FILEIO_NO_SIZE;
    return 0;
  }

  // Allocate buffer memory.
  unsigned char *data = malloc(size);
  if (data == NULL) {
    *error = GIF_ERR_MEMIO;
    return 0;
  }

  // Read the file into the buffer.
  size_t read = fread(data, 1, size, file);
  if (read != size) {
    *error = GIF_ERR_FILEIO_READ_ERROR;
    free(data);
    return 0;
  }

  *output = data;
  return read;
}

void rgba_to_argb(unsigned char *rgba, unsigned char *argb, size_t width, size_t height) {
  int byte_count = width * height * 4;
  unsigned char pixel[4];

  // We use temp buffer to allow input and output to be a pointer to the same
  // array. It's not as clean looking, but it's more handy in case you don't
  // want to make the conversion inline without spawning more memory blocks.
  for (int idx = 0; idx < byte_count; idx += 4) {
    // Save current pixel.
    pixel[0] = rgba[idx + 3];
    pixel[1] = rgba[idx + 0];
    pixel[2] = rgba[idx + 1];
    pixel[3] = rgba[idx + 2];
    // Write it to output array.
    memcpy(argb + idx, pixel, 4);
  }
}

void rgba_to_bgra(unsigned char *src, unsigned char *dest, size_t width, size_t height) {
  int byte_count = width * height * 4;
  unsigned char pixel[4];

  // We use temp buffer to allow input and output to be a pointer to the same
  // array. It's not as clean looking, but it's more handy in case you don't
  // want to make the conversion inline without spawning more memory blocks.
  for (int idx = 0; idx < byte_count; idx += 4) {
    // Save current pixel.
    pixel[0] = src[idx + 2];
    pixel[1] = src[idx + 1];
    pixel[2] = src[idx + 0];
    pixel[3] = src[idx + 3];
    // Write it to output array.
    memcpy(dest + idx, pixel, 4);
  }
}

void print_binary(unsigned char x) {
  static char b[9];
  b[0] = '\0';

  int z;
  for (z = 128; z > 0; z >>= 1) {
    strcat(b, ((x & z) == z) ? "1" : "0");
  }
  printf("%s", b);
}

