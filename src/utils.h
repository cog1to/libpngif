#ifndef PNGIF_UTILS_INCLUDE
#define PNGIF_UTILS_INCLUDE

static const char PNG_HEADER[9] = { -119, 80, 78, 71, 13, 10, 26, 10, 0 };

/**
 * Reads a file into a char array.
 *
 * @param file File to read.
 * @param output Pointer that will hold the data that was read.
 * @param error Error output.
 *
 * @return Number of bytes read.
 */
size_t pngif_read_file(FILE *file, unsigned char **output, int *error);

#endif
