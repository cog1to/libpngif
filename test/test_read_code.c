/**
 * A test to verify LZW code reading algorithm. The main function `decode`
 * takes an LZW-encoded data array, transforms it into a list of codes, and
 * compares to an expected code sequence.
 */

#include <stdlib.h>
#include <stdio.h>

extern size_t gif_read_next_code(
  u_int16_t *output,
  unsigned char *data,
  size_t offset,
  unsigned char code_size
);

void decode(unsigned char *data, u_int16_t *codes, unsigned char code_size, int length) {
  u_int16_t output = 0;
  size_t offset = 0;
  char error = 0;

  printf("CODE SIZE %d\n", code_size);
  printf("----------------------\n");
  printf("| EXPECTED | DECODED |\n");
  printf("----------------------\n");

  for (int idx = 0; idx < length; idx++) {
    offset = gif_read_next_code(&output, data, offset, code_size);
    printf("| %8hu | %7hu |", codes[idx], output);
    if (codes[idx] != output) {
      error = 1;
      printf(" *\n");
    } else {
      printf("\n");
    }
  }
  printf("----------------------\n");

  if (error) {
    printf("failed");
  }
  printf("\n");
}

int main(int argc, char **argv) {
  u_int16_t codes3[] = { 5, 6, 0, 2, 1, 4, 5, 7 };
  unsigned char data3[] = { 0x35, 0x14, 0xF6 };
  decode(data3, codes3, 3, 8);

  u_int16_t codes5[] = { 25, 21, 3, 31, 10, 19, 14, 17 };
  unsigned char data5[] = { 0xB9, 0x8E, 0xAF, 0xA6, 0x8B };
  decode(data5, codes5, 5, 8);

  u_int16_t codes8[] = { 255, 128, 2, 60, 54, 17, 163, 220 };
  unsigned char data8[] = { 0xFF, 0x80, 0x02, 0x3C, 0x36, 0x11, 0xA3, 0xDC };
  decode(data8, codes8, 8, 8);

  u_int16_t codes12[] = { 2032, 5, 330, 255, 4000, 18 };
  unsigned char data12[] = { 0xF0, 0x57, 0x00, 0x4A, 0xF1, 0x0F, 0xA0, 0x2F, 0x01 };
  decode(data12, codes12, 12, 6);

  u_int16_t codes11[] = { 565, 2047, 1231, 100, 893, 893, 1500, 12, 234 };
  unsigned char data11[] = { 0x35, 0xFA, 0xFF, 0x33, 0xC9, 0xD0, 0xB7, 0xBE, 0x71, 0x97, 0x01 };
  decode(data11, codes11, 11, 8);

  return 0;
}
