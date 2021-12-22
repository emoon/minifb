#import "OSXWindow.h"
#import "OSXView.h"
#include "WindowData_OSX.h"
#include <MiniFB_internal.h>
#include <MiniFB_enums.h>

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
        OSXView *view = (OSXView *) self->childContentView.superview;
        view->window_data = windowData;
    }
    return self;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

- (void) removeWindowData {
    self->window_data = 0x0;
    OSXView *view = (OSXView *) self->childContentView.superview;
    view->window_data = 0x0;
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
    if(window_data == 0x0)
        return;

    const uint32_t flags = [event modifierFlags];
    uint32_t	mod_keys = 0, mod_keys_aux = 0;

    //NSEventModifierFlagHelp = 1 << 22,
    //NSEventModifierFlagFunction = 1 << 23,
    if(flags & NSEventModifierFlagCapsLock) {
        mod_keys |= KB_MOD_CAPS_LOCK;
    }
    if(flags & NSEventModifierFlagShift) {
        mod_keys |= KB_MOD_SHIFT;
    }
    if(flags & NSEventModifierFlagControl) {
        mod_keys |= KB_MOD_CONTROL;
    }
    if(flags & NSEventModifierFlagOption) {
        mod_keys |= KB_MOD_ALT;
    }
    if(flags & NSEventModifierFlagCommand) {
        mod_keys |= KB_MOD_SUPER;
    }
    if(flags & NSEventModifierFlagNumericPad) {
        mod_keys |= KB_MOD_NUM_LOCK;
    }

    if(mod_keys != window_data->mod_keys) {
        short int key_code = g_keycodes[[event keyCode] & 0x1ff];
        if(key_code != KB_KEY_UNKNOWN) {
            mod_keys_aux = mod_keys ^ window_data->mod_keys;
            if(mod_keys_aux & KB_MOD_CAPS_LOCK) {
                window_data->key_status[key_code] = (mod_keys & KB_MOD_CAPS_LOCK) != 0;
                kCall(keyboard_func, key_code, mod_keys, window_data->key_status[key_code]);
            }
            if(mod_keys_aux & KB_MOD_SHIFT) {
                window_data->key_status[key_code] = (mod_keys & KB_MOD_SHIFT) != 0;
                kCall(keyboard_func, key_code, mod_keys, window_data->key_status[key_code]);
            }
            if(mod_keys_aux & KB_MOD_CONTROL) {
                window_data->key_status[key_code] = (mod_keys & KB_MOD_CONTROL) != 0;
                kCall(keyboard_func, key_code, mod_keys, window_data->key_status[key_code]);
            }
            if(mod_keys_aux & KB_MOD_ALT) {
                window_data->key_status[key_code] = (mod_keys & KB_MOD_ALT) != 0;
                kCall(keyboard_func, key_code, mod_keys, window_data->key_status[key_code]);
            }
            if(mod_keys_aux & KB_MOD_SUPER) {
                window_data->key_status[key_code] = (mod_keys & KB_MOD_SUPER) != 0;
                kCall(keyboard_func, key_code, mod_keys, window_data->key_status[key_code]);
            }
            if(mod_keys_aux & KB_MOD_NUM_LOCK) {
                window_data->key_status[key_code] = (mod_keys & KB_MOD_NUM_LOCK) != 0;
                kCall(keyboard_func, key_code, mod_keys, window_data->key_status[key_code]);
            }
        }
    }
    window_data->mod_keys = mod_keys;

    [super flagsChanged:event];
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

- (void)keyDown:(NSEvent *)event
{
    if(window_data != 0x0) {
        short int key_code = g_keycodes[[event keyCode] & 0x1ff];
        window_data->key_status[key_code] = true;
        kCall(keyboard_func, key_code, window_data->mod_keys, true);
    }
    [childContentView.superview interpretKeyEvents:@[event]];
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

- (void)keyUp:(NSEvent *)event
{
    if(window_data != 0x0) {
        short int key_code = g_keycodes[[event keyCode] & 0x1ff];
        window_data->key_status[key_code] = false;
        kCall(keyboard_func, key_code, window_data->mod_keys, false);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

- (void)mainWindowChanged:(NSNotification *)notification
{
    kUnused(notification);

    if(window_data != 0x0) {
        if(window_data->is_active == true) {
            window_data->is_active = false;
            kCall(active_func, false);
        }
    }
}

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

        [super setContentView:frameView];
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
        kCall(active_func, true);
    }
}

- (void)windowDidResignKey:(NSNotification *)notification
{
    kUnused(notification);
    if(window_data) {
        window_data->is_active = false;
        kCall(active_func, false);
    }
}

- (BOOL)windowShouldClose:(NSWindow *) window 
{
    bool destroy = false;
    if (!window_data) {
        destroy = true;
    } else {
        // Obtain a confirmation of close
        if (!window_data->close_func || window_data->close_func((struct mfb_window*)window_data)) {
            destroy = true;
        }
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

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

- (void)willClose
{
    if(window_data != 0x0) {
        window_data->close = true;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

- (void)windowDidResize:(NSNotification *)notification {
    kUnused(notification);
    if(window_data != 0x0) {
        CGSize size = [self contentRectForFrameRect:[self frame]].size;

        window_data->window_width  = size.width;
        window_data->window_height = size.height;
        resize_dst(window_data, size.width, size.height);

        kCall(resize_func, size.width, size.height);
    }
}

@end
