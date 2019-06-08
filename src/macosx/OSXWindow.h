#import <Cocoa/Cocoa.h>

// @class OSXWindowFrameView;

@interface OSXWindow : NSWindow<NSWindowDelegate>
{
    NSView  *childContentView;
}

@end
