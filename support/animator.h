#import <Cocoa/Cocoa.h>
#import <pngif/image.h>

@interface Animator: NSObject
  -(id)initWithDecodedGif:(gif_decoded_t *)gif;
  -(id)initWithDecodedPng:(png_decoded_t *)png;
  -(id)initWithImage:(animated_image_t *)image;
  -(void)setImageView:(NSImageView *)imageView;
  -(void)start;
@end

