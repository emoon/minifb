#pragma once

#if defined(USE_METAL_API)
#include "WindowData_OSX.h"
#import <MetalKit/MetalKit.h>

// Number of textures in flight (tripple buffered)
enum { MaxBuffersInFlight = 3 };

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

@interface OSXViewController : NSViewController<MTKViewDelegate> 
{
    @public SWindowData             *window_data;
    @public SWindowData_OSX         *window_data_osx;

    @public id<MTLDevice>           metal_device;
    @public id<MTLLibrary>          metal_library;
    @public id<MTLTexture>          texture_buffers[MaxBuffersInFlight]; 
    @public int                     current_buffer;
    @public dispatch_semaphore_t    semaphore;    // Used for syncing with CPU/GPU
}

- (id) initWithWindowData:(SWindowData *) windowData;

@end
#endif
