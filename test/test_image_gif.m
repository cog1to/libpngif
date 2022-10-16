#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <errors.h>
#include <gif_parsed.h>
#include <gif_decoded.h>
#include <image.h>

#import "Cocoa/Cocoa.h"
#import "animator.h"
#import "appdelegate.h"

int max_wsize(int a, int b);
void show_image(animated_image_t *gif);

int main(int argc, char **argv) {
  if (argc < 2) {
    printf("Usage: %s [-b] <filepath>\n", argv[0]);
    printf("Options:\n  -b: Ignore background color\n");
    return 0;
  }

  int ignore_background = 0;
  if (argc > 2) {
    if (strcmp(argv[1], "-b") == 0) {
      ignore_background = 1;
    } else {
      printf("Unknown option: %s", argv[1]);
    }
  }

  int error = 0;
  gif_parsed_t *raw = gif_parsed_from_file(argv[argc - 1], &error);

  if (error != 0 || raw == NULL) {
    printf("File read error: %d\n", error);
    return -1;
  }

  gif_decoded_t *dec = gif_decoded_from_parsed(raw, &error);

  if (error != 0 || dec == NULL) {
    printf("Decoding error: %d\n", error);
    return -1;
  }

  animated_image_t *image = image_from_decoded_gif(dec, ignore_background, &error);

  if (error != 0 || image == NULL) {
    printf("Image drawing error: %d\n", error);
    return -1;
  }
  
  show_image(image);

  gif_parsed_free(raw);
  gif_decoded_free(dec);
  animated_image_free(image);

  return 0;
}

/** Private **/

int max_wsize(int a, int b) {
  if (a > b)
    return a;
  return b;
}

static const int border = 20;

void show_image(animated_image_t *image) {
  NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];

  [NSApplication sharedApplication];

  NSUInteger windowStyle = NSWindowStyleMaskTitled
    | NSWindowStyleMaskClosable
    | NSWindowStyleMaskResizable;

  int wwidth = max_wsize(100, image->width + border);
  int wheight = max_wsize(100, image->height + border);

  NSRect windowRect = NSMakeRect(
    100,
    100,
    wwidth,
    wheight
  );
  NSWindow * window = [[NSWindow alloc] initWithContentRect:windowRect
                                        styleMask:windowStyle
                                        backing:NSBackingStoreBuffered
                                        defer:NO];
  [window autorelease];

  NSWindowController * windowController = [[NSWindowController alloc] initWithWindow:window];
  [windowController autorelease];

  // Set up AppDelegate. Right now used only to properly close the app when
  // window is closed.
  ViewerAppDelegate *appDelegate = [[ViewerAppDelegate alloc] init];
  [[NSApplication sharedApplication] setDelegate: appDelegate];
  [appDelegate autorelease];

  NSRect imageRect = NSMakeRect(
    (wwidth - image->width) / 2.0,
    (wheight - image->height) / 2.0,
    image->width,
    image->height
  );

  NSImageView *imageView = [[[NSImageView alloc] initWithFrame:imageRect] autorelease];
  [[window contentView] addSubview:imageView];

  Animator *animator = [[Animator alloc] initWithImage:image];
  [animator setImageView: imageView];
  [animator start];

  // Show window and run event loop.
  [window orderFrontRegardless];
  [NSApp run];

  [pool drain];
}

