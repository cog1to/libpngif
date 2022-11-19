#ifndef IMAGE_VIEWER_H
#define IMAGE_VIEWER_H

#include "image.h"

void show_image(animated_image_t *image);
void show_decoded_gif(gif_decoded_t *gif);
void show_decoded_png(png_decoded_t *png);

#endif
