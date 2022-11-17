#ifndef PNGIF_UTILS_INCLUDE
#define PNGIF_UTILS_INCLUDE

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
