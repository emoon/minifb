#import "OSXView.h"
#import "OSXWindow.h"
#import "WindowData_OSX.h"
#include <MiniFB_internal.h>

//-------------------------------------
@implementation OSXView

#if defined(USE_METAL_API)

//-------------------------------------
- (void)updateTrackingAreas {
    if(tracking_area != nil) {
        [self removeTrackingArea:tracking_area];
        [tracking_area release];
    }

    int opts = (NSTrackingMouseEnteredAndExited | NSTrackingActiveAlways);
    tracking_area = [[NSTrackingArea alloc] initWithRect:[self bounds]
                                                 options:opts
                                                   owner:self
                                                userInfo:nil];
    [self addTrackingArea:tracking_area];
}

#else

//-------------------------------------
- (NSRect)resizeRect {
	const CGFloat resizeBoxSize = 16.0;
	const CGFloat contentViewPadding = 5.5;

	NSRect contentViewRect = [[self window] contentRectForFrameRect:[[self window] frame]];
	NSRect resizeRect = NSMakeRect(
		NSMaxX(contentViewRect) + contentViewPadding,
		NSMinY(contentViewRect) - resizeBoxSize - contentViewPadding,
		resizeBoxSize,
		resizeBoxSize
    );

	return resizeRect;
}

//-------------------------------------
- (void)drawRect:(NSRect)rect {
	(void)rect;

    if(window_data == 0x0)
        return;

    SWindowData_OSX *window_data_osx = (SWindowData_OSX *) window_data->specific;
	if (!window_data_osx || !window_data_osx->window || !window_data->draw_buffer)
		return;

	CGContextRef context = [[NSGraphicsContext currentContext] graphicsPort];

	CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
	CGDataProviderRef provider = CGDataProviderCreateWithData(0x0, window_data->draw_buffer, window_data->buffer_width * window_data->buffer_height * 4, 0x0);

	CGImageRef img = CGImageCreate(window_data->buffer_width, window_data->buffer_height, 8, 32, window_data->buffer_width * 4, space, kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder32Little,
								   provider, 0x0, false, kCGRenderingIntentDefault);

    const CGFloat components[] = {0.0f, 0.0f, 0.0f, 1.0f};
    const CGColorRef black = CGColorCreate(space, components);

	CGColorSpaceRelease(space);
	CGDataProviderRelease(provider);

    if(window_data->dst_offset_x != 0 || window_data->dst_offset_y != 0 || window_data->dst_width != window_data->window_width || window_data->dst_height != window_data->window_height) {
        CGContextSetFillColorWithColor(context, black);
        CGContextFillRect(context, CGRectMake(0, 0, window_data->window_width, window_data->window_height));
    }

	CGContextDrawImage(context, CGRectMake(window_data->dst_offset_x, window_data->dst_offset_y, window_data->dst_width, window_data->dst_height), img);

	CGImageRelease(img);
}

#endif

//-------------------------------------
- (BOOL)acceptsFirstMouse:(NSEvent *)event {
    (void)event;
    return YES;
}

//-------------------------------------
- (void)mouseDown:(NSEvent*)event {
    (void)event;
    if(window_data != 0x0) {
        kCall(mouse_btn_func, MOUSE_BTN_1, window_data->mod_keys, true);
    }
}

//-------------------------------------
- (void)mouseUp:(NSEvent*)event {
    (void)event;
    if(window_data != 0x0) {
        kCall(mouse_btn_func, MOUSE_BTN_1, window_data->mod_keys, false);
    }
}

//-------------------------------------
- (void)rightMouseDown:(NSEvent*)event {
    (void)event;
    if(window_data != 0x0) {
        kCall(mouse_btn_func, MOUSE_BTN_2, window_data->mod_keys, true);
    }
}

//-------------------------------------
- (void)rightMouseUp:(NSEvent*)event {
    (void)event;
    if(window_data != 0x0) {
        kCall(mouse_btn_func, MOUSE_BTN_1, window_data->mod_keys, false);
    }
}

//-------------------------------------
- (void)otherMouseDown:(NSEvent *)event {
    (void)event;
    if(window_data != 0x0) {
        kCall(mouse_btn_func, [event buttonNumber], window_data->mod_keys, true);
    }
}

//-------------------------------------
- (void)otherMouseUp:(NSEvent *)event {
    (void)event;
    if(window_data != 0x0) {
        kCall(mouse_btn_func, [event buttonNumber], window_data->mod_keys, false);
    }
}

//-------------------------------------
- (void)scrollWheel:(NSEvent *)event {
    if(window_data != 0x0) {
        kCall(mouse_wheel_func, window_data->mod_keys, [event deltaX], [event deltaY]);
    }
}

//-------------------------------------
- (void)mouseDragged:(NSEvent *)event {
    [self mouseMoved:event];
}

//-------------------------------------
- (void)rightMouseDragged:(NSEvent *)event {
    [self mouseMoved:event];
}

//-------------------------------------
- (void)otherMouseDragged:(NSEvent *)event {
    [self mouseMoved:event];
}

//-------------------------------------
- (void)mouseMoved:(NSEvent *)event {
    if(window_data != 0x0) {
        NSPoint point = [event locationInWindow];
        //NSPoint localPoint = [self convertPoint:point fromView:nil];
        window_data->mouse_pos_x = point.x;
        window_data->mouse_pos_y = point.y;
        kCall(mouse_move_func, point.x, point.y);
    }
}

//-------------------------------------
- (void)mouseExited:(NSEvent *)event {
    (void)event;
    //printf("mouse exit\n");
}

//-------------------------------------
- (void)mouseEntered:(NSEvent *)event {
    (void)event;
    //printf("mouse enter\n");
}

//-------------------------------------
- (BOOL)canBecomeKeyView {
    return YES;
}

//-------------------------------------
- (NSView *)nextValidKeyView {
    return self;
}

//-------------------------------------
- (NSView *)previousValidKeyView {
    return self;
}

//-------------------------------------
- (BOOL)acceptsFirstResponder {
    return YES;
}

//-------------------------------------
- (void)viewDidMoveToWindow {
}

//-------------------------------------
- (void)dealloc {
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    [super dealloc];
}

@end

