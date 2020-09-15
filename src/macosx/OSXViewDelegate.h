#pragma once

#if defined(USE_METAL_API)

#import <MetalKit/MetalKit.h>
#include "WindowData_OSX.h"

// Number of textures in flight (tripple buffered)
enum { MaxBuffersInFlight = 3 };

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

@interface OSXViewDelegate : NSViewController<MTKViewDelegate>
{
    @public SWindowData         *window_data;
    @public SWindowData_OSX     *window_data_osx;

    id<MTLDevice>               metal_device;
    id<MTLLibrary>              metal_library;

    dispatch_semaphore_t        semaphore;    // Used for syncing with CPU/GPU
    id<MTLCommandQueue>         command_queue;

    id<MTLRenderPipelineState>  pipeline_state;
    id<MTLTexture>              texture_buffers[MaxBuffersInFlight];

    int                         current_buffer;
}

- (id) initWithWindowData:(SWindowData *) windowData;
- (void) resizeTextures;

@end

#endif
