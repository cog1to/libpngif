#ifndef GIF_PARSED_HEADER
#define GIF_PARSED_HEADER

#include <stdlib.h>

/** Data types **/

typedef struct {
  u_int32_t width;
  u_int32_t height;
  unsigned char color_resolution;
  unsigned char color_table_size;
  unsigned char background_color_index;
  unsigned char pixel_aspect_ratio;
} gif_lsd_t;

typedef struct {
  unsigned char red;
  unsigned char green;
  unsigned char blue;
} gif_color_t;

/** Blocks **/

typedef struct {
  unsigned char type;
} gif_block_t;

static const unsigned char GIF_BLOCK_APPLICATION = 1;
static const unsigned char GIF_BLOCK_COMMENT = 2;
static const unsigned char GIF_BLOCK_PLAIN_TEXT = 3;
static const unsigned char GIF_BLOCK_GRAPHIC_CONTROL = 4;
static const unsigned char GIF_BLOCK_IMAGE = 5;

typedef struct {
  unsigned char type;

  // Application signature/identifier.
  char identifier[9];
  char auth_code[4];

  // Extension data.
  size_t length;
  unsigned char *data;
} gif_application_block_t;

typedef struct {
  unsigned char type;
  size_t length;
  char *data;
} gif_text_block_t;

typedef struct {
  unsigned char type;
  u_int8_t dispose_method;
  u_int8_t input_flag;
  u_int8_t transparency_flag;
  u_int8_t transparent_color_index;
  u_int16_t delay_cs;
} gif_gc_block_t;

typedef struct {
  u_int32_t width;
  u_int32_t height;
  u_int32_t left;
  u_int32_t top;
  unsigned char interlace;
  unsigned char color_table_size;
} gif_image_descriptor_t;

typedef struct {
  unsigned char type;

  // Image descriptor.
  gif_image_descriptor_t descriptor;

  // Optional fields.
  gif_gc_block_t *gc;
  gif_color_t *color_table;

  // Data blocks.
  u_int8_t minimum_code_size;
  size_t data_length;
  unsigned char *data;
} gif_image_block_t;

/** Container **/

typedef struct {
  char version[4];
  gif_lsd_t screen;
  gif_color_t *global_color_table;

  size_t block_count;
  gif_block_t **blocks;
} gif_parsed_t;

/** Interface **/

/**
 * Loads GIF data from a file.
 *
 * @param filename Path to the file to read.
 * @param error Output error;
 *
 * @return Parsed GIF data, or NULL in case of fatal errors.
 **/
gif_parsed_t* gif_parsed_from_file(const char *filename, int *error);

/**
 * Frees memory occupied by a parsed GIF.
 *
 * @param gif GIF struct to free.
 */
void gif_parsed_free(gif_parsed_t *gif);

#endif
