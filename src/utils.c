#include <stdio.h>
#include <stdlib.h>

#include "errors.h"

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

