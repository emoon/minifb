#import <Cocoa/Cocoa.h>
#include <WindowData.h>
// @class OSXWindowFrameView;

@interface OSXWindow : NSWindow<NSWindowDelegate>
{
    NSView              *childContentView;
    @public SWindowData *window_data;
}

- (id)initWithContentRect:(NSRect)contentRect
                styleMask:(NSWindowStyleMask)windowStyle
                  backing:(NSBackingStoreType)bufferingType
                    defer:(BOOL)deferCreation
               windowData:(SWindowData *) windowData;

- (void) removeWindowData;
@end
