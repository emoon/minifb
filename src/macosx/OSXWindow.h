#import <Cocoa/Cocoa.h>
#include <WindowData.h>
#include <MiniFB_enums.h>

//-------------------------------------
static inline uint32_t
translate_modifiers(NSEventModifierFlags flags) {
    uint32_t mod_keys = 0;

    if (flags & NSEventModifierFlagCapsLock) {
        mod_keys |= KB_MOD_CAPS_LOCK;
    }
    if (flags & NSEventModifierFlagShift) {
        mod_keys |= KB_MOD_SHIFT;
    }
    if (flags & NSEventModifierFlagControl) {
        mod_keys |= KB_MOD_CONTROL;
    }
    if (flags & NSEventModifierFlagOption) {
        mod_keys |= KB_MOD_ALT;
    }
    if (flags & NSEventModifierFlagCommand) {
        mod_keys |= KB_MOD_SUPER;
    }
    if (flags & NSEventModifierFlagNumericPad) {
        mod_keys |= KB_MOD_NUM_LOCK;
    }

    return mod_keys;
}

//-------------------------------------
@interface OSXWindow : NSWindow<NSWindowDelegate>
{
    NSView              *childContentView;
    @public SWindowData *window_data;
}

//-------------------------------------
- (id)initWithContentRect:(NSRect)contentRect
                styleMask:(NSWindowStyleMask)windowStyle
                  backing:(NSBackingStoreType)bufferingType
                    defer:(BOOL)deferCreation
               windowData:(SWindowData *) windowData;

- (void) removeWindowData;

// Return the real content view (the internal frame view that implements drawRect:)
- (NSView *) rootContentView;

// Invalidate/reset cursor rects for the frame view so per-window cursor changes take effect
- (void) updateCursorRects;

@end
