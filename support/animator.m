#import <Cocoa/Cocoa.h>
#import <pngif/gif_decoded.h>
#import <pngif/image.h>
#import <animator.h>

/** Image data **/

@interface ImageHolder: NSObject

@property (nonatomic, retain) NSImage *image;
@property (nonatomic) CGFloat delayCS;

-(id)initWithImage:(NSImage*)image delay:(NSTimeInterval)delay;

@end

@implementation ImageHolder

@synthesize image;
@synthesize delayCS;

-(id)initWithImage:(NSImage*)i delay:(NSTimeInterval)d {
  if (self = [super init]) {
    self.image = i;
    self.delayCS = d;
  }
  return self;
}

@end

/** Animator helper **/

@implementation Animator {
  NSArray<ImageHolder*> *images;
  NSImageView *imageView;
  int frame_index;
  BOOL running;
}

-(id)init {
  self = [super init];
  return self;
}

-(id)initWithDecodedGif:(gif_decoded_t *)gif {
  if (self = [super init]) {
    if (gif == NULL) {
      return self;
    }

    if (gif->image_count == 0 || gif->images == NULL) {
      return self;
    }

    NSMutableArray *output = [[[NSMutableArray alloc] init] autorelease];

    for (int idx = 0; idx < gif->image_count; idx++) {
      gif_decoded_image_t img = gif->images[idx];

      NSBitmapImageRep *newRep = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:&img.rgba
        pixelsWide:img.width
        pixelsHigh:img.height
        bitsPerSample:8
        samplesPerPixel:4
        hasAlpha:YES
        isPlanar:NO
        colorSpaceName:NSDeviceRGBColorSpace
        bytesPerRow:4 * img.width
        bitsPerPixel:32];

      NSImage* image = [[NSImage alloc] initWithSize:NSMakeSize(img.width,  img.height)];
      [image addRepresentation: newRep];

      ImageHolder *holder = [[ImageHolder alloc] initWithImage:image delay: img.delay_cs];

      [output addObject: holder];
    }

    images = output;
    frame_index = 0;
    running = NO;
  }
  return self;
}

-(id)initWithDecodedPng:(png_decoded_t *)png {
  if (self = [super init]) {
    if (png == NULL) {
      return self;
    }

    NSMutableArray *output = [[[NSMutableArray alloc] init] autorelease];

    if (png->frames == NULL || png->frames->length == 0) {
      // Non-animated.
      NSBitmapImageRep *newRep = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:&png->data
        pixelsWide:png->width
        pixelsHigh:png->height
        bitsPerSample:8
        samplesPerPixel:4
        hasAlpha:YES
        isPlanar:NO
        colorSpaceName:NSDeviceRGBColorSpace
        bytesPerRow:4 * png->width
        bitsPerPixel:32];

      NSImage* image = [[NSImage alloc] initWithSize:NSMakeSize(png->width, png->height)];
      [image addRepresentation: newRep];

      ImageHolder *holder = [[ImageHolder alloc] initWithImage:image delay:0];

      [output addObject: holder];
    } else {
      for (int idx = 0; idx < png->frames->length; idx++) {
        png_frame_t frame = png->frames->frames[idx];

        NSBitmapImageRep *newRep = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:&frame.data
          pixelsWide:frame.width
          pixelsHigh:frame.height
          bitsPerSample:8
          samplesPerPixel:4
          hasAlpha:YES
          isPlanar:NO
          colorSpaceName:NSDeviceRGBColorSpace
          bytesPerRow:4 * frame.width
          bitsPerPixel:32];

        NSImage* image = [[NSImage alloc] initWithSize:NSMakeSize(frame.width,  frame.height)];
        [image addRepresentation: newRep];

        ImageHolder *holder = [[ImageHolder alloc] initWithImage:image delay:(frame.delay * 100)];

        [output addObject: holder];
      }
    }

    images = output;
    frame_index = 0;
    running = NO;
  }
  return self;
}

-(id)initWithImage:(animated_image_t *)image {
  if (self = [super init]) {
    if (image == NULL) {
      return self;
    }

    if (image->frame_count == 0 || image->frames == NULL) {
      return self;
    }

    NSMutableArray *output = [[[NSMutableArray alloc] init] autorelease];

    for (int idx = 0; idx < image->frame_count; idx++) {
      image_frame_t frame = image->frames[idx];

      NSBitmapImageRep *newRep = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:&frame.rgba
        pixelsWide:image->width
        pixelsHigh:image->height
        bitsPerSample:8
        samplesPerPixel:4
        hasAlpha:YES
        isPlanar:NO
        colorSpaceName:NSDeviceRGBColorSpace
        bytesPerRow:4 * image->width
        bitsPerPixel:32];

      NSImage* nsimage = [[NSImage alloc] initWithSize:NSMakeSize(image->width, image->height)];
      [nsimage addRepresentation: newRep];

      ImageHolder *holder = [[ImageHolder alloc] initWithImage:nsimage delay: frame.duration_ms / 10];

      [output addObject: holder];
    }

    images = output;
    frame_index = 0;
    running = NO;
  }
  return self;
}

-(void)setImageView:(NSImageView *)iv {
  imageView = iv;
}

-(void)start {
  if (imageView == NULL || images == NULL || [images count] == 0) {
    return;
  }

  imageView.image = [images objectAtIndex: 0].image;

  if ([images count] == 1) {
    return;
  }

  running = YES;

  dispatch_after(
    dispatch_time(DISPATCH_TIME_NOW, [images objectAtIndex: 0].delayCS / 100.0f * NSEC_PER_SEC),
    dispatch_get_main_queue(), ^{
      [self next];
    }
  );
}

-(void)next {
  if (!running) {
    return;
  }

  frame_index += 1;
  if (frame_index == [images count]) {
    frame_index = 0;
  }

  ImageHolder *holder = [images objectAtIndex: frame_index];
  imageView.image = holder.image;

  dispatch_after(
    dispatch_time(DISPATCH_TIME_NOW, holder.delayCS / 100.0f * NSEC_PER_SEC),
    dispatch_get_main_queue(), ^{
      [self next];
    }
  );
}

-(void)stop {
  frame_index = 0;
  running = NO;
}

@end
