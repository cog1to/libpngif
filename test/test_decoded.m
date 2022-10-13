#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <errors.h>
#include <gif_parsed.h>
#include <gif_decoded.h>

#import "Cocoa/Cocoa.h"
#import "animator.h"

int max_wsize(int a, int b);
void show_decoded_image(gif_decoded_t *gif);
NSArray *images_from_decoded(gif_decoded_t *gif);

int main(int argc, char **argv) {
  if (argc < 2) {
    printf("Usage: %s <filepath>\n", argv[0]);
    return 0;
  }

  int error = 0;
  gif_parsed_t *raw = gif_parsed_from_file(argv[1], &error);

  if (error != 0 || raw == NULL) {
    printf("File read error: %d\n", error);
    return -1;
  }

  gif_decoded_t *dec = gif_decoded_from_parsed(raw, &error);

  if (error != 0 || dec == NULL) {
    printf("Decoding error: %d\n", error);
    return -1;
  }

  show_decoded_image(dec);
  gif_decoded_free(dec);

  return 0;
}

/** Private **/

int max_wsize(int a, int b) {
  if (a > b)
    return a;
  return b;
}

static const int border = 20;

void show_decoded_image(gif_decoded_t *gif) {
  NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];

  [NSApplication sharedApplication];

  NSUInteger windowStyle = NSWindowStyleMaskTitled
    | NSWindowStyleMaskClosable
    | NSWindowStyleMaskResizable;

  int wwidth = max_wsize(100, gif->width + border);
  int wheight = max_wsize(100, gif->height + border);

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

  NSRect imageRect = NSMakeRect(
    (wwidth - gif->width) / 2.0,
    (wheight - gif->height) / 2.0,
    gif->width,
    gif->height
  );

  NSImageView *imageView = [[[NSImageView alloc] initWithFrame:imageRect] autorelease];
  [[window contentView] addSubview:imageView];

  Animator *animator = [[Animator alloc] initWithDecodedGif:(gif)];
  [animator setImageView: imageView];
  [animator start];

  // Show window and run event loop.
  [window orderFrontRegardless];
  [NSApp run];

  [pool drain];
}

