#ifndef _PNG_UTIL_INCLUDE
#define _PNG_UTIL_INCLUDE

/** Useful constants **/

#define COLOR_TYPE_GRAYSCALE 0
#define COLOR_TYPE_TRUECOLOR 2
#define COLOR_TYPE_INDEXED 3
#define COLOR_TYPE_GRAYSCALE_ALPHA 4
#define COLOR_TYPE_TRUECOLOR_ALPHA 6

#define FILTER_NONE 0
#define FILTER_SUB 1
#define FILTER_UP 2
#define FILTER_AVG 3
#define FILTER_PAETH 4

/** Interface **/

/**
 * CRC32 from a byte array.
 *
 * @param input Byte array for which to calculate CRC32
 * @oaram length Length of the array.
 *
 * @return CRC32 checksum for given array.
 **/
u_int32_t u_crc32(void *input, size_t length);

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
