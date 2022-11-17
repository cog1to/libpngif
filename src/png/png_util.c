#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/**
 * The code below is taken verbatim from the PNG standard
 *   https://www.w3.org/TR/PNG/
 * with an exception of small optimization for crc table constructor taken from
 *   https://create.stephan-brumme.com/crc32/
 **/

/* Table of CRCs of all 8-bit messages. */
u_int32_t crc_table[256];

/* Flag: has the table been computed? Initially false. */
int crc_table_computed = 0;

/* Make the table for a fast CRC. */
void make_crc_table(void) {
  u_int32_t c;
  int n, k;

  for (n = 0; n < 256; n++) {
    c = (u_int32_t) n;
    for (k = 0; k < 8; k++) {
       c = (c >> 1) ^ (-(int)(c & 1) & 0xedb88320);
    }
    crc_table[n] = c;
  }
  crc_table_computed = 1;
}

u_int32_t update_crc(u_int32_t crc, unsigned char *buf, int len) {
  u_int32_t c = crc;
  int n;

  if (!crc_table_computed)
    make_crc_table();
  for (n = 0; n < len; n++) {
    c = crc_table[(c ^ buf[n]) & 0xff] ^ (c >> 8);
  }
  return c;
}

u_int32_t u_crc32(void *buf, size_t length) {
  return update_crc(0xffffffff, buf, length) ^ 0xffffffff;
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
