#include <stdlib.h>
#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <string.h>

#include "util.h"
#include "raw.h"
#include "parser.h"
#include "decoder.h"

/** Private definitions **/

void show_frames(png_t *png);

XImage *create_ximage(
  Display *display,
  Visual *visual,
  unsigned char *image,
  u_int32_t canvas_width,
  u_int32_t canvas_height,
  u_int32_t offset_x,
  u_int32_t offset_y,
  u_int32_t image_width,
  u_int32_t image_height
);

int processEvent(Display *display, Window window, GC gc, XImage **ximage, int count, int width, int height);

/** Main **/

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

  show_frames(png);
  png_raw_free(raw);
  png_parsed_free(parsed);
  png_free(png);
}

/** Private **/

XImage *create_ximage(
  Display *display,
  Visual *visual,
  unsigned char *image,
  u_int32_t canvas_width,
  u_int32_t canvas_height,
  u_int32_t offset_x,
  u_int32_t offset_y,
  u_int32_t image_width,
  u_int32_t image_height
) {
  unsigned char *image32 = (unsigned char *)calloc(canvas_width * canvas_height * 4, 1);
  for (int row = 0; row < image_height; row++) {
    for (int col = 0; col < image_width; col++) {
      int out_offset = ((row + offset_y) * canvas_width + (offset_y + col)) * 4;
      int in_offset = (row * image_width + col) * 4;
      // X11 can't work with 32-bit color space properly (or I'm dumb and can't
      // find appropriate info on it), so here we do a color transform: for each
      // pixel we explicitly multiply it by its alpha value to get it
      // proportionally closer to the window's 'background pixel', which is set
      // to 0x00000000 on window's creation.
      //
      // This should not be required on systems that do support ARGB properly,
      // they should be able to just take and apply pixel's alpha value.
      image32[out_offset + 0] = (u_int16_t)(image[in_offset + 0]) * image[in_offset + 3] / 255;
      image32[out_offset + 1] = (u_int16_t)(image[in_offset + 1]) * image[in_offset + 3] / 255;
      image32[out_offset + 2] = (u_int16_t)(image[in_offset + 2]) * image[in_offset + 3] / 255;
      image32[out_offset + 3] = image[in_offset + 3];
    }
  }

  return XCreateImage(display, visual, 32, ZPixmap, 0, (char *)image32, canvas_width, canvas_height, 32, 0);
}

int processEvent(Display *display, Window window, GC gc, XImage **ximages, int count, int width, int height) {
  XEvent ev;
  XNextEvent(display, &ev);

  switch(ev.type) {
  case Expose:
    for (int idx = 0; idx < count; idx++) {
      XPutImage(display, window, gc, ximages[idx], 0, 0, idx * width, 0, width, height);
    }
    break;
  case ButtonPress:
    return 1;
  }

  return 0;
}

void show_frames(png_t *png) {
  XImage **ximages = NULL;

  Display *display = XOpenDisplay(NULL);

  XVisualInfo vinfo;
  XMatchVisualInfo(display, DefaultScreen(display), 32, TrueColor, &vinfo);

  XSetWindowAttributes attr;
  attr.colormap = XCreateColormap(display, DefaultRootWindow(display), vinfo.visual, AllocNone);
  attr.border_pixel = 0;
  attr.background_pixel = 0;

  u_int32_t width = png->width;
  if (png->frames != NULL) {
    width = png->frames->length * png->width;
  }

  Window window = XCreateWindow(
    display,
    DefaultRootWindow(display),
    0,
    0,
    width,
    png->height,
    0,
    vinfo.depth,
    InputOutput,
    vinfo.visual,
    CWColormap | CWBorderPixel | CWBackPixel,
    &attr
  );

  int count = 1;
  if (png->frames != NULL) {
    count = png->frames->length;
    ximages = malloc(png->frames->length * sizeof(XImage*));
    for (int idx = 0; idx < png->frames->length; idx++) {
      ximages[idx] = create_ximage(
        display,
        vinfo.visual,
        png->frames->frames[idx].data,
        png->width,
        png->height,
        png->frames->frames[idx].x_offset,
        png->frames->frames[idx].y_offset,
        png->frames->frames[idx].width,
        png->frames->frames[idx].height
      );
    }
  } else {
    ximages = malloc(sizeof(XImage*));
    ximages[0] = create_ximage(
      display,
      vinfo.visual,
      png->data,
      png->width,
      png->height,
      0,
      0,
      png->width,
      png->height
    );
  }

  GC gc = XCreateGC(display, window, 0, 0);
  XSelectInput(display, window, ButtonPressMask|ExposureMask);
  XMapWindow(display, window);

  int should_exit = 0;
  while (should_exit == 0) {
    should_exit = processEvent(display, window, gc, ximages, count, png->width, png->height);
  }

  for (int idx = 0; idx < count; idx++) {
    XDestroyImage(ximages[idx]);
  }
  free(ximages);
  XDestroyWindow(display, window);
}
