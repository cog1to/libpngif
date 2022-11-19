#include <stdlib.h>
#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <string.h>

#include "png_decoded.h"
#include "gif_decoded.h"
#include "image.h"
#include "png_util.h"

/** Private **/

XImage *ximage_from_data(
  Display *display, Visual *visual,
  unsigned char *image,
  int canvas_width, int canvas_height,
  int offset_x, int offset_y,
  int width, int height
) {
  // XLib wants BGRA image format, so convert it.
  unsigned char *brga = malloc(width * height * 4);
  rgba_to_bgra(image, brga, width, height);

  unsigned char *image32 = (unsigned char *)calloc(1, canvas_width * canvas_height * 4);

  for (int row = 0; row < height; row++) {
    for (int col = 0; col < width; col++) {
      int canvas_offset = (canvas_width * (offset_y + row) * 4) + ((offset_x + col) * 4);
      int offset = (row * width + col) * 4;
      // X11 can't work with 32-bit color space properly (or I'm dumb and can't
      // find appropriate info on it), so here we do a color transform: for each
      // pixel we explicitly multiply it by its alpha value to get it
      // proportionally closer to the window's 'background pixel', which is set
      // to 0x00000000 on window's creation.
      //
      // This should not be required on systems that do support ARGB properly,
      // they should be able to just take and apply pixel's alpha value.
      image32[canvas_offset + 0] = (u_int16_t)(brga[offset + 0]) * brga[offset + 3] / 255;
      image32[canvas_offset + 1] = (u_int16_t)(brga[offset + 1]) * brga[offset + 3] / 255;
      image32[canvas_offset + 2] = (u_int16_t)(brga[offset + 2]) * brga[offset + 3] / 255;
      image32[canvas_offset + 3] = brga[offset + 3];
    }
  }

  free(brga);
  return XCreateImage(display, visual, 32, ZPixmap, 0, (char *)image32, width, height, 32, 0);
}

/** Container **/

typedef struct {
  int delay_ms;
  XImage *image;
} image_holder_frame_t;

typedef struct {
  // Image settings.
  int32_t width;
  int32_t height;
  // Frames.
  int length;
  image_holder_frame_t *frames;
  // Animation state.
  int index;
  int running;
} image_holder_t;

image_holder_t *image_holder_from_png(png_t *png, Display *display, Visual *visual) {
  if (png == NULL || png->data == NULL) {
    return NULL;
  }

  image_holder_t *holder = malloc(sizeof(image_holder_t));
  if (holder == NULL) {
    return NULL;
  }

  // TODO: Frames
  holder->width = png->width;
  holder->height = png->height;
  holder->index = 0;
  holder->running = 0;

  if (png->frames != NULL && png->frames->length > 0) {
    holder->frames = malloc(sizeof(image_holder_frame_t) * png->frames->length);

    for (int idx = 0; idx < png->frames->length; idx++) {
      holder->frames[idx].delay_ms = png->frames->frames[idx].delay * 1000;
      holder->length = png->frames->length;
      holder->frames[idx].image = ximage_from_data(
        display, visual,
        png->frames->frames[idx].data,
        png->width, png->height,
        png->frames->frames[idx].x_offset, png->frames->frames[idx].y_offset,
        png->frames->frames[idx].width, png->frames->frames[idx].height
      );
    }
  } else {
    holder->frames = malloc(sizeof(image_holder_frame_t) * 1);
    holder->length = 1;
    holder->frames[0].delay_ms = 0;
    holder->frames[0].image = ximage_from_data(
      display, visual,
      png->data,
      png->width, png->height,
      0, 0,
      png->width, png->height
    );
  }

  return holder;
}

void image_holder_free(image_holder_t *holder) {
  if (holder == NULL)
    return;

  for (int idx = 0; idx < holder->length; idx++) {
    if (holder->frames[idx].image != NULL) {
      XDestroyImage(holder->frames[idx].image);
    }
  }

  free(holder->frames);
  free(holder);
}

/** Private API **/

int processEvent(Display *display, Window window, GC gc, image_holder_t *holder);
void run_window_with_image_holder(image_holder_t *holder, Display *display, XVisualInfo vinfo);

/** Public API **/

void show_decoded_png(png_t *png) {
  Display *display = XOpenDisplay(NULL);
  XVisualInfo vinfo;
  XMatchVisualInfo(display, DefaultScreen(display), 32, TrueColor, &vinfo);

  image_holder_t *holder = image_holder_from_png(png, display, vinfo.visual);
  if (holder == NULL) {
    printf("Failed to convert PNG to image_holder_t\n");
    return;
  }

  run_window_with_image_holder(holder, display, vinfo);
  image_holder_free(holder);
}

void show_image(animated_image_t *image) {

}

void show_decoded_gif(gif_decoded_t *gif) {

}


/** Private **/

int processEvent(Display *display, Window window, GC gc, image_holder_t *holder) {
  XEvent ev;
  XNextEvent(display, &ev);

  switch(ev.type) {
  case Expose:
    XPutImage(display, window, gc, holder->frames[0].image, 0, 0, 0, 0, holder->width, holder->height);
    break;
  case ButtonPress:
    return 1;
  }

  return 0;
}

void run_window_with_image_holder(image_holder_t *holder, Display *display, XVisualInfo vinfo) {
  XSetWindowAttributes attr;
  attr.colormap = XCreateColormap(display, DefaultRootWindow(display), vinfo.visual, AllocNone);
  attr.border_pixel = 0;
  attr.background_pixel = 0;

  Window window = XCreateWindow(
    display,
    DefaultRootWindow(display),
    0,
    0,
    holder->width,
    holder->height,
    0,
    vinfo.depth,
    InputOutput,
    vinfo.visual,
    CWColormap | CWBorderPixel | CWBackPixel,
    &attr
  );

  GC gc = XCreateGC(display, window, 0, 0);
  XSelectInput(display, window, ButtonPressMask|ExposureMask);
  XMapWindow(display, window);

  int should_exit = 0;
  while (should_exit == 0) {
    should_exit = processEvent(display, window, gc, holder);
  }

  XDestroyWindow(display, window);
}
