#import <Cocoa/Cocoa.h>

#include "WindowData.h"

//-------------------------------------
@interface OSXView : NSView<NSTextInputClient>
{
    @public  SWindowData     *window_data;
    @private NSTrackingArea  *tracking_area;
}

@end
