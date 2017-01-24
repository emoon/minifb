#import <Cocoa/Cocoa.h>

// @class OSXWindowFrameView;

@interface OSXWindow : NSWindow
{
	NSView* childContentView;
	@public bool closed;
}

@end
