#import "OSXView.h"
#import "OSXWindow.h"
#import "WindowData_OSX.h"
#include <MiniFB_internal.h>

//-------------------------------------
@implementation OSXView

//-------------------------------------
// Tracking area: keeps mouseEntered:/mouseExited: events alive in both Metal and non-Metal paths.
- (void)updateTrackingAreas {
    if(tracking_area != nil) {
        [self removeTrackingArea:tracking_area];
        [tracking_area release];
        tracking_area = nil;
    }

    int opts = (NSTrackingMouseEnteredAndExited | NSTrackingActiveAlways);
    tracking_area = [[NSTrackingArea alloc] initWithRect:[self bounds]
                                                 options:opts
                                                   owner:self
                                                userInfo:nil];
    [self addTrackingArea:tracking_area];
}

#if !defined(USE_METAL_API)

// DEAD CODE: resizeRect was intended for custom resize handle drawing.
// macOS provides the resize handle automatically for NSWindowStyleMaskResizable windows.
// Kept for reference in case a custom resize affordance is needed in the future.
//- (NSRect)resizeRect {
//    const CGFloat resizeBoxSize = 16.0;
//    const CGFloat contentViewPadding = 5.5;
//
//    NSRect contentViewRect = [[self window] contentRectForFrameRect:[[self window] frame]];
//    NSRect resizeRect = NSMakeRect(
//        NSMaxX(contentViewRect) + contentViewPadding,
//        NSMinY(contentViewRect) - resizeBoxSize - contentViewPadding,
//        resizeBoxSize,
//        resizeBoxSize
//    );
//
//    return resizeRect;
//}

//-------------------------------------
- (void)drawRect:(NSRect)rect {
    (void)rect;

    if(window_data == 0x0)
        return;

    SWindowData_OSX *window_data_osx = (SWindowData_OSX *) window_data->specific;
    if (!window_data_osx || !window_data_osx->window || !window_data->draw_buffer || window_data->buffer_stride == 0 || window_data->buffer_height == 0)
        return;

    CGContextRef context = [[NSGraphicsContext currentContext] CGContext];
    size_t image_size = (size_t) window_data->buffer_stride * (size_t) window_data->buffer_height;

    CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
    if (space == NULL) {
        mfb_log(MFB_LOG_ERROR, "OSXView: failed to create RGB color space for drawRect.");
        return;
    }
    CGDataProviderRef provider = CGDataProviderCreateWithData(0x0,
                                                              window_data->draw_buffer,
                                                              image_size,
                                                              0x0
    );
    if (provider == NULL) {
        mfb_log(MFB_LOG_ERROR, "OSXView: failed to create CGDataProvider for drawRect.");
        CGColorSpaceRelease(space);
        return;
    }

    CGImageRef img = CGImageCreate(window_data->buffer_width
                                 , window_data->buffer_height
                                 , 8
                                 , 32
                                 , window_data->buffer_stride
                                 , space
                                 , kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder32Little
                                 , provider
                                 , 0x0
                                 , false
                                 , kCGRenderingIntentDefault
    );
    if (img == NULL) {
        mfb_log(MFB_LOG_ERROR, "OSXView: failed to create CGImage for drawRect.");
        CGDataProviderRelease(provider);
        CGColorSpaceRelease(space);
        return;
    }

    const CGFloat components[] = {0.0f, 0.0f, 0.0f, 1.0f};
    const CGColorRef black = CGColorCreate(space, components);

    CGColorSpaceRelease(space);
    CGDataProviderRelease(provider);

    if(window_data->dst_offset_x != 0 || window_data->dst_offset_y != 0 || window_data->dst_width != window_data->window_width || window_data->dst_height != window_data->window_height) {
        if (black != NULL) {
            CGContextSetFillColorWithColor(context, black);
            CGContextFillRect(context, [self bounds]);
        }
    }

    CGContextDrawImage(context,
                       CGRectMake(window_data->dst_offset_x, window_data->dst_offset_y, window_data->dst_width, window_data->dst_height),
                       img
    );

    if (black != NULL) {
        CGColorRelease(black);
    }
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
    if(window_data != 0x0) {
        window_data->mod_keys = translate_modifiers([event modifierFlags]);
        window_data->mouse_button_status[MOUSE_BTN_1] = true;
        kCall(mouse_btn_func, MOUSE_BTN_1, window_data->mod_keys, true);
    }
}

//-------------------------------------
- (void)mouseUp:(NSEvent*)event {
    if(window_data != 0x0) {
        window_data->mod_keys = translate_modifiers([event modifierFlags]);
        window_data->mouse_button_status[MOUSE_BTN_1] = false;
        kCall(mouse_btn_func, MOUSE_BTN_1, window_data->mod_keys, false);
    }
}

//-------------------------------------
- (void)rightMouseDown:(NSEvent*)event {
    if(window_data != 0x0) {
        window_data->mod_keys = translate_modifiers([event modifierFlags]);
        window_data->mouse_button_status[MOUSE_BTN_2] = true;
        kCall(mouse_btn_func, MOUSE_BTN_2, window_data->mod_keys, true);
    }
}

//-------------------------------------
- (void)rightMouseUp:(NSEvent*)event {
    if(window_data != 0x0) {
        window_data->mod_keys = translate_modifiers([event modifierFlags]);
        window_data->mouse_button_status[MOUSE_BTN_2] = false;
        kCall(mouse_btn_func, MOUSE_BTN_2, window_data->mod_keys, false);
    }
}

//-------------------------------------
- (void)otherMouseDown:(NSEvent *)event {
    if(window_data != 0x0) {
        uint32_t mapped_button = (uint32_t) [event buttonNumber] + 1u;
        if (mapped_button > (uint32_t) MOUSE_BTN_7) {
            mfb_log(MFB_LOG_WARNING, "OSXView: otherMouseDown received buttonNumber=%u; clamping to MOUSE_BTN_7.",
                    (unsigned) [event buttonNumber]);
            mapped_button = (uint32_t) MOUSE_BTN_7;
        }
        mfb_mouse_button button = (mfb_mouse_button) mapped_button;
        window_data->mod_keys = translate_modifiers([event modifierFlags]);
        window_data->mouse_button_status[mapped_button] = true;
        kCall(mouse_btn_func, button, window_data->mod_keys, true);
    }
}

//-------------------------------------
- (void)otherMouseUp:(NSEvent *)event {
    if(window_data != 0x0) {
        uint32_t mapped_button = (uint32_t) [event buttonNumber] + 1u;
        if (mapped_button > (uint32_t) MOUSE_BTN_7) {
            mfb_log(MFB_LOG_WARNING, "OSXView: otherMouseUp received buttonNumber=%u; clamping to MOUSE_BTN_7.",
                    (unsigned) [event buttonNumber]);
            mapped_button = (uint32_t) MOUSE_BTN_7;
        }
        mfb_mouse_button button = (mfb_mouse_button) mapped_button;
        window_data->mod_keys = translate_modifiers([event modifierFlags]);
        window_data->mouse_button_status[mapped_button] = false;
        kCall(mouse_btn_func, button, window_data->mod_keys, false);
    }
}

//-------------------------------------
- (void)scrollWheel:(NSEvent *)event {
    if(window_data != 0x0) {
        window_data->mod_keys = translate_modifiers([event modifierFlags]);
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
        window_data->mod_keys = translate_modifiers([event modifierFlags]);
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
    // On mouse exit, refresh cursor rects so the window-specific cursor state is applied.
    if (window_data != 0x0 && window_data->is_cursor_visible == false) {
        OSXWindow *window = (OSXWindow *)[self window];
        if (window) {
            [window updateCursorRects];
        }
    }
}

//-------------------------------------
- (void)mouseEntered:(NSEvent *)event {
    (void)event;
    //printf("mouse enter\n");
    // On mouse enter, refresh cursor rects so the window-specific cursor state is applied.
    if (window_data != 0x0 && window_data->is_cursor_visible == false) {
        OSXWindow *window = (OSXWindow *)[self window];
        if (window) {
            [window updateCursorRects];
        }
    }
}

// Ensure the view provides per-window cursor control by installing a cursor rect
// for the whole view. When the view requests cursor hiding for its window this
// will use an invisible cursor instead of hiding the global cursor.
- (void)resetCursorRects {
    [super resetCursorRects];

    static NSCursor *invisible_cursor = nil;
    if (invisible_cursor == nil) {
        NSImage *img = [[NSImage alloc] initWithSize:NSMakeSize(16, 16)];
        [img lockFocus];
        [[NSColor clearColor] set];
        NSRectFill(NSMakeRect(0, 0, 16, 16));
        [img unlockFocus];

        invisible_cursor = [[NSCursor alloc] initWithImage:img hotSpot:NSMakePoint(0,0)];
        [img release];
    }

    if (window_data != 0x0 && window_data->is_cursor_visible == false) {
        [self addCursorRect:[self bounds] cursor:invisible_cursor];
    }
    else {
        [self addCursorRect:[self bounds] cursor:[NSCursor arrowCursor]];
    }
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
    if (tracking_area != nil) {
        [self removeTrackingArea:tracking_area];
        [tracking_area release];
        tracking_area = nil;
    }
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
static const NSRange g_empty_range = { NSNotFound, 0 };

// Returns a Boolean value indicating whether the receiver has marked text.
//-------------------------------------
- (BOOL)hasMarkedText {
    return false;
}

// Returns the range of the marked text.
//-------------------------------------
- (NSRange)markedRange {
    return g_empty_range;
}

// Returns the range of selected text.
//-------------------------------------
- (NSRange)selectedRange {
    return g_empty_range;
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
