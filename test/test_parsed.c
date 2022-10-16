/**
 * Takes a GIF file, parses it into separate blocks, and dumps each block's
 * content into STDIO.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <errors.h>
#include <gif_parsed.h>

int main(int argc, char **argv) {
  if (argc < 2) {
    printf("Usage: %s <filepath>\n", argv[0]);
    return 0;
  }

  int error = 0;
  gif_parsed_t *raw = gif_parsed_from_path(argv[1], &error);

  if (error != 0 || raw == NULL) {
    printf("File read error: %d\n", error);
    return -1;
  }

  // Version.
  printf("GIF version: %s\n", raw->version);

  // Logical Screen Descriptor.
  printf("SIZE: %dx%d\n", raw->screen.width, raw->screen.height);
  printf("COLOR RESOLUTION: %d\n", raw->screen.color_resolution);
  printf("COLOR TABLE SIZE: %ld\n", raw->screen.color_table_size);
  printf("BACK PIXEL INDEX: %d\n", raw->screen.background_color_index);
  printf("PIXEL ASPECT RATIO: %d\n", raw->screen.pixel_aspect_ratio);

  // Global color table.
  if (raw->global_color_table != NULL) {
    printf("GLOBAL COLOR TABLE:\n");
    for (int idx = 0; idx < raw->screen.color_table_size; idx++) {
      gif_color_t *color = raw->global_color_table + idx;
      printf("  %d: %02x%02x%02x\n", idx, color->red, color->green, color->blue);
    }
  }

  // Output each block.
  if (raw->block_count > 0) {
    printf("BLOCK COUNT: %ld\n", raw->block_count);
    for (int idx = 0; idx < raw->block_count; idx++) {
      gif_block_t *block = raw->blocks[idx];
      if (block->type == GIF_BLOCK_IMAGE) {
        printf("%d: IMAGE BLOCK\n", idx);
        gif_image_block_t *image = (gif_image_block_t *)block;

        // Descriptor.
        printf("  OFFSET: %d TOP, %d LEFT\n", image->descriptor.top, image->descriptor.left);
        printf("  SIZE: %dx%d\n", image->descriptor.width, image->descriptor.height);
        printf("  INTERLACE: %s\n", image->descriptor.interlace ? "yes" : "no");

        // Color table.
        if (image->descriptor.color_table_size > 0 && image->color_table != NULL) {
          printf("  LOCAL COLOR TABLE:\n");
          for (int cidx = 0; cidx < image->descriptor.color_table_size; cidx++) {
            gif_color_t *color = image->color_table + cidx;
            printf("    %d: %02x%02x%02x\n", cidx, color->red, color->green, color->blue);
          }
        }

        // Graphic control.
        if (image->gc != NULL) {
          printf("  GC DISPOSE: %d\n", image->gc->dispose_method);
          printf("  GC DELAY (IN CENTISEC): %d\n", image->gc->delay_cs);
          printf("  GC USER INPUT: %d\n", image->gc->input_flag);
          printf("  GC TRANSPARENCY: %s\n", (image->gc->transparency_flag > 0) ? "yes" : "no");
          printf("  GC TRANSPARENT COLOR: %d\n", image->gc->transparent_color_index);
        }

        // Min block size.
        printf("  MIN CODE SIZE: %d\n", image->minimum_code_size);

        // Data blocks.
        printf("  IMAGE DATA SIZE: %ld\n", image->data_length);
      } else if (block->type == GIF_BLOCK_APPLICATION) {
        printf("%d: APPLICATION BLOCK\n", idx);
        gif_application_block_t *app = (gif_application_block_t *)block;

        // ID.
        printf("  %s %s\n", app->identifier, app->auth_code);
        printf("  DATA LENGTH: %ld\n", app->length);

        if (strcmp(app->identifier, "NETSCAPE") == 0) {
          printf("  REPEAT COUNT: %d\n", *(u_int16_t *)(app->data + 1));
        }
      } else if (block->type == GIF_BLOCK_COMMENT) {
        printf("%d: COMMENT BLOCK\n", idx);
        gif_text_block_t *text = (gif_text_block_t *)block;
        printf("  '%s'\n", text->data);
      } else if (block->type == GIF_BLOCK_COMMENT) {
        printf("%d: PLAIN TEXT BLOCK\n", idx);
        gif_text_block_t *text = (gif_text_block_t *)block;
        printf("  '%s'\n", text->data);
      }
    }
  } else {
    printf("NO DATA BLOCKS\n");
  }

  gif_parsed_free(raw);
  return 0;
}
