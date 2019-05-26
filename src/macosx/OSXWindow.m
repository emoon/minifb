#import "OSXWindow.h"
#import "OSXWindowFrameView.h"
#include "OSXWindowData.h"
#include <MiniFB_internal.h>
#include <MiniFB_enums.h>

extern SWindowData  g_window_data;
extern short int    g_keycodes[512];

bool gActive = false;

@implementation OSXWindow

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

- (id)initWithContentRect:(NSRect)contentRect
    styleMask:(NSWindowStyleMask)windowStyle
    backing:(NSBackingStoreType)bufferingType
    defer:(BOOL)deferCreation
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
    }
    return self;
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

-(void)flagsChanged:(NSEvent *)event
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

    if(mod_keys != g_window_data.mod_keys) {
        short int keyCode = keycodes[[event keyCode] & 0x1ff];
        if(keyCode != KB_KEY_UNKNOWN) {
            mod_keys_aux = mod_keys ^ g_window_data.mod_keys;
            if(mod_keys_aux & KB_MOD_CAPS_LOCK) {
                kCall(g_keyboard_func, keyCode, mod_keys, (mod_keys & KB_MOD_CAPS_LOCK) != 0);
            }
            if(mod_keys_aux & KB_MOD_SHIFT) {
                kCall(g_keyboard_func, keyCode, mod_keys, (mod_keys & KB_MOD_SHIFT) != 0);
            }
            if(mod_keys_aux & KB_MOD_CONTROL) {
                kCall(g_keyboard_func, keyCode, mod_keys, (mod_keys & KB_MOD_CONTROL) != 0);
            }
            if(mod_keys_aux & KB_MOD_ALT) {
                kCall(g_keyboard_func, keyCode, mod_keys, (mod_keys & KB_MOD_ALT) != 0);
            }
            if(mod_keys_aux & KB_MOD_SUPER) {
                kCall(g_keyboard_func, keyCode, mod_keys, (mod_keys & KB_MOD_SUPER) != 0);
            }
            if(mod_keys_aux & KB_MOD_NUM_LOCK) {
                kCall(g_keyboard_func, keyCode, mod_keys, (mod_keys & KB_MOD_NUM_LOCK) != 0);
            }
        }
    }
    g_window_data.mod_keys = mod_keys;
    //NSLog(@"KeyCode: %d (%x) - %x", [event keyCode], [event keyCode], flags);

    [super flagsChanged:event];
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

- (void)keyDown:(NSEvent *)event
{
    short int keyCode = keycodes[[event keyCode] & 0x1ff];
    kCall(g_keyboard_func, keyCode, g_window_data.mod_keys, true);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

- (void)keyUp:(NSEvent *)event
{
    short int keyCode = keycodes[[event keyCode] & 0x1ff];
    kCall(g_keyboard_func, keyCode, g_window_data.mod_keys, false);
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

        kCall(g_char_input_func, code);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

- (void)mainWindowChanged:(NSNotification *)notification
{
    kUnused(notification);

    if(gActive == true) {
        gActive = false;
        kCall(g_active_func, false);
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
    kCall(g_active_func, true);
}

- (void)windowDidResignKey:(NSNotification *)notification
{
    kUnused(notification);
    kCall(g_active_func, false);
}

- (void)windowWillClose:(NSNotification *)notification {
    kUnused(notification);
    g_window_data.close = true;
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
    g_window_data.close = true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

- (void)windowDidResize:(NSNotification *)notification {
    kUnused(notification);
    CGSize size = [self contentRectForFrameRect:[self frame]].size;

    g_window_data.window_width  = size.width;
    g_window_data.window_height = size.height;

    kCall(g_resize_func, size.width, size.height);
}

@end
