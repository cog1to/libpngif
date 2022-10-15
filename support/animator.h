#import <Cocoa/Cocoa.h>
#import <image.h>

@interface Animator: NSObject
  -(id)initWithDecodedGif:(gif_decoded_t *)gif;
  -(id)initWithImage:(animated_image_t *)image;
  -(void)setImageView:(NSImageView *)imageView;
  -(void)start;
@end
