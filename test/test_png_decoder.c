#include <stdlib.h>
#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <string.h>

#include "util.h"
#include "raw.h"
#include "parser.h"
#include "decoder.h"

void show_image(unsigned char *png, size_t width, size_t height);
XImage *create_ximage(Display *display, Visual *visual, unsigned char *image, int width, int height);
int processEvent(Display *display, Window window, GC gc, XImage *ximage, int width, int height);

int main(int argc, char **argv) {
  if (argc < 2) {
    printf("Usage: %s <filename.png>\n", argv[0]);
    return 0;
  }

  int error = 0;
  png_raw_t *raw = png_raw_read_file(argv[1], 0, &error);
  if (raw == NULL || error != 0) {
    printf("Failed to parse file: %d.\n", error);
    return 0;
  }

  png_parsed_t *parsed = png_create_from_raw(raw, &error);
  if (parsed == NULL || error != 0) {
    printf("Failed to decode PNG chunks: %d.\n", error);
    return 0;
  }

  png_t *png = png_create_from_parsed(parsed, 0, &error);
  if (parsed == NULL || error != 0) {
    printf("Failed to decode PNG data: %d.\n", error);
    return 0;
  }

  // XLib wants BGRA image format, so convert it.
  rgba_to_bgra(png->data, png->data, png->width, png->height);

  show_image(png->data, png->width, png->height);
  png_raw_free(raw);
  png_parsed_free(parsed);
  png_free(png);
}

/** Private **/

XImage *create_ximage(Display *display, Visual *visual, unsigned char *image, int width, int height) {
  unsigned char *image32 = (unsigned char *)malloc(width * height * 4);
  for (int row = 0; row < height; row++) {
    for (int col = 0; col < width; col++) {
      int offset = (row * width + col) * 4;
      // X11 can't work with 32-bit color space properly (or I'm dumb and can't
      // find appropriate info on it), so here we do a color transform: for each
      // pixel we explicitly multiply it by its alpha value to get it
      // proportionally closer to the window's 'background pixel', which is set
      // to 0x00000000 on window's creation.
      //
      // This should not be required on systems that do support ARGB properly,
      // they should be able to just take and apply pixel's alpha value.
      image32[offset + 0] = (u_int16_t)(image[offset + 0]) * image[offset + 3] / 255;
      image32[offset + 1] = (u_int16_t)(image[offset + 1]) * image[offset + 3] / 255;
      image32[offset + 2] = (u_int16_t)(image[offset + 2]) * image[offset + 3] / 255;
      image32[offset + 3] = image[offset + 3];
    }
  }

  return XCreateImage(display, visual, 32, ZPixmap, 0, (char *)image32, width, height, 32, 0);
}

int processEvent(Display *display, Window window, GC gc, XImage *ximage, int width, int height) {
  XEvent ev;
  XNextEvent(display, &ev);

  switch(ev.type) {
  case Expose:
    XPutImage(display, window, gc, ximage, 0, 0, 0, 0, width, height);
    break;
  case ButtonPress:
    return 1;
  }

  return 0;
}

void show_image(unsigned char *data, size_t width, size_t height) {
  XImage *ximage;

  Display *display = XOpenDisplay(NULL);

  XVisualInfo vinfo;
  XMatchVisualInfo(display, DefaultScreen(display), 32, TrueColor, &vinfo);

  XSetWindowAttributes attr;
  attr.colormap = XCreateColormap(display, DefaultRootWindow(display), vinfo.visual, AllocNone);
  attr.border_pixel = 0;
  attr.background_pixel = 0;

  Window window = XCreateWindow(
    display,
    DefaultRootWindow(display),
    0,
    0,
    width,
    height,
    0,
    vinfo.depth,
    InputOutput,
    vinfo.visual,
    CWColormap | CWBorderPixel | CWBackPixel,
    &attr
  );

  ximage = create_ximage(display, vinfo.visual, data, width, height);

  GC gc = XCreateGC(display, window, 0, 0);
  XSelectInput(display, window, ButtonPressMask|ExposureMask);
  XMapWindow(display, window);

  int should_exit = 0;
  while (should_exit == 0) {
    should_exit = processEvent(display, window, gc, ximage, width, height);
  }

  XDestroyImage(ximage);
  XDestroyWindow(display, window);
}
