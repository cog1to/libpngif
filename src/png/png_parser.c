#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include <zlib.h>

#include "util.h"
#include "raw.h"
#include "parser.h"

/** Private **/

#define CHUNK 32768

/**
 * Compares two strings as if they're headers of a PNG chunk, i.e. check first
 * four bytes of each string. If either string is less than four bytes, the
 * behavior is undefined.
 *
 * @param target First string
 * @param source Second string
 *
 * @return 0 if headers are identical, 1 otherwise.
 */
int cmphdr(char *target, char *source) {
  if (
    target[0] == source[0] &&
    target[1] == source[1] &&
    target[2] == source[2] &&
    target[3] == source[3]
  ) {
    return 0;
  }

  return 1;
}

int parse_header(unsigned char *data, png_header_t *header) {
  header->width = ntohl(*(u_int32_t*)data);
  header->height = ntohl(*(u_int32_t*)(data + 4));
  header->depth = *(u_int8_t*)(data + 8);
  header->color_type = *(u_int8_t*)(data + 9);
  header->compression = *(u_int8_t*)(data + 10);
  header->filter = *(u_int8_t*)(data + 11);
  header->interlace = *(u_int8_t*)(data + 12);

  return 0;
}

int parse_gamma(unsigned char *data, png_gamma_t **gamma) {
  png_gamma_t *out = calloc(1, sizeof(png_gamma_t));
  if (out == NULL) {
    return PNG_ERR_MEMIO;
  }

  out->gamma = ntohl(*(u_int32_t*)data);

  *gamma = out;
  return 0;
}

int parse_palette(unsigned char *data, size_t length, png_palette_t **palette) {
  size_t entries_count = length / 3;

  if ((entries_count == 0) || (length % 3 != 0))
    return PNG_ERR_CHUNK_FORMAT;

  png_palette_t *out = calloc(1, sizeof(png_palette_t));
  if (out == NULL) {
    return PNG_ERR_MEMIO;
  }

  png_color_index_t *entries = malloc(sizeof(png_color_index_t) * entries_count);
  if (entries == NULL) {
    return PNG_ERR_MEMIO;
  }

  for (int idx = 0; idx < entries_count; idx++) {
    entries[idx].red = data[idx * 3];
    entries[idx].green = data[idx * 3 + 1];
    entries[idx].blue = data[idx * 3 + 2];
  }

  out->length = entries_count;
  out->entries = entries;

  *palette = out;
  return 0;
}

int parse_transparency(
  unsigned char *data,
  size_t length,
  int color_type,
  png_transparency_t **transparency
) {
  png_transparency_t *output = malloc(sizeof(png_transparency_t));
  if (output == NULL) {
    return PNG_ERR_MEMIO;
  }

  if (color_type == COLOR_TYPE_GRAYSCALE) {
    output->grayscale = ntohs(*(u_int16_t*)data);
  } else if (color_type == COLOR_TYPE_TRUECOLOR) {
    output->red = ntohs(*(u_int16_t*)data);
    output->green = ntohs(*(u_int16_t*)(data + 2));
    output->blue = ntohs(*(u_int16_t*)(data + 4));
  } else if (color_type == COLOR_TYPE_INDEXED) {
    memset(output->entries, 255, 256);
    for (int idx = 0; idx < length; idx++) {
      output->entries[idx] = data[idx];
    }
  } else {
     return PNG_ERR_INVALID_FORMAT;
  }

  *transparency = output;

  return 0;
}

int parse_sbits(
  unsigned char *data,
  size_t length,
  int color_type,
  unsigned char **sbits
) {
  unsigned char *output = NULL;
  int element_count = 0;

  switch (color_type) {
  case COLOR_TYPE_GRAYSCALE:
    element_count = 1;
    break;
  case COLOR_TYPE_INDEXED:
  case COLOR_TYPE_TRUECOLOR:
    element_count = 3;
    break;
  case COLOR_TYPE_GRAYSCALE_ALPHA:
    element_count = 2;
    break;
  case COLOR_TYPE_TRUECOLOR_ALPHA:
    element_count = 4;
    break;
  }

  if (length != element_count) {
    return PNG_ERR_CHUNK_FORMAT;
  }

  if ((output = malloc(element_count)) == NULL) {
    return PNG_ERR_MEMIO;
  }

  memcpy(output, data, element_count);
  *sbits = output;
  return 0;
}

/**
 * Data parsing is mostly just a Zlib stream decompression. The code for this
 * function is adapter from the Zlib tutorial [https://zlib.net/zlib_how.html]
 *
 * @param raw Raw PNG data struct
 * @param type Chunk type to expect.
 * @param include_seqnum Flag indicating whether first byte of data is a
 *   sequence number.
 * @param idx Starting index of the first data chunk.
 * @param data Output struct.
 *
 * @return Error code if there was a parsing error, or 0 if parsing was
 * successful.
 */
int parse_data(png_raw_t *raw, char *type, int include_seqnum, int *idx, png_data_t *data) {
  int ret;
  size_t have = 0, total = 0;
  z_stream strm;
  unsigned char out[CHUNK];
  unsigned char *uncompressed = NULL;

  // Zlib initialization.
  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  strm.opaque = Z_NULL;
  strm.avail_in = 0;
  strm.next_in = Z_NULL;
  ret = inflateInit(&strm);
  if (ret != Z_OK)
    return ret;

  // Go through all chunks and process data ones.
  for (; *idx < raw->chunk_count; *idx = *idx + 1) {
    png_chunk_raw_t *chunk = raw->chunks[*idx];
    if (cmphdr(type, chunk->type) != 0) {
      continue;
    }

    // Set up to parse next chunk.
    strm.avail_in = chunk->length - 4 * include_seqnum;
    strm.next_in = chunk->data + 4 * include_seqnum;

    // Inflate until output buffer is not full.
    do {
      strm.avail_out = CHUNK;
      strm.next_out = out;
      ret = inflate(&strm, Z_NO_FLUSH);
      switch (ret) {
      case Z_STREAM_ERROR:
      case Z_NEED_DICT:
      case Z_DATA_ERROR:
      case Z_MEM_ERROR:
        (void)inflateEnd(&strm);
        free(uncompressed);
        return ret;
      }

      // Reallocate output array to fit the new chunk.
      have = CHUNK - strm.avail_out;
      uncompressed = realloc(uncompressed, total + have);

      // Allocation error.
      if (uncompressed == NULL) {
        (void)inflateEnd(&strm);
        free(uncompressed);
        return PNG_ERR_MEMIO;
      }

      // Copy the new data to the uncompressed stream.
      memcpy(uncompressed + total, out, have);
      total += have;
    } while (strm.avail_out == 0);

    if (ret == Z_STREAM_END) {
      break;
    }
  }

  // Deinit Zlib.
  (void)inflateEnd(&strm);

  // Check if the Zlib is finished. Anything other than Z_STREAM_END means
  // the data is incomplete.
  if (ret != Z_STREAM_END) {
    free(uncompressed);
    return PNG_ERR_ZLIB;
  }

  // Copy the data to the output.
  data->length = total;
  data->data = uncompressed;

  return 0;
}

int parse_idata(png_raw_t *raw, png_data_t *data) {
  // Find first IDAT chunk.
  int idx = 0;
  while (cmphdr(raw->chunks[idx]->type, "IDAT") != 0) {
    idx += 1;
  }

  // Parse data starting from the first IDAT chunk position.
  return parse_data(raw, "IDAT", 0, &idx, data);
}

int parse_frame_data(png_raw_t *raw, int *idx, png_data_t *data) {
  return parse_data(raw, "fdAT", 1, idx, data);
}

int parse_anim_control(unsigned char *data, png_animation_control_t **anim) {
  png_animation_control_t *out = malloc(sizeof(png_animation_control_t));
  if (out == 0) {
    return PNG_ERR_MEMIO;
  }

  out->num_frames = ntohl(*(u_int32_t*)data);
  out->num_plays = ntohl(*(u_int32_t*)(data + 4));

  *anim = out;
  return 0;
}

int parse_frame_control(unsigned char *data, png_frame_control_t *frame) {
  frame->width = ntohl(*(u_int32_t*)(data + 4));
  frame->height = ntohl(*(u_int32_t*)(data + 8));
  frame->x_offset = ntohl(*(u_int32_t*)(data + 12));
  frame->y_offset = ntohl(*(u_int32_t*)(data + 16));
  frame->delay_num = ntohs(*(u_int16_t*)(data + 20));
  frame->delay_den = ntohs(*(u_int16_t*)(data + 22));
  frame->dispose_type = data[24];
  frame->blend_type = data[25];

  return 0;
}

int parse_anim(png_raw_t *raw, png_parsed_t *png) {
  int err = 0, idx = 0;
  png_animation_control_t *anim = NULL;

  // Search for animation control chunk.
  for (idx = 0; idx < raw->chunk_count; idx++) {
    png_chunk_raw_t *chunk = raw->chunks[idx];
    if (cmphdr("acTL", chunk->type) == 0) {
      err = parse_anim_control(chunk->data, &anim);
      break;
    } else if (cmphdr("fcTL", chunk->type) == 0 || cmphdr("fdAT", chunk->type) == 0) {
      err = PNG_ERR_INVALID_FORMAT;
      break;
    }
  }

  if (err != 0) {
    return err;
  }

  if (anim == NULL || anim->num_frames == 0) {
    // Not an animated PNG, stop here.
    return 0;
  }

  // Now we have frame count, we can allocate space for frame data.
  png_frame_control_t *controls = malloc(sizeof(png_frame_control_t) * anim->num_frames);
  png_data_t *frames = malloc(sizeof(png_data_t) * anim->num_frames);

  // Skip to first 'fcTL' chunk.
  while (cmphdr("fcTL", raw->chunks[idx]->type) != 0) {
    idx += 1;
  }

  // TODO: Verify sequence numbers in frame chunks.
  int frame_index = 0, control = 1;
  for (; idx < raw->chunk_count; idx++) {
    png_chunk_raw_t *chunk = raw->chunks[idx];
    if ((cmphdr("fcTL", chunk->type) == 0) && (control == 1)) {
      parse_frame_control(chunk->data, controls + frame_index);
      control = 0;
    } else if (cmphdr("fdAT", chunk->type) == 0 && (control == 0)) {
      err = parse_frame_data(raw, &idx, frames + frame_index);
      frame_index += 1;
      control = 1;
    } else if ((cmphdr("IDAT", chunk->type) == 0) && (control == 0) && (frame_index == 0)) {
      // Default image is the first frame.
      png->is_data_first_frame = 1;
      memcpy(frames + frame_index, &png->data, sizeof(png_data_t));
      // Skip all IDAT chunks.
      do {
        idx += 1;
      } while (cmphdr("IDAT", raw->chunks[idx]->type) == 0);
      // First frame is filled, advance the frame index.
      idx -= 1;
      frame_index += 1;
      control = 1;
    } else if (cmphdr("IEND", chunk->type) != 0) {
      // Error. Wrong chunk type.
      err = PNG_ERR_INVALID_FORMAT;
      break;
    }
  }

  if (err != 0) {
    free(controls);
    for (int i = 0; i < anim->num_frames; i++) {
      if (frames[i].data != NULL) {
        free(frames[i].data);
      }
    }
    free(frames);
  }

  png->anim_control = anim;
  png->frame_controls = controls;
  png->frames = frames;

  return 0;
}

/** Public **/

void png_parsed_free(png_parsed_t *png) {
  if (png == NULL) {
    return;
  }

  if (png->gamma != NULL)
    free(png->gamma);
  if (png->data.data != NULL)
    free(png->data.data);
  if (png->palette != NULL) {
    if (png->palette->entries != NULL)
      free(png->palette->entries);
    free(png->palette);
  }
  if (png->transparency != NULL)
    free(png->transparency);
  if (png->anim_control != NULL) {
    if (png->frame_controls != NULL)
      free(png->frame_controls);
    if (png->frame_controls != NULL) {
      for (int idx = 0; idx < png->anim_control->num_frames; idx++) {
        if (idx != 0 || png->is_data_first_frame == 0) {
           free(png->frames[idx].data);
        }
      }
      free(png->frames);
    }
  }
  if (png->sbits != NULL)
    free(png->sbits);

  free(png);
}

png_parsed_t *png_create_from_raw(png_raw_t *raw, int *error) {
  if (raw == NULL) {
    return NULL;
  }

  png_parsed_t *png = calloc(1, sizeof(png_parsed_t));
  if (png == NULL) {
    *error = PNG_ERR_MEMIO;
    return NULL;
  }

  int err = 0;
  for (int idx = 0; idx < raw->chunk_count; idx++) {
    png_chunk_raw_t *chunk = raw->chunks[idx];
    if (cmphdr("IHDR", chunk->type) == 0) {
      err = parse_header(chunk->data, &png->header);
    } else if (cmphdr("gAMA", chunk->type) == 0) {
      err = parse_gamma(chunk->data, &png->gamma);
    } else if (cmphdr("PLTE", chunk->type) == 0) {
      err = parse_palette(chunk->data, chunk->length, &png->palette);
    } else if (cmphdr("tRNS", chunk->type) == 0) {
      err = parse_transparency(chunk->data, chunk->length, png->header.color_type, &png->transparency);
    } else if (cmphdr("sBIT", chunk->type) == 0) {
      err = parse_sbits(chunk->data, chunk->length, png->header.color_type, &png->sbits);
    } else {
      // UNKNOWN TYPE OR ANIMATION TYPE.
    }

    if (err != 0) {
      break;
    }
  }

  // Parse IDAT chunks.
  err = parse_idata(raw, &png->data);
  if (err != 0) {
    *error = err;
  }

  // Check and parse animation data.
  err = parse_anim(raw, png);

  *error = err;
  return png;
}
