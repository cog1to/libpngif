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

