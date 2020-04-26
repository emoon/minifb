#import <Cocoa/Cocoa.h>

#include "WindowData.h"

#if defined(USE_METAL_API)
#import <MetalKit/MetalKit.h>

typedef struct Vertex {
    float x, y, z, w;
} Vertex;

// Number of textures in flight (tripple buffered)
enum { MaxBuffersInFlight = 3 };

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

@interface WindowViewController : NSViewController<MTKViewDelegate> 
{
    @public id<MTLTexture>          texture_buffers[MaxBuffersInFlight]; 
    @public int                     current_buffer;
    @public dispatch_semaphore_t    semaphore;    // Used for syncing with CPU/GPU
}

@end
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

@interface OSXWindowFrameView : NSView
{
    @public SWindowData     *window_data;
#if defined(USE_METAL_API)
    @private NSTrackingArea *tracking_area;
#endif
}

@end

