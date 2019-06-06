#import "OSXWindow.h"
#import "OSXWindowFrameView.h"
#include "WindowData_OSX.h"
#include <MiniFB_internal.h>
#include <MiniFB_enums.h>

extern short int    g_keycodes[512];

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
        OSXWindowFrameView *view = (OSXWindowFrameView *) self->childContentView.superview;
        view->window_data = windowData;
    }
    return self;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

- (void) removeWindowData {
    self->window_data = 0x0;
    OSXWindowFrameView *view = (OSXWindowFrameView *) self->childContentView.superview;
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
    
    OSXWindowFrameView *frameView = [super contentView];
    NSSize newFrameSize = [frameView bounds].size;
    newFrameSize.width += sizeDelta.width;
    newFrameSize.height += sizeDelta.height;
    
    [super setContentSize:newFrameSize];
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

- (void)flagsChanged:(NSEvent *)event
{
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
        short int keyCode = keycodes[[event keyCode] & 0x1ff];
        if(keyCode != KB_KEY_UNKNOWN) {
            mod_keys_aux = mod_keys ^ window_data->mod_keys;
            if(mod_keys_aux & KB_MOD_CAPS_LOCK) {
                kCall(keyboard_func, keyCode, mod_keys, (mod_keys & KB_MOD_CAPS_LOCK) != 0);
            }
            if(mod_keys_aux & KB_MOD_SHIFT) {
                kCall(keyboard_func, keyCode, mod_keys, (mod_keys & KB_MOD_SHIFT) != 0);
            }
            if(mod_keys_aux & KB_MOD_CONTROL) {
                kCall(keyboard_func, keyCode, mod_keys, (mod_keys & KB_MOD_CONTROL) != 0);
            }
            if(mod_keys_aux & KB_MOD_ALT) {
                kCall(keyboard_func, keyCode, mod_keys, (mod_keys & KB_MOD_ALT) != 0);
            }
            if(mod_keys_aux & KB_MOD_SUPER) {
                kCall(keyboard_func, keyCode, mod_keys, (mod_keys & KB_MOD_SUPER) != 0);
            }
            if(mod_keys_aux & KB_MOD_NUM_LOCK) {
                kCall(keyboard_func, keyCode, mod_keys, (mod_keys & KB_MOD_NUM_LOCK) != 0);
            }
        }
    }
    window_data->mod_keys = mod_keys;

    [super flagsChanged:event];
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

- (void)keyDown:(NSEvent *)event
{
    short int keyCode = keycodes[[event keyCode] & 0x1ff];
    kCall(keyboard_func, keyCode, window_data->mod_keys, true);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

- (void)keyUp:(NSEvent *)event
{
    short int keyCode = keycodes[[event keyCode] & 0x1ff];
    kCall(keyboard_func, keyCode, window_data->mod_keys, false);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

- (void)insertText:(id)string replacementRange:(NSRange)replacementRange
{
    NSString    *characters;
    NSUInteger  length;
    
    kUnused(replacementRange);

    if ([string isKindOfClass:[NSAttributedString class]])
        characters = [string string];
    else
        characters = (NSString*) string;

    length = [characters length];
    for (NSUInteger i = 0;  i < length;  i++)
    {
        const unichar code = [characters characterAtIndex:i];
        if ((code & 0xff00) == 0xf700)
            continue;

        kCall(char_input_func, code);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

- (void)mainWindowChanged:(NSNotification *)notification
{
    kUnused(notification);

    SWindowData_OSX *window_data_osx = (SWindowData_OSX *) window_data->specific;
    if(window_data_osx->active == true) {
        window_data_osx->active = false;
        kCall(active_func, false);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

- (void)setContentView:(NSView *)aView
{
    if ([childContentView isEqualTo:aView])
    {
        return;
    }
    NSRect bounds = [self frame];
    bounds.origin = NSZeroPoint;

    OSXWindowFrameView *frameView = [super contentView];
    if (!frameView)
    {
        frameView = [[[OSXWindowFrameView alloc] initWithFrame:bounds] autorelease];
        
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
    kCall(active_func, true);
}

- (void)windowDidResignKey:(NSNotification *)notification
{
    kUnused(notification);
    kCall(active_func, false);
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
    window_data->close = true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

- (void)windowDidResize:(NSNotification *)notification {
    kUnused(notification);
    CGSize size = [self contentRectForFrameRect:[self frame]].size;

    window_data->window_width  = size.width;
    window_data->window_height = size.height;

    kCall(resize_func, size.width, size.height);
}

@end
