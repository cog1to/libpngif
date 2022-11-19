#ifndef _PNG_UTIL_INCLUDE
#define _PNG_UTIL_INCLUDE

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
