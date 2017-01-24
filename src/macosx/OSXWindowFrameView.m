#import "OSXWindowFrameView.h"

@implementation OSXWindowFrameView

extern void* g_updateBuffer;
extern int g_width;
extern int g_height;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

- (NSRect)resizeRect
{
	const CGFloat resizeBoxSize = 16.0;
	const CGFloat contentViewPadding = 5.5;
	
	NSRect contentViewRect = [[self window] contentRectForFrameRect:[[self window] frame]];
	NSRect resizeRect = NSMakeRect(
		NSMaxX(contentViewRect) + contentViewPadding,
		NSMinY(contentViewRect) - resizeBoxSize - contentViewPadding,
		resizeBoxSize,
		resizeBoxSize);
	
	return resizeRect;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

- (void)drawRect:(NSRect)rect
{
	(void)rect;

	if (!g_updateBuffer)
		return;

	CGContextRef context = [[NSGraphicsContext currentContext] graphicsPort];

	CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
	CGDataProviderRef provider = CGDataProviderCreateWithData(NULL, g_updateBuffer, g_width * g_height * 4, NULL); 

	CGImageRef img = CGImageCreate(g_width, g_height, 8, 32, g_width * 4, space, kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder32Little, 
								   provider, NULL, false, kCGRenderingIntentDefault);

	CGColorSpaceRelease(space);
	CGDataProviderRelease(provider);

	CGContextDrawImage(context, CGRectMake(0, 0, g_width, g_height), img);

	CGImageRelease(img);
}

@end

