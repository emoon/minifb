#import <MetalKit/MetalKit.h>
#include "WindowData.h"

@interface iOSView : MTKView
{
    @public SWindowData *window_data;
}

@end
