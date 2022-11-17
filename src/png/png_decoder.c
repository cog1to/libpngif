#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

#include "png_util.h"
#include "png_raw.h"
#include "png_parser.h"
#include "png_decoder.h"
#include "math.h"

/** Private **/

/**
 * Verifies that color type and bit depth are compatible.
 *
 * @param parsed Parsed PNG data.
 *
 * @return Error code in case of a mismatch, or 0 otherwise.
 */
int verify_color_bit_depth(png_parsed_t *parsed) {
  unsigned short depth = parsed->header.depth;

  switch (parsed->header.color_type) {
  case COLOR_TYPE_GRAYSCALE:
    break; // Valid for every bit depth.
  case COLOR_TYPE_TRUECOLOR:
    if (depth != 8 && depth != 16)
      return PNG_ERR_INVALID_FORMAT;
    break;
  case COLOR_TYPE_INDEXED:
    if (depth == 16)
      return PNG_ERR_INVALID_FORMAT;
    break;
  case COLOR_TYPE_GRAYSCALE_ALPHA:
    if (depth != 8 && depth != 16)
      return PNG_ERR_INVALID_FORMAT;
    break;
  case COLOR_TYPE_TRUECOLOR_ALPHA:
    if (depth != 8 && depth != 16)
      return PNG_ERR_INVALID_FORMAT;
    break;
  }

  return 0;
}

/**
 * Returns the number of samples that compose a pixel for given color type.
 * For example, if the color type is TrueColor, it means that each pixel has
 * R, G, and B values, thus the number of samples is 3.
 *
 * @param type Color type value.
 *
 * @return Number of samples composing a pixel.
 */
int samples_per_pixel(int type) {
  switch (type) {
  case COLOR_TYPE_GRAYSCALE:
    return 1;
  case COLOR_TYPE_TRUECOLOR:
    return 3;
  case COLOR_TYPE_INDEXED:
    return 1;
  case COLOR_TYPE_GRAYSCALE_ALPHA:
    return 2;
  case COLOR_TYPE_TRUECOLOR_ALPHA:
    return 4;
  }

  // Unknown color type. We shouldn't be here.
  return -1;
}

/**
 * De-filtering functions
 *
 * Note: PNG standard is a little confusing when it comes to reconstructing
 * image data from filtered data. If bit depth of the image is less than 8,
 * then the term 'previous byte' means just than - the byte preceding the
 * current byte in the scanline. But when bit depth is set to 8 or 16, it takes
 * another meaning: a previous byte is now a byte at the same offset as the
 * current byte from the start of the previous pixel data. Here's an
 * illustration. Let's say we have an TrueColor image with bith depth 8. Each
 * pixel is 3 bytes long. Here's how we get to the 'previous byte' from a given
 * position:
 *
 *                                   ┌─current byte
 * Scanline data: ...RGB|RGB|RGB|RGB|RGB|RGB|RGB|RGB|RGB...
 *                 previous byte─┘
 *
 * Same goes for bytes 'above current byte' and 'before the byte above current
 * byte', and for 16-bit image data: if current byte is a second byte in Green
 * component of a pixel, then previous byte would be a second byte in Green
 * component of a previous pixel.
 **/

/**
 * For given scanline number and byte position in a scanline, returns a
 * corresponding byte value in a previous pixel.
 *
 * @param data Array of pixel data.
 * @param width Size of a single scanline.
 * @param pos Byte offset to the target positon within a scanline
 * @param depth Bit depth of the image data.
 * @param bpp Number of bytes per pixel in the image.
 *
 * @return Byte value for the previous pixel in the same byte position.
 */
unsigned char prev_byte(unsigned char *data, size_t width, size_t line, size_t pos, int depth, int bpp) {
  if (pos < bpp) {
    return 0;
  }

  if (depth < 8) {
    return data[width * line + pos - 1];
  } else {
    return data[width * line + pos - bpp];
  }
}

/**
 * For given scanline number and byte position in a scanline, returns a
 * corresponding byte value in a pixel above current pixel.
 *
 * @param data Array of pixel data.
 * @param width Size of a single scanline.
 * @param pos Byte offset to the target positon within a scanline
 * @param depth Bit depth of the image data.
 * @param bpp Number of samples per pixel in the image.
 *
 * @return Byte value for the pixel above in the same byte position.
 */
unsigned char up_byte(unsigned char *data, size_t width, size_t line, size_t pos, int depth, int bpp) {
  if (line == 0)
    return 0;

  return data[width * (line - 1) + pos];
}

/**
 * For given scanline number and byte position in a scanline, returns an
 * average of previous byte and the byte above current byte.
 *
 * @param data Array of pixel data.
 * @param width Size of a single scanline.
 * @param pos Byte offset to the target positon within a scanline
 * @param depth Bit depth of the image data.
 * @param bpp Number of samples per pixel in the image.
 *
 * @return Average of the byte in the previous pixel and a byte above current
 *   pixel.
 */
unsigned char avg_byte(unsigned char *data, size_t width, size_t line, size_t pos, int depth, int bpp) {
  // Using unsigned short to avoid overflow. Overflow is expected in other
  // restoration functions, but not in Average or Paeth math.
  unsigned short uprev = prev_byte(data, width, line, pos, depth, bpp);
  unsigned short uup = up_byte(data, width, line, pos, depth, bpp);
  unsigned short result = (uprev + uup) >> 1;
  return (unsigned char)result;
}

/**
 * For given scanline number and byte position in a scanline, returns a
 * corresponding byte value for a pixel right before the pixel above the target
 * pixel.
 *
 * @param data Array of pixel data.
 * @param width Size of a single scanline.
 * @param pos Byte offset to the target positon within a scanline
 * @param depth Bit depth of the image data.
 * @param bpp Number of samples per pixel in the image.
 *
 * @return A byte value from the pixel that is one pixel above and before the
 *   pixel containing target byte.
 */
unsigned char prev_up_byte(unsigned char *data, size_t width, size_t line, size_t pos, int depth, int bpp) {
  if (line == 0)
    return 0;

  if (pos < bpp)
    return 0;

  if (depth < 8) {
    return data[width * (line - 1) + pos - 1];
  } else {
    return data[width * (line - 1) + pos - bpp];
  }
}

/**
 * For given scanline number and byte position in a scanline, returns a
 * Paeth predictor value, which is calculated from the previous byte value
 * above byte value, and the byte value previous to the above byte value.
 *
 * @param data Array of pixel data.
 * @param width Size of a single scanline.
 * @param pos Byte offset to the target positon within a scanline
 * @param depth Bit depth of the image data.
 * @param bpp Number of samples per pixel in the image.
 *
 * @return Paeth predictor value for the given byte.
 */
unsigned char paeth_byte(unsigned char *data, size_t width, size_t line, size_t pos, int depth, int bpp) {
  short a = prev_byte(data, width, line, pos, depth, bpp);
  short b = up_byte(data, width, line, pos, depth, bpp);
  short c = prev_up_byte(data, width, line, pos, depth, bpp);

  short p = a + b - c;
  short pa = abs(p - a);
  short pb = abs(p - b);
  short pc = abs(p - c);

  if ((pa <= pb) && (pa <= pc)) {
    return a;
  } else if (pb <= pc) {
    return b;
  } else {
    return c;
  }
}

/** De-filetering algorithm **/

/**
 * Reverses the filter applications for given image data. In PNG, all scanlines
 * are stored filtered. A filtered scanlane consists of a byte value of the
 * filter, followed by the byte values calculated as a result of applying the
 * selected filter to the original image data.
 *
 * @param data Image data in a filtered format. It should be larger than the
 *   clear image data, because each scanline is preceded by a filter byte.
 * @param width Image width in pixels.
 * @param height Image height in pixels.
 * @param type Color type of the image.
 * @oaram depth Bit dfepth of the image.
 * @param error Error return value.
 *
 * @return Defiltered data of size (width * height) * (samples) * (depth / 8).
 */
unsigned char *defilter_data(unsigned char *data, size_t width, size_t height, int type, int depth, int *error) {
  int bytes_per_line = (width * samples_per_pixel(type) * depth + 8 - 1) / 8;
  // Number of bytes per pixel.
  int bpp = (depth < 8) ? 1 : (samples_per_pixel(type) * (depth / 8));

  unsigned char *output = malloc(bytes_per_line * height);
  if (output == NULL) {
    *error = PNG_ERR_MEMIO;
    return NULL;
  }

  for (size_t line = 0; line < height; line++) {
    // Filter type. One of 'None', 'Sub', 'Up', 'Average', 'Paeth'.
    u_int8_t filter_type = data[line * (bytes_per_line + 1)];

    // Transform each byte.
    for (u_int32_t byte = 0; byte < bytes_per_line; byte++) {
      u_int32_t out_offset = line * (bytes_per_line) + byte;
      u_int32_t in_offset = line * (bytes_per_line + 1) + byte + 1;

      switch (filter_type) {
      case FILTER_NONE:
        output[out_offset] = data[in_offset];
        break;
      case FILTER_SUB:
        output[out_offset] = data[in_offset] + prev_byte(output, bytes_per_line, line, byte, depth, bpp);
        break;
      case FILTER_UP:
        output[out_offset] = data[in_offset] + up_byte(output, bytes_per_line, line, byte, depth, bpp);
        break;
      case FILTER_AVG:
        output[out_offset] = data[in_offset] + avg_byte(output, bytes_per_line, line, byte, depth, bpp);
        break;
      case FILTER_PAETH:
        output[out_offset] = data[in_offset] + paeth_byte(output, bytes_per_line, line, byte, depth, bpp);
        break;
      }
    }
  }

  return output;
}

/**
 * Max unsigned integer value for given bit depth.
 *
 * @param depth Bit depth.
 *
 * @return Maximum integer value for given number of bits.
 */
u_int16_t max_int(int depth) {
  switch (depth) {
  case 1:
    return 1;
  case 2:
    return 3;
  case 4:
    return 15;
  case 8:
    return 255;
  case 16:
    return 65535;
  default:
    return 0; // Unsupported bit depth. We shouldn't be here
  }
}

/**
 * Returns a sample value extracted from the image data at given byte offset and bit offset.
 *
 * @param data Image data.
 * @param byte_offset Byte offset within the image data.
 * @param bit_offset Bit offset to the target sample value within the byte.
 * @param depth Bit depth of the image.
 * @param type Color type of the image.
 *
 * @return Sample value extracted from the image data at given position.
 */
u_int16_t take_sample(
  unsigned char *data,
  u_int32_t byte_offset,
  u_int32_t bit_offset,
  int depth,
  int type
) {
  u_int16_t sample = 0;
  if (depth == 16) {
    // 16-bit values are in network byte order, so we read and reverse it (if needed).
    sample = ntohs(*(u_int16_t*)(data + byte_offset));
  } else if (depth == 8) {
    sample = data[byte_offset];
  } else {
    // To isolate sample value in a byte we first erase bit_index previous bits,
    // and then erase all bits after the sample. For example, let's say we have
    // bit depth of 2, and current sample offset of 2:
    //   input byte: 01[11]0101
    //   1. erase bit_index first bits by shift left: 01[11]0101 -> [11]010100
    //   2. erase bits after sample by shift right: [11]010100 -> 0000000[11]
    unsigned char byte = data[byte_offset];
    unsigned char sample_value = byte << bit_offset;
    sample = sample_value >> (8 - depth);
  }

  return sample;
}

/**
 * Converts 16-bit pixel values to 8-bit ones.
 *
 * @param original Original pixel color values.
 * @param dest Pointer to the destination pixel values.
 * @param color_type Image's color type.
 * @param depth Image's bit depth.
 */
void scale_pixel(u_int16_t *original, unsigned char *dest, int color_type, int depth) {
  for (int idx = 0; idx < 4; idx++) {
    if (depth == 16) {
      dest[idx] = (unsigned char)floor(((float)original[idx] * (float)max_int(8) / (float)max_int(16)) + 0.5);
    } else if (depth == 8 || color_type == COLOR_TYPE_INDEXED) {
      dest[idx] = original[idx];
    } else {
      dest[idx] = original[idx] * 255 / max_int(depth);
    }
  }
}

/**
 * Transforms "packed" image data, i. e. an array of concatenated pixel values
 * with a specific sample size into an array of RGBA values.
 *
 * @param data Image data in a packed format.
 * @param width Image width in pixels.
 * @param height Image height in pixels.
 * @param type Color type of the image.
 * @param depth Sample bit depth of the image.
 * @param palette Optional pointer to the image's palette. Relevant for
 *   Index-Colored images.
 * @param transparency Optional transparency data. Used to change transparency
 *   of certain pixels based on the data.
 *
 * @return An array of RGBA pixel values extracted from the original samples.
 */
unsigned char *unpack_data(
  unsigned char *data,
  size_t width,
  size_t height,
  int type,
  int depth,
  png_palette_t *palette,
  png_transparency_t *transparency,
  int *error
) {
  // Number of bytes in a scanline.
  u_int32_t scanline_size = (width * samples_per_pixel(type) * depth + 8 - 1) / 8;
  // Offset to current pixel in output.
  u_int32_t out_offset = 0;
  // Temp storage for 'current' pixel we're reading from image data.
  u_int16_t pixel[4] = { 0 };
  // Pixel color value offset;
  u_int32_t pixel_offset = 0;

  unsigned char *output = malloc(width * height * 4); // 4-byte RGBA.
  if (output == NULL) {
    *error = PNG_ERR_MEMIO;
    return NULL;
  }

  if (type == COLOR_TYPE_INDEXED && palette == NULL) {
    *error = PNG_ERR_INVALID_FORMAT;
    return NULL;
  }

  for (int line = 0; line < height; line++) {
    for (int idx = 0; idx < width * samples_per_pixel(type); idx++) {
      // Bit offset to current sample: [scanline offset] + [sample index] * [bits per sample];
      u_int32_t sample_offset = (line * scanline_size) * 8 + idx * depth;
      // Full byte offset to current sample.
      u_int32_t byte_offset = sample_offset / 8;
      // Bit offset within current byte.
      u_int32_t bit_offset = sample_offset % 8;

      // Sample value.
      u_int16_t sample = take_sample(data, byte_offset, bit_offset, depth, type);

      switch (type) {
      case COLOR_TYPE_GRAYSCALE:
        pixel[0] = sample;
        pixel[1] = sample;
        pixel[2] = sample;
        if (transparency != NULL && transparency->grayscale == sample) {
          pixel[3] = 0;
        } else {
          pixel[3] = max_int(depth);
        }

        pixel_offset += 4;
        break;
      case COLOR_TYPE_TRUECOLOR:
        // First three samples are R, G, B
        pixel[pixel_offset] = sample;
        pixel_offset += 1;
        // Next byte is alpha, check transparency table.
        if (pixel_offset == 3) {
          if (transparency != NULL &&
            transparency->red == pixel[0] &&
            transparency->green == pixel[1] &&
            transparency->blue == pixel[2]
          ) {
            pixel[3] = 0;
          } else {
            pixel[3] = max_int(depth);
          }
          pixel_offset += 1;
        }
        break;
      case COLOR_TYPE_GRAYSCALE_ALPHA:
        // first sample -> same value for each of RGB, next sample -> alpha
        if ((pixel_offset % 4) == 0) {
          pixel[pixel_offset + 0] = sample; // R
          pixel[pixel_offset + 1] = sample; // G
          pixel[pixel_offset + 2] = sample; // B
          pixel_offset += 3;
        } else {
          pixel[pixel_offset] = sample; // A
          pixel_offset += 1;
        }
        break;
      case COLOR_TYPE_TRUECOLOR_ALPHA:
        // Each sample is 1-to-1 match to output.
        pixel[pixel_offset] = sample;
        pixel_offset += 1;
        break;
      case COLOR_TYPE_INDEXED:
        // Sample value should be within the palette size. Otherwise it's an error
        // and we skip it.
        if (sample < palette->length) {
          png_color_index_t color = palette->entries[sample];
          pixel[0] = color.red;
          pixel[1] = color.green;
          pixel[2] = color.blue;
          if (transparency != NULL) {
            pixel[3] = transparency->entries[sample];
          } else {
            pixel[3] = max_int(8);
          }
        }
        pixel_offset += 4;
        break;
      }

      // Full pixel is filled, convert it to 8-bit and save.
      if (pixel_offset == 4) {
        scale_pixel(pixel, output + out_offset, type, depth);
        out_offset += 4;
        pixel_offset = 0;
      }
    }
  }

  return output;
}

unsigned char *decode_normal_data(
  unsigned char *data,
  size_t width,
  size_t height,
  int type,
  int depth,
  png_palette_t *palette,
  png_transparency_t *transparency,
  int *error
) {
  // Defilter data.
  unsigned char *defiltered = defilter_data(
    data,
    width,
    height,
    type,
    depth,
    error
  );

  if (defiltered == NULL || *error != 0) {
    return NULL;
  }

  // Decode pixel data.
  unsigned char *unpacked = unpack_data(
    defiltered,
    width,
    height,
    type,
    depth,
    palette,
    transparency,
    error
  );

  free (defiltered);
  return unpacked;
}

unsigned char *decode_interlaced_data(
  unsigned char *data,
  size_t width,
  size_t height,
  int type,
  int depth,
  png_palette_t *palette,
  png_transparency_t *transparency,
  int *error
) {
  unsigned char *output = malloc(width * height * 4); // 4-byte RGBA.
  if (output == NULL) {
    *error = PNG_ERR_MEMIO;
    return NULL;
  }

  /**
   * Adam7 Interlacing method defines 7 passes for interlacing:
   *
   * 1 6 4 6 2 6 4 6
   * 7 7 7 7 7 7 7 7
   * 5 6 5 6 5 6 5 6
   * 7 7 7 7 7 7 7 7
   * 3 6 4 6 3 6 4 6
   * 7 7 7 7 7 7 7 7
   * 5 6 5 6 5 6 5 6
   * 7 7 7 7 7 7 7 7
   *
   * This pattern is placed on top of the image data starting from upper left
   * pixel, and repeated. Thus each pixel gets a "pass" number assigned to it.
   * Then pixels with the matching pass number are extracted into a separate
   * "reduced image", thus forming 1 to 7 new sub-images. Each image is then
   * serialized as a normal PNG image, and all serialized images' data is
   * concatenated to form the final data stream.
   *
   * So, to deinterlace the image, we iterate over pass numbers, extract and
   * decode each "reduce image", and then place pixels from that image into the
   * final image. The target pixel's position is determined from the source
   * pixel's coordinates in the reduced image and current pass number.
   */
  int starting_row[7]  = { 0, 0, 4, 0, 2, 0, 1 };
  int starting_col[7]  = { 0, 4, 0, 2, 0, 1, 0 };
  int row_increment[7] = { 8, 8, 8, 4, 4, 2, 2 };
  int col_increment[7] = { 8, 8, 4, 4, 2, 2, 1 };

  size_t offset = 0;
  for (int pass = 0; pass < 7; pass++) {
    if (width > starting_col[pass] && height > starting_row[pass]) {
      // Number of pixels per line in a reduced image.
      size_t pixels_per_line = (width - starting_col[pass] + col_increment[pass] - 1) / col_increment[pass];
      // Number of lines in a reduced image.
      size_t line_count = (height - starting_row[pass] + row_increment[pass] - 1) / row_increment[pass];
      // Number of bytes in a scanline + 1 extra for filter byte.
      size_t scanline_size = (pixels_per_line * samples_per_pixel(type) * depth + 8 - 1) / 8 + 1;

      // Decode reduced image
      unsigned char *reduced_image = decode_normal_data(
        data + offset,
        pixels_per_line,
        line_count,
        type,
        depth,
        palette,
        transparency,
        error
      );

      if (reduced_image == NULL || *error != 0) {
        free(output);
        return NULL;
      }

      // Fill the image from reduced image.
      for (size_t row = starting_row[pass], rrow = 0; row < height; row += row_increment[pass], rrow += 1) {
        for (size_t col = starting_col[pass], rcol = 0; col < width; col += col_increment[pass], rcol += 1) {
          memcpy(output + (row * width + col) * 4, reduced_image + (pixels_per_line * rrow + rcol) * 4, 4);
        }
      }

      // Cleanup.
      free(reduced_image);
      offset += (scanline_size * line_count);
    }
  }

  return output;
}

unsigned char *decode_image(
  png_parsed_t *parsed,
  u_int32_t width,
  u_int32_t height,
  unsigned char *data,
  int *error
) {
  if (parsed->header.interlace == 1) {
   return decode_interlaced_data(
      data,
      width,
      height,
      parsed->header.color_type,
      parsed->header.depth,
      parsed->palette,
      parsed->transparency,
      error
    );
  } else if (parsed->header.interlace == 0) {
    return decode_normal_data(
      data,
      width,
      height,
      parsed->header.color_type,
      parsed->header.depth,
      parsed->palette,
      parsed->transparency,
      error
    );
  } else {
    *error = PNG_ERR_UNSUPPORTED_FORMAT;
    return NULL;
  }
}

void decode_frames(png_t *png, png_parsed_t *parsed, int *error) {
  u_int32_t num_frames = parsed->anim_control->num_frames;
  if (num_frames <= 0) {
    return;
  }

  png_frame_list_t *list = malloc(sizeof(png_frame_list_t));
  if (list == NULL) {
    *error = PNG_ERR_MEMIO;
    return;
  }

  list->length = num_frames;
  list->plays = parsed->anim_control->num_plays;
  list->frames = malloc(sizeof(png_frame_t) * num_frames);

  u_int32_t idx;
  for (idx = 0; idx < num_frames; idx++) {
    png_frame_control_t *control = parsed->frame_controls + idx;
    unsigned char *decoded_frame = NULL;

    if (idx != 0 || parsed->is_data_first_frame != 1) {
      // Decode image.
      png_data_t data = parsed->frames[idx];
      decoded_frame = decode_image(
        parsed,
        control->width,
        control->height,
        data.data,
        error
      );
    } else {
      // First frame is default image, copy it.
      u_int32_t total_size = 4 * png->width * png->height;
      if ((decoded_frame = malloc(total_size)) != NULL) {
        memcpy(decoded_frame, png->data, total_size);
      }
    }

    if (decoded_frame == NULL || *error != 0) {
      break;
    }

    list->frames[idx].data = decoded_frame;
    list->frames[idx].width = control->width;
    list->frames[idx].height = control->height;
    list->frames[idx].x_offset = control->x_offset;
    list->frames[idx].y_offset = control->y_offset;
    list->frames[idx].dispose_type = control->dispose_type;
    list->frames[idx].blend_type = control->blend_type;

    if (control->delay_den == 0) {
      list->frames[idx].delay = (float)(control->delay_num) / 100.0;
    } else {
      list->frames[idx].delay = (float)(control->delay_num) / (float)(control->delay_den);
    }
  }

  if (*error != 0) {
    // Cleanup. TODO: Better cleanup.
    free(list);
    return;
  }

  png->frames = list;
}

/** Public **/

void png_free(png_t *png) {
  if (png == NULL) {
    return;
  }

  if (png->data != NULL) {
    free(png->data);
  }

  if (png->frames != NULL) {
    if (png->frames->frames != NULL) {
      for (int idx = 0; idx < png->frames->length; idx++) {
        free(png->frames->frames[idx].data);
      }
      free(png->frames->frames);
    }
    free(png->frames);
  }

  free(png);
}

png_t *png_create_from_parsed(png_parsed_t *parsed, int convert_to_argb, int *error) {
  if (parsed == NULL || parsed->data.length == 0 || parsed->data.data == NULL) {
    return NULL;
  }

  // Verify the format.
  int err = verify_color_bit_depth(parsed);
  if (err != 0) {
    *error = err;
    return NULL;
  }

  unsigned char *decoded = NULL;

  if (parsed->header.interlace == 1) {
    decoded = decode_interlaced_data(
      parsed->data.data,
      parsed->header.width,
      parsed->header.height,
      parsed->header.color_type,
      parsed->header.depth,
      parsed->palette,
      parsed->transparency,
      &err
    );
  } else if (parsed->header.interlace == 0) {
    decoded = decode_normal_data(
      parsed->data.data,
      parsed->header.width,
      parsed->header.height,
      parsed->header.color_type,
      parsed->header.depth,
      parsed->palette,
      parsed->transparency,
      &err
    );
  } else {
    *error = PNG_ERR_UNSUPPORTED_FORMAT;
    return NULL;
  }

  if (err != 0 || decoded == NULL) {
    *error = err;
    return NULL;
  }

  // Allocate PNG struct.
  png_t *result = malloc(sizeof(png_t));
  if (result == NULL) {
    *error = PNG_ERR_MEMIO;
    return NULL;
  }

  result->width = parsed->header.width;
  result->height = parsed->header.height;
  result->data = decoded;

  // Decode animation data.
  if (parsed->anim_control != NULL) {
    decode_frames(result, parsed, &err);
  } else {
    result->frames = NULL;
  }

  return result;
}
