#import <Cocoa/Cocoa.h>

@interface Animator: NSObject
  -(id)initWithDecodedGif:(gif_decoded_t *)gif;
  -(void)setImageView:(NSImageView *)imageView;
  -(void)start;
@end

