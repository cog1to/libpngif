#ifndef IMAGE_VIEWER_H
#define IMAGE_VIEWER_H

#include "image.h"

void show_image_mac(animated_image_t *image);
void show_decoded_gif_mac(gif_decoded_t *gif);

void show_image_linux(animated_image_t *image);
void show_decoded_gif_linux(gif_decoded_t *gif);

#endif
