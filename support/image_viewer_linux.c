#include <stdlib.h>
#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/signalfd.h>
#include <sys/timerfd.h>
#include <stdarg.h>

#include "png_decoded.h"
#include "gif_decoded.h"
#include "image.h"
#include "png_util.h"

/** Private **/

int max_fd(int count, ...) {
  va_list args;
  int max = 0, cur = 0;
  va_start(args, count);
  for (int idx = 0; idx < count; idx++) {
    cur = va_arg(args, int);
    if (cur > max) {
      max = cur;
    }
  }
  va_end(args);
  return max;
}

void draw_subimage(
  unsigned char *dest,
  unsigned char *source,
  int32_t canvas_width, int32_t canvas_height,
  int32_t offset_x, int32_t offset_y,
  int32_t width, int32_t height,
  int transparent
) {
  // XLib wants BGRA data instead of RGBA.
  unsigned char *bgra = malloc(width * height * 4);
  rgba_to_bgra(source, bgra, width, height);

  for (int row = 0; row < height; row++) {
    for (int col = 0; col < width; col++) {
      int32_t canvas_offset = (canvas_width * (offset_y + row) * 4) + (offset_x + col) * 4;
      int32_t offset = (row * width + col) * 4;
      // X11 can't work with 32-bit color space properly (or I'm dumb and can't
      // find appropriate info on it), so here we do a color transform: for each
      // pixel we explicitly multiply it by its alpha value to get it
      // proportionally closer to the window's 'background pixel', which is set
      // to 0x00000000 on window's creation.
      //
      // This should not be required on systems that do support ARGB properly,
      // they should be able to just take and apply pixel's alpha value.
      if (bgra[offset + 3] != 0) {
        if (transparent) {
          dest[canvas_offset + 0] = (u_int16_t)(bgra[offset + 0]) * bgra[offset + 3] / 255;
          dest[canvas_offset + 1] = (u_int16_t)(bgra[offset + 1]) * bgra[offset + 3] / 255;
          dest[canvas_offset + 2] = (u_int16_t)(bgra[offset + 2]) * bgra[offset + 3] / 255;
          dest[canvas_offset + 3] = bgra[offset + 3];
        } else {
          dest[canvas_offset + 0] = (u_int16_t)(bgra[offset + 0]) * bgra[offset + 3] / 255
            + (u_int16_t)(dest[canvas_offset + 0]) * (255 - bgra[offset + 3]) / 255;
          dest[canvas_offset + 1] = (u_int16_t)(bgra[offset + 1]) * bgra[offset + 3] / 255
            + (u_int16_t)(dest[canvas_offset + 1]) * (255 - bgra[offset + 3]) / 255;
          dest[canvas_offset + 2] = (u_int16_t)(bgra[offset + 2]) * bgra[offset + 3] / 255
            + (u_int16_t)(dest[canvas_offset + 2]) * (255 - bgra[offset + 3]) / 255;
        }
      }
    }
  }

  free(bgra);
}

XImage *ximage_from_data(
  Display *display, Visual *visual,
  unsigned char *image,
  int32_t canvas_width, int32_t canvas_height,
  int32_t offset_x, int32_t offset_y,
  int32_t width, int32_t height,
  int transparent
) {
  // Set up canvas.
  unsigned char *image32 = (unsigned char *)calloc(canvas_width * canvas_height * 4, 1);

  // Paint white if transparency is disabled.
  if (transparent == 0)
    memset(image32, 255, canvas_width * canvas_height * 4);

  // Draw image to the canvas.
  draw_subimage(image32, image, canvas_width, canvas_height, offset_x, offset_y, width, height, transparent);

  // Create XImage.
  return XCreateImage(display, visual, 32, ZPixmap, 0, (char *)image32, canvas_width, canvas_height, 32, 0);
}

XImage *ximage_from_tiled_gif(
  Display *display, Visual *visual,
  gif_decoded_t *gif,
  int transparent
) {
  // Set up canvas.
  unsigned char *image32 = (unsigned char *)calloc(gif->width * gif->height * 4, 1);

  // Paint white if transparency is disabled.
  if (transparent == 0)
    memset(image32, 255, gif->width * gif->height * 4);

  // Draw sub-images to the canvas.
  for (int idx = 0; idx<gif->image_count; idx++) {
    gif_decoded_image_t image = gif->images[idx];
    draw_subimage(image32, image.rgba, gif->width, gif->height, image.left, image.top, image.width, image.height, transparent);
  }

  // Create XImage.
  return XCreateImage(display, visual, 32, ZPixmap, 0, (char *)image32, gif->width, gif->height, 32, 0);
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

  holder->width = png->width;
  holder->height = png->height;
  holder->index = 0;
  holder->running = 0;

  if (png->frames != NULL && png->frames->length > 0) {
    holder->frames = malloc(sizeof(image_holder_frame_t) * png->frames->length);
    holder->length = png->frames->length;

    for (int idx = 0; idx < png->frames->length; idx++) {
      png_frame_t *frame = png->frames->frames + idx;
      holder->frames[idx].delay_ms = frame->delay * 1000;
      holder->frames[idx].image = ximage_from_data(
        display, visual,
        frame->data,
        png->width, png->height,
        frame->x_offset, frame->y_offset,
        frame->width, frame->height,
        1
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
      png->width, png->height,
      1
    );
  }

  return holder;
}

image_holder_t *image_holder_from_gif(gif_decoded_t *gif, Display *display, Visual *visual) {
  if (gif->image_count == 0 || gif->images == NULL) {
    return NULL;
  }

  image_holder_t *holder = malloc(sizeof(image_holder_t));
  if (holder == NULL) {
    return NULL;
  }

  holder->width = gif->width;
  holder->height = gif->height;
  holder->index = 0;
  holder->running = 0;

  if (gif->animated) {
    holder->frames = malloc(sizeof(image_holder_frame_t) * gif->image_count);
    holder->length = gif->image_count;

    for (int idx = 0; idx < gif->image_count; idx++) {
      gif_decoded_image_t img = gif->images[idx];

      holder->frames[idx].delay_ms = img.delay_cs * 10;
      holder->frames[idx].image = ximage_from_data(
        display, visual,
        img.rgba,
        gif->width, gif->height,
        img.left, img.top,
        img.width, img.height,
        1
      );
    }
  } else {
    holder->frames = malloc(sizeof(image_holder_frame_t) * 1);
    holder->length = 1;
    holder->frames[0].delay_ms = 0;
    holder->frames[0].image = ximage_from_tiled_gif(
      display, visual,
      gif,
      1
    );
  }

  return holder;
}

void image_holder_set_frame_delay(image_holder_t *holder, struct itimerspec *spec, int index) {
  if (holder->length == 0) {
    spec->it_value.tv_sec = 0;
    spec->it_value.tv_nsec = 0;
    spec->it_interval.tv_sec = 0;
    spec->it_interval.tv_nsec = 0;
  } else {
    spec->it_value.tv_sec = holder->frames[index].delay_ms / 1000;
    spec->it_value.tv_nsec = (holder->frames[index].delay_ms % 1000) * 1000000;
    spec->it_interval.tv_sec = 0;
    spec->it_interval.tv_nsec = 0;
  }
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

int process_event(Display *display, Window window, GC gc, image_holder_t *holder);
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
  Display *display = XOpenDisplay(NULL);
  XVisualInfo vinfo;
  XMatchVisualInfo(display, DefaultScreen(display), 32, TrueColor, &vinfo);

  image_holder_t *holder = image_holder_from_gif(gif, display, vinfo.visual);
  if (holder == NULL) {
    printf("Failed to convert GIF to image_holder_t\n");
    return;
  }

  run_window_with_image_holder(holder, display, vinfo);
  image_holder_free(holder);
}

/** Private **/

int process_event(Display *display, Window window, GC gc, image_holder_t *holder) {
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

  XClearWindow(display, window);
  XFlush(display);

  /* Event sources setup */
  int should_exit = 0;
  fd_set readfds;

  /* X connection FD */
  int connection_fd = ConnectionNumber(display);

  /* Animation timer */
  int timer_fd = timerfd_create(CLOCK_REALTIME, 0);
  struct itimerspec frame_delay;
  image_holder_set_frame_delay(holder, &frame_delay, 0);
  // Initial frame delay.
  timerfd_settime(timer_fd, 0, &frame_delay, NULL);

  /* Max FD value for pselect */
  int maxfd = max_fd(2, connection_fd, timer_fd);

  while (should_exit == 0) {
    FD_ZERO(&readfds);
    FD_SET(connection_fd, &readfds);
    FD_SET(timer_fd, &readfds);

    int activity = pselect(maxfd+1, &readfds, NULL, NULL, NULL, NULL);
    if (activity == -1) {
      perror("Error while waiting for the input");
      break;
    }

    /* Animation timer */
    if (FD_ISSET(timer_fd, &readfds)) {
      // 1. Increase frame index.
      holder->index = (holder->index + 1) % holder->length;
      // 2. Redraw.
      XPutImage(display, window, gc, holder->frames[holder->index].image, 0, 0, 0, 0, holder->width, holder->height);
      // 3. Set timer to next frame.
      image_holder_set_frame_delay(holder, &frame_delay, holder->index);
      timerfd_settime(timer_fd, 0, &frame_delay, NULL);
    }

    /* XEvents handling */
    if (FD_ISSET(connection_fd, &readfds)) {
      while (XPending(display)) {
        should_exit = process_event(display, window, gc, holder);
      }
    }
  }

  XDestroyWindow(display, window);
}
