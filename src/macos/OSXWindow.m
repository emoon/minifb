#import "OSXWindow.h"
#import "OSXView.h"
#include "WindowData_OSX.h"
#include <MiniFB_internal.h>
#include <MiniFB_enums.h>
#include <Carbon/Carbon.h>
#include <IOKit/hidsystem/IOLLEvent.h>

#if defined(USE_METAL_API)
//-------------------------------------
static void
update_metal_viewport_vertices(SWindowData *window_data) {
    if (window_data == NULL || window_data->specific == NULL ||
        window_data->window_width == 0 || window_data->window_height == 0) {
        return;
    }

    SWindowData_OSX *window_data_specific = (SWindowData_OSX *) window_data->specific;
    if (window_data_specific == NULL) {
        return;
    }

    float inv_width  = 1.0f / (float) window_data->window_width;
    float inv_height = 1.0f / (float) window_data->window_height;

    float x1 = ((float) window_data->dst_offset_x * inv_width) * 2.0f - 1.0f;
    float x2 = ((float) (window_data->dst_offset_x + window_data->dst_width) * inv_width) * 2.0f - 1.0f;
    float y1 = ((float) window_data->dst_offset_y * inv_height) * 2.0f - 1.0f;
    float y2 = ((float) (window_data->dst_offset_y + window_data->dst_height) * inv_height) * 2.0f - 1.0f;

    window_data_specific->metal.vertices[0].x = x1;
    window_data_specific->metal.vertices[0].y = y1;

    window_data_specific->metal.vertices[1].x = x1;
    window_data_specific->metal.vertices[1].y = y2;

    window_data_specific->metal.vertices[2].x = x2;
    window_data_specific->metal.vertices[2].y = y1;

    window_data_specific->metal.vertices[3].x = x2;
    window_data_specific->metal.vertices[3].y = y2;
}
#endif

//-------------------------------------
static inline bool
key_is_valid(mfb_key key_code) {
    return key_code != MFB_KB_KEY_UNKNOWN &&
           (int) key_code >= 0 && (int) key_code < MFB_MAX_KEYS;
}

//-------------------------------------
// Apple named these keycodes after ANSI hardware, but the driver assigns them by the keyboard
// type it believes it has: on an ISO keyboard 0x0A is the key below Escape and 0x32 the one
// next to left Shift, the opposite of what the names say. The type cannot be cached. It is
// unknown until the first key event reaches the process, and another keyboard can be plugged
// in while the window is open.
//-------------------------------------
static mfb_key
translate_keycode(unsigned short keycode) {
    if ((keycode == 0x0A || keycode == 0x32) &&
        KBGetLayoutType(LMGetKbdType()) == kKeyboardISO) {
        keycode = (keycode == 0x0A) ? 0x32 : 0x0A;
    }

    return (mfb_key) g_keycodes[keycode & 0x1ff];
}

//-------------------------------------
// A keyboard that does not report sides sets only the generic bit, and then it is the one
// answer available. It gives itself away when the generic bit disagrees with the OR of the
// two device bits.
//-------------------------------------
static bool
is_modifier_pressed(NSEventModifierFlags flags, NSEventModifierFlags target_mask,
                    NSEventModifierFlags other_mask, NSEventModifierFlags either_mask) {
    bool target_pressed = (flags & target_mask) != 0;
    bool other_pressed  = (flags & other_mask) != 0;
    bool either_pressed = (flags & either_mask) != 0;

    if (either_pressed != (target_pressed || other_pressed)) {
        return either_pressed;
    }

    return target_pressed;
}

//-------------------------------------
static const struct {
    mfb_key              key_code;
    NSEventModifierFlags target_mask;
    NSEventModifierFlags other_mask;
    NSEventModifierFlags either_mask;
} g_modifier_keys[] = {
    { MFB_KB_KEY_LEFT_SHIFT,    NX_DEVICELSHIFTKEYMASK, NX_DEVICERSHIFTKEYMASK, NX_SHIFTMASK     },
    { MFB_KB_KEY_RIGHT_SHIFT,   NX_DEVICERSHIFTKEYMASK, NX_DEVICELSHIFTKEYMASK, NX_SHIFTMASK     },
    { MFB_KB_KEY_LEFT_CONTROL,  NX_DEVICELCTLKEYMASK,   NX_DEVICERCTLKEYMASK,   NX_CONTROLMASK   },
    { MFB_KB_KEY_RIGHT_CONTROL, NX_DEVICERCTLKEYMASK,   NX_DEVICELCTLKEYMASK,   NX_CONTROLMASK   },
    { MFB_KB_KEY_LEFT_ALT,      NX_DEVICELALTKEYMASK,   NX_DEVICERALTKEYMASK,   NX_ALTERNATEMASK },
    { MFB_KB_KEY_RIGHT_ALT,     NX_DEVICERALTKEYMASK,   NX_DEVICELALTKEYMASK,   NX_ALTERNATEMASK },
    { MFB_KB_KEY_LEFT_SUPER,    NX_DEVICELCMDKEYMASK,   NX_DEVICERCMDKEYMASK,   NX_COMMANDMASK   },
    { MFB_KB_KEY_RIGHT_SUPER,   NX_DEVICERCMDKEYMASK,   NX_DEVICELCMDKEYMASK,   NX_COMMANDMASK   },
};

//-------------------------------------
// The platform reports the lock and never the key, and nothing says the key came back up, so
// the release is synthesized after the press to match what every other backend reports.
//-------------------------------------
static void
update_caps_lock(SWindowData *window_data, NSEventModifierFlags flags) {
    SWindowData_OSX *window_data_specific = (SWindowData_OSX *) window_data->specific;
    if (window_data_specific == NULL) {
        return;
    }

    bool caps_lock_on = (flags & NSEventModifierFlagCapsLock) != 0;
    if (caps_lock_on == window_data_specific->caps_lock_on) {
        return;
    }
    window_data_specific->caps_lock_on = caps_lock_on;

    window_data->key_status[MFB_KB_KEY_CAPS_LOCK] = 1;
    uint32_t mod_keys = update_mod_keys(window_data, flags);
    kCall(keyboard_func, MFB_KB_KEY_CAPS_LOCK, (mfb_key_mod) mod_keys, true);

    window_data->key_status[MFB_KB_KEY_CAPS_LOCK] = 0;
    mod_keys = update_mod_keys(window_data, flags);
    kCall(keyboard_func, MFB_KB_KEY_CAPS_LOCK, (mfb_key_mod) mod_keys, false);
}

//-------------------------------------
// Keys pressed while another window had the focus produce no event here, so the state is read
// from the device instead of from what this window happened to see. Caps Lock is left out:
// the device reports it down for as long as the lock is on, which is not a key being held.
//-------------------------------------
void
sync_key_status(SWindowData *window_data) {
    if (window_data == NULL) {
        return;
    }

    memset(window_data->key_status, 0, sizeof(window_data->key_status));

    for (unsigned keycode = 0; keycode < 0x80; ++keycode) {
        if (keycode == kVK_CapsLock) {
            continue;
        }

        if (CGEventSourceKeyState(kCGEventSourceStateCombinedSessionState,
                                  (CGKeyCode) keycode) == false) {
            continue;
        }

        mfb_key key_code = translate_keycode((unsigned short) keycode);
        if (key_is_valid(key_code) == true) {
            window_data->key_status[key_code] = 1;
        }
    }

    NSEventModifierFlags flags = [NSEvent modifierFlags];

    SWindowData_OSX *window_data_specific = (SWindowData_OSX *) window_data->specific;
    if (window_data_specific != NULL) {
        // Without this, a lock toggled while another window had the focus would look like a
        // keystroke on the next flagsChanged and report a press nobody made.
        window_data_specific->caps_lock_on = (flags & NSEventModifierFlagCapsLock) != 0;
    }

    update_mod_keys(window_data, flags);
}

//-------------------------------------
static void
set_frame_view_window_data(NSView *frame_view, SWindowData *window_data) {
    if (frame_view == nil) {
        return;
    }

    if ([frame_view isKindOfClass:[OSXView class]]) {
        ((OSXView *) frame_view)->window_data = window_data;
    }
    else {
        MFB_LOG(MFB_LOG_WARNING, "OSXWindow: root content view is not an OSXView instance.");
    }
}

@implementation OSXWindow

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

- (id)initWithContentRect:(NSRect)contentRect
                styleMask:(NSWindowStyleMask)windowStyle
                  backing:(NSBackingStoreType)bufferingType
                    defer:(BOOL)deferCreation
               windowData:(SWindowData *) windowData
{
    self = [super
        initWithContentRect:contentRect
        styleMask:windowStyle
        backing:bufferingType
        defer:deferCreation];

    if (self)
    {
        [self setOpaque:YES];
        [self setBackgroundColor:[NSColor clearColor]];

        self.delegate = self;

        self->window_data = windowData;
        set_frame_view_window_data([super contentView], windowData);
    }
    return self;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

- (void) removeWindowData {
    self->window_data = 0x0;
    set_frame_view_window_data([super contentView], 0x0);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter]
        removeObserver:self];
    [super dealloc];
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

- (void)setContentSize:(NSSize)newSize
{
    NSSize sizeDelta = newSize;
    NSSize childBoundsSize = [childContentView bounds].size;
    sizeDelta.width -= childBoundsSize.width;
    sizeDelta.height -= childBoundsSize.height;

    OSXView *frameView = [super contentView];
    NSSize newFrameSize = [frameView bounds].size;
    newFrameSize.width += sizeDelta.width;
    newFrameSize.height += sizeDelta.height;

    [super setContentSize:newFrameSize];
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

- (void)flagsChanged:(NSEvent *)event
{
    if(window_data == 0x0) {
        [super flagsChanged:event];
        return;
    }

    NSEventModifierFlags flags = [event modifierFlags];

    update_caps_lock(window_data, flags);

    // All eight are reevaluated, and not only the one the keyCode names, because the event
    // that releases one side while the other is held carries the keyCode of neither.
    for (unsigned i = 0; i < sizeof(g_modifier_keys) / sizeof(g_modifier_keys[0]); ++i) {
        mfb_key key_code = g_modifier_keys[i].key_code;
        bool    is_pressed = is_modifier_pressed(flags,
                                                 g_modifier_keys[i].target_mask,
                                                 g_modifier_keys[i].other_mask,
                                                 g_modifier_keys[i].either_mask);

        if ((window_data->key_status[key_code] != 0) == is_pressed) {
            continue;
        }

        window_data->key_status[key_code] = is_pressed;
        uint32_t mod_keys = update_mod_keys(window_data, flags);
        kCall(keyboard_func, key_code, (mfb_key_mod) mod_keys, is_pressed);
    }

    update_mod_keys(window_data, flags);

    [super flagsChanged:event];
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

- (void)keyDown:(NSEvent *)event
{
    if(window_data != 0x0) {
        mfb_key key_code = translate_keycode([event keyCode]);
        bool    report = key_is_valid(key_code);

        if (report == true) {
            window_data->key_status[key_code] = true;
        }

        uint32_t mod_keys = update_mod_keys(window_data, [event modifierFlags]);

        if (report == true) {
            kCall(keyboard_func, key_code, (mfb_key_mod) mod_keys, true);
        }
    }
    [childContentView.superview interpretKeyEvents:@[event]];
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

- (void)keyUp:(NSEvent *)event
{
    if(window_data != 0x0) {
        mfb_key key_code = translate_keycode([event keyCode]);
        // macOS delivers the key up of a key that losing the focus already released, and
        // reporting it again would be a second release for a single press.
        bool    report = key_is_valid(key_code) &&
                         window_data->key_status[key_code] != 0;

        if (report == true) {
            window_data->key_status[key_code] = false;
        }

        uint32_t mod_keys = update_mod_keys(window_data, [event modifierFlags]);

        if (report == true) {
            kCall(keyboard_func, key_code, (mfb_key_mod) mod_keys, false);
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// DEAD CODE: mainWindowChanged: is not a standard NSWindowDelegate method and is never
// registered as an observer. Active/inactive state is handled by the standard delegate
// methods windowDidBecomeKey: and windowDidResignKey: below.
//- (void)mainWindowChanged:(NSNotification *)notification
//{
//    kUnused(notification);
//
//    if(window_data != 0x0) {
//        if(window_data->is_active == true) {
//            window_data->is_active = false;
//            kCall(active_func, false);
//        }
//    }
//}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

- (void)setContentView:(NSView *)aView
{
    if ([childContentView isEqualTo:aView]) {
        return;
    }

    NSRect bounds = [self frame];
    bounds.origin = NSZeroPoint;

    OSXView *frameView = [super contentView];
    if (!frameView)
    {
        frameView = [[[OSXView alloc] initWithFrame:bounds] autorelease];
        frameView->window_data = self->window_data;

        [super setContentView:frameView];
    }
    else {
        set_frame_view_window_data(frameView, self->window_data);
    }

    if (childContentView)
    {
        [childContentView removeFromSuperview];
    }
    childContentView = aView;
    [childContentView setFrame:[self contentRectForFrameRect:bounds]];
    [childContentView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [frameView addSubview:childContentView];
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

- (NSView *)contentView
{
    return childContentView;
}

// Return the real content view (the internal frame view created via super setContentView:)
- (NSView *)rootContentView
{
    return [super contentView];
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

- (BOOL)canBecomeKeyWindow
{
    return YES;
}

- (void)windowDidBecomeKey:(NSNotification *)notification
{
    kUnused(notification);
    if(window_data != 0x0) {
        window_data->is_active = true;
        sync_key_status(window_data);
        kCall(active_func, true);
    }
}

- (void)windowDidResignKey:(NSNotification *)notification
{
    kUnused(notification);
    if(window_data) {
        window_data->is_active = false;
        kCall(active_func, false);
        mfb_release_held_keys(window_data, translate_modifiers([NSEvent modifierFlags]));
    }
}

- (BOOL)windowShouldClose:(NSWindow *) window
{
    kUnused(window);
    bool destroy = false;
    if (!window_data) {
        destroy = true;
    }
    else {
        // Obtain a confirmation of close
        if (!window_data->close_func || window_data->close_func((struct mfb_window *)window_data)) {
            destroy = true;
        }
    }

    if (!destroy) {
        MFB_LOG(MFB_LOG_DEBUG, "OSXWindow: close request was rejected by close callback.");
    }

    return destroy;
}

- (void)windowWillClose:(NSNotification *)notification {
    kUnused(notification);
    if(window_data) {
        window_data->close = true;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

- (BOOL)canBecomeMainWindow
{
    return YES;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

- (NSRect)contentRectForFrameRect:(NSRect)windowFrame
{
    windowFrame.origin = NSZeroPoint;
    return NSInsetRect(windowFrame, 0, 0);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

+ (NSRect)frameRectForContentRect:(NSRect)windowContentRect styleMask:(NSWindowStyleMask)windowStyle
{
    kUnused(windowStyle);
    return NSInsetRect(windowContentRect, 0, 0);
}

// DEAD CODE: willClose is never called. Window close signalling is handled by the
// standard NSWindowDelegate method windowWillClose: above.
//- (void)willClose
//{
//    if(window_data != 0x0) {
//        window_data->close = true;
//    }
//}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

- (void)windowDidResize:(NSNotification *)notification {
    kUnused(notification);
    if(window_data != 0x0) {
        CGSize size = [self contentRectForFrameRect:[self frame]].size;
        uint32_t resized_width  = size.width  > 0.0 ? (uint32_t) size.width  : 0u;
        uint32_t resized_height = size.height > 0.0 ? (uint32_t) size.height : 0u;

        window_data->window_width  = resized_width;
        window_data->window_height = resized_height;
        resize_dst(window_data, resized_width, resized_height);

#if defined(USE_METAL_API)
        update_metal_viewport_vertices(window_data);
        window_data->must_resize_context = true;
#else
        kCall(resize_func, (int) resized_width, (int) resized_height);
#endif
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

- (void)updateCursorRects {
    NSView *frame_view = nil;
    if (self->childContentView != nil) {
        frame_view = self->childContentView.superview;
    }
    if (frame_view == nil) {
        frame_view = [super contentView];
    }

    // Ask the window to invalidate the cursor rects for the frame view.
    // The system will call -resetCursorRects on that view, where we install
    // the proper per-window cursor.
    if (frame_view != nil) {
        [self invalidateCursorRectsForView:frame_view];
    }
}

@end
