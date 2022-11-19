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

#endif
