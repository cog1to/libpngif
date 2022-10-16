#import <Cocoa/Cocoa.h>

#import "image.h"
#import "animator.h"
#import "appdelegate.h"

int max_wsize(int a, int b) {
  if (a > b)
    return a;
  return b;
}

void window_with_image_animator(int width, int height, Animator *animator) {
  NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];

  NSApplication *app = [NSApplication sharedApplication];

  NSUInteger windowStyle = NSWindowStyleMaskTitled
    | NSWindowStyleMaskClosable
    | NSWindowStyleMaskResizable;

  int border = 10;
  int wwidth = max_wsize(100, width + border);
  int wheight = max_wsize(100, height + border);

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
  ViewerAppDelegate *appDelegate = [[[ViewerAppDelegate alloc] init] autorelease];
  [app setDelegate: appDelegate];

  NSRect imageRect = NSMakeRect(
    (wwidth - width) / 2.0,
    (wheight - height) / 2.0,
    width,
    height
  );

  NSImageView *imageView = [[[NSImageView alloc] initWithFrame:imageRect] autorelease];
  [[window contentView] addSubview:imageView];

  // Bind animator to image view and start animation.
  [animator setImageView: imageView];
  [animator start];

  // Show window and run event loop.
  [window orderFrontRegardless];
  [NSApp run];

  // Clear memory after app close.
  [pool drain];
}

void show_image(animated_image_t *image) {
  Animator *animator = [[[Animator alloc] initWithImage:image] autorelease];
  window_with_image_animator(image->width, image->height, animator);
}

void show_decoded_gif(gif_decoded_t *gif) {
  Animator *animator = [[[Animator alloc] initWithDecodedGif:gif] autorelease];
  window_with_image_animator(gif->width, gif->height, animator);
}
