#import <Cocoa/Cocoa.h>
#include <WindowData.h>
#include <MiniFB_enums.h>
#include <MiniFB_internal.h>

//-------------------------------------
// There is no Num Lock on macOS. NSEventModifierFlagNumericPad only says the key belongs to
// the keypad or is an arrow, which is not a lock and does not belong in the modifier state.
//-------------------------------------
static inline uint32_t
translate_modifiers(NSEventModifierFlags flags) {
    uint32_t mod_keys = 0;

    if (flags & NSEventModifierFlagCapsLock) {
        mod_keys |= MFB_KB_MOD_CAPS_LOCK;
    }
    if (flags & NSEventModifierFlagShift) {
        mod_keys |= MFB_KB_MOD_SHIFT;
    }
    if (flags & NSEventModifierFlagControl) {
        mod_keys |= MFB_KB_MOD_CONTROL;
    }
    if (flags & NSEventModifierFlagOption) {
        mod_keys |= MFB_KB_MOD_ALT;
    }
    if (flags & NSEventModifierFlagCommand) {
        mod_keys |= MFB_KB_MOD_SUPER;
    }

    return mod_keys;
}

//-------------------------------------
static inline uint32_t
update_mod_keys(SWindowData *window_data, NSEventModifierFlags flags) {
    return mfb_recalc_mod_keys(window_data, translate_modifiers(flags));
}

//-------------------------------------
void sync_key_status(SWindowData *window_data);

//-------------------------------------
@interface OSXWindow : NSWindow<NSWindowDelegate>
{
    NSView              *childContentView;
    @public SWindowData *window_data;
}

//-------------------------------------
- (id) initWithContentRect:(NSRect) contentRect
                styleMask:(NSWindowStyleMask) windowStyle
                  backing:(NSBackingStoreType) bufferingType
                    defer:(BOOL) deferCreation
               windowData:(SWindowData *) windowData;

- (void) removeWindowData;

// Return the real content view (the internal frame view that implements drawRect:)
- (NSView *) rootContentView;

// Invalidate/reset cursor rects for the frame view so per-window cursor changes take effect
- (void) updateCursorRects;

@end
