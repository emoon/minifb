#import <MetalKit/MetalKit.h>
#include "WindowData.h"

@interface iOSView : MTKView
{
    @public SWindowData         *window_data;
    @public NSMutableDictionary *touch_to_button; // Maps UITouch * -> button index (NSNumber)
}

@end
