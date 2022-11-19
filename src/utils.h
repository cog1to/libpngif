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

/**
 * Prints byte in binary form to standard ouput.
 * 
 * @param x Byte to print.
 */
void print_binary(unsigned char x);

/**
 * RGBA to ARGB converter.
 *
 * @param src RGBA data array.
 * @param dest ARGB output array. Must be preallocated with the right size.
 * @oaram width Image width.
 * @oaram height Image height.
 */
void rgba_to_argb(unsigned char *rgba, unsigned char *argb, size_t width, size_t height);

/**
 * RGBA to BGRA converter.
 *
 * @param src RGBA data array.
 * @param dest BGRA output array. Must be preallocated with the right size.
 * @oaram width Image width.
 * @oaram height Image height.
 */
void rgba_to_bgra(unsigned char *rgba, unsigned char *argb, size_t width, size_t height);


#endif
