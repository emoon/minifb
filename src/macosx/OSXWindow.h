#import <Cocoa/Cocoa.h>

// @class OSXWindowFrameView;

@interface OSXWindow : NSWindow
{
	NSView* childContentView;
	@public bool closed;
#if defined(USE_METAL_API)
	@public int width;
	@public int height;
	@public void* draw_buffer;
#endif
}

@end
