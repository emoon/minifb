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

    CGContextRef context = [[NSGraphicsContext currentContext] CGContext];

	CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
	CGDataProviderRef provider = CGDataProviderCreateWithData(0x0,
                                                              window_data->draw_buffer,
                                                              window_data->buffer_width * window_data->buffer_height * 4,
                                                              0x0
    );

	CGImageRef img = CGImageCreate(window_data->buffer_width
                                 , window_data->buffer_height
                                 , 8
                                 , 32
                                 , window_data->buffer_width * 4
                                 , space
                                 , kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder32Little
                                 , provider
                                 , 0x0
                                 , false
                                 , kCGRenderingIntentDefault
    );

    const CGFloat components[] = {0.0f, 0.0f, 0.0f, 1.0f};
    const CGColorRef black = CGColorCreate(space, components);

	CGColorSpaceRelease(space);
	CGDataProviderRelease(provider);

    if(window_data->dst_offset_x != 0 || window_data->dst_offset_y != 0 || window_data->dst_width != window_data->window_width || window_data->dst_height != window_data->window_height) {
        CGContextSetFillColorWithColor(context, black);
        CGContextFillRect(context, rect);
    }

    // TODO: Sometimes there is a crash here
	CGContextDrawImage(context,
                       CGRectMake(window_data->dst_offset_x, window_data->dst_offset_y, window_data->dst_width, window_data->dst_height),
                       img
    );

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
        window_data->mouse_button_status[MOUSE_BTN_1] = true;
        kCall(mouse_btn_func, MOUSE_BTN_1, window_data->mod_keys, true);
    }
}

//-------------------------------------
- (void)mouseUp:(NSEvent*)event {
    (void)event;
    if(window_data != 0x0) {
        window_data->mouse_button_status[MOUSE_BTN_1] = false;
        kCall(mouse_btn_func, MOUSE_BTN_1, window_data->mod_keys, false);
    }
}

//-------------------------------------
- (void)rightMouseDown:(NSEvent*)event {
    (void)event;
    if(window_data != 0x0) {
        window_data->mouse_button_status[MOUSE_BTN_2] = true;
        kCall(mouse_btn_func, MOUSE_BTN_2, window_data->mod_keys, true);
    }
}

//-------------------------------------
- (void)rightMouseUp:(NSEvent*)event {
    (void)event;
    if(window_data != 0x0) {
        window_data->mouse_button_status[MOUSE_BTN_2] = false;
        kCall(mouse_btn_func, MOUSE_BTN_2, window_data->mod_keys, false);
    }
}

//-------------------------------------
- (void)otherMouseDown:(NSEvent *)event {
    (void)event;
    if(window_data != 0x0) {
        window_data->mouse_button_status[[event buttonNumber] & 0x07] = true;
        kCall(mouse_btn_func, [event buttonNumber], window_data->mod_keys, true);
    }
}

//-------------------------------------
- (void)otherMouseUp:(NSEvent *)event {
    (void)event;
    if(window_data != 0x0) {
        window_data->mouse_button_status[[event buttonNumber] & 0x07] = false;
        kCall(mouse_btn_func, [event buttonNumber], window_data->mod_keys, false);
    }
}

//-------------------------------------
- (void)scrollWheel:(NSEvent *)event {
    if(window_data != 0x0) {
        window_data->mouse_wheel_x = [event deltaX];
        window_data->mouse_wheel_y = [event deltaY];
        kCall(mouse_wheel_func, window_data->mod_keys, window_data->mouse_wheel_x, window_data->mouse_wheel_y);
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
#if defined(USE_INVERTED_Y_ON_MACOS)
        window_data->mouse_pos_y = point.y;
#else
        window_data->mouse_pos_y = window_data->window_height - point.y;
#endif
        kCall(mouse_move_func, window_data->mouse_pos_x, window_data->mouse_pos_y);
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

#pragma mark NSTextInputClient

//-------------------------------------
// [Binding Keystrokes]
//-------------------------------------

// Invokes the action specified by the given selector.
//-------------------------------------
- (void)doCommandBySelector:(nonnull SEL)selector {
    kUnused(selector);
}

//-------------------------------------
// [Storing Text]
//-------------------------------------

// Returns an attributed string derived from the given range in the receiver's text storage.
//-------------------------------------
- (nullable NSAttributedString *)attributedSubstringForProposedRange:(NSRange)range actualRange:(nullable NSRangePointer)actualRange {
    kUnused(range);
    kUnused(actualRange);
    return nil;
}

// Inserts the given string into the receiver, replacing the specified content.
//-------------------------------------
- (void)insertText:(nonnull id)string replacementRange:(NSRange)replacementRange {
    kUnused(replacementRange);

    if(window_data != 0x0) {
        NSString    *characters;
        NSUInteger  codepoint;

        if ([string isKindOfClass:[NSAttributedString class]])
            characters = [string string];
        else
            characters = (NSString*) string;

        NSRange range = NSMakeRange(0, [characters length]);
        while (range.length) {
            codepoint = 0;
            if ([characters getBytes:&codepoint
                       maxLength:sizeof(codepoint)
                      usedLength:NULL
                        encoding:NSUTF32StringEncoding // NSUTF8StringEncoding
                         options:0
                           range:range
                  remainingRange:&range]) {

                if ((codepoint & 0xff00) == 0xf700)
                    continue;

                kCall(char_input_func, codepoint);
            }
        }
    }
}

//-------------------------------------
// [Getting Character Coordinates]
//-------------------------------------

// Returns the index of the character whose bounding rectangle includes the given point.
//-------------------------------------
- (NSUInteger)characterIndexForPoint:(NSPoint)point {
    kUnused(point);
    return 0;
}

// Returns the first logical boundary rectangle for characters in the given range.
//-------------------------------------
- (NSRect)firstRectForCharacterRange:(NSRange)range actualRange:(nullable NSRangePointer)actualRange {
    kUnused(range);
    kUnused(actualRange);
    return NSMakeRect(0.0, 0.0, 0.0, 0.0);
}

//-------------------------------------
// [Handling Marked Text]
//-------------------------------------

//-------------------------------------
static const NSRange kEmptyRange = { NSNotFound, 0 };

// Returns a Boolean value indicating whether the receiver has marked text.
//-------------------------------------
- (BOOL)hasMarkedText {
    return false;
}

// Returns the range of the marked text.
//-------------------------------------
- (NSRange)markedRange {
    return kEmptyRange;
}

// Returns the range of selected text.
//-------------------------------------
- (NSRange)selectedRange {
    return kEmptyRange;
}

// Replaces a specified range in the receiver’s text storage with the given string and sets the selection.
//-------------------------------------
- (void)setMarkedText:(nonnull id)string selectedRange:(NSRange)selectedRange replacementRange:(NSRange)replacementRange {
    kUnused(string);
    kUnused(selectedRange);
    kUnused(replacementRange);
}

// Unmarks the marked text.
//-------------------------------------
- (void)unmarkText {
}

// Returns an array of attribute names recognized by the receiver.
//-------------------------------------
- (nonnull NSArray<NSString *> *)validAttributesForMarkedText {
    return [NSArray array];
}
//----

@end
