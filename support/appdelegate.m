#import <Cocoa/Cocoa.h>

#import "appdelegate.h"

@implementation ViewerAppDelegate

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
  return YES;
}

@end


