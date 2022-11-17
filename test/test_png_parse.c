#include <stdlib.h>
#include <stdio.h>

#include "png_util.h"
#include "png_raw.h"
#include "png_parser.h"

int main(int argc, char **argv) {
  if (argc < 2) {
    printf("Usage: %s <filename.png>\n", argv[0]);
    return 0;
  }

  int error = 0;
  png_parsed_t *png = png_parsed_from_path(argv[1], &error);
  if (png == NULL || error != 0) {
    printf("Failed to parse file: %d.\n", error);
    return 0;
  }

  // Print the header.
  printf("SIZE: %dx%d\n", png->header.width, png->header.height);
  printf("BIT DEPTH: %d\n", png->header.depth);
  printf("COLOR TYPE: %d\n", png->header.color_type);
  printf("COMPRESSION: %d\n", png->header.compression);
  printf("FILTER: %d\n", png->header.filter);
  printf("INTERLACE: %d\n", png->header.interlace);

  // Image data.
  printf("****************\n");
  printf("DATA SIZE: %d\n", png->data.length);

  // Non-essential data.
  printf("****************\n");

  if (png->sbits != NULL) {
    printf("SBITS: ");
    switch (png->header.color_type) {
    case COLOR_TYPE_GRAYSCALE:
      printf("G: %d\n", png->sbits[0]);
      break;
    case COLOR_TYPE_GRAYSCALE_ALPHA:
      printf("G: %d, A: %d\n", png->sbits[0], png->sbits[1]);
      break;
    case COLOR_TYPE_TRUECOLOR:
    case COLOR_TYPE_INDEXED:
      printf("R %d, G: %d, B: %d\n",
        png->sbits[0],
        png->sbits[1],
        png->sbits[2]
      );
      break;
    case COLOR_TYPE_TRUECOLOR_ALPHA:
      printf("R %d, G: %d, B: %d, A: %d\n",
        png->sbits[0],
        png->sbits[1],
        png->sbits[2],
        png->sbits[3]
      );
      break;
    }
  }

  if (png->gamma != NULL) {
    printf("GAMMA: %d\n", png->gamma->gamma);
  }

  if (png->palette != NULL) {
    printf("PALETTE: %ld entries\n", png->palette->length);
    for (int idx = 0; idx < png->palette->length; idx++) {
      png_color_index_t index = png->palette->entries[idx];
      printf("  %03d: %d,%d,%d\n", idx, index.red, index.green, index.blue);
    }
  }

  if (png->transparency != NULL) {
    printf("TRANSPARENCY: ");
    switch (png->header.color_type) {
    case COLOR_TYPE_GRAYSCALE:
      printf("%d\n", png->transparency->grayscale);
      break;
    case COLOR_TYPE_TRUECOLOR:
      printf("R %d, G: %d, B: %d\n",
        png->transparency->red,
        png->transparency->green,
        png->transparency->blue
      );
      break;
    case COLOR_TYPE_INDEXED:
      printf("\n");
      for (int idx = 0; idx < 256; idx++) {
        if (png->transparency->entries[idx] != 255) {
          printf("  %03d: %u\n", idx, png->transparency->entries[idx]);
        }
      }
      break;
    }
  }

  if (png->anim_control != NULL) {
    printf("ANIMATION:\n");
    printf("  frames: %d, plays: %d, include default image: %s\n",
      png->anim_control->num_frames,
      png->anim_control->num_plays,
      png->is_data_first_frame ? "yes" : "no"
    );
    for (int idx = 0; idx < png->anim_control->num_frames; idx++) {
      printf("  FRAME %d:\n", idx);
      png_frame_control_t control = png->frame_controls[idx];
      printf("    pos: (%d, %d), size: %dx%d\n",
        control.x_offset,
        control.y_offset,
        control.width,
        control.height
      );
      printf("    delay: %d\\%d\n", control.delay_num, control.delay_den);
      printf("    dispose: %d, blend: %d\n", control.dispose_type, control.blend_type);
      printf("    data size: %d\n", png->frames[idx].length);
    }
  }

  png_parsed_free(png);
  return 1;
}
