#import <Cocoa/Cocoa.h>

#if defined(USE_METAL_API)
#import <MetalKit/MetalKit.h>

// Number of textures in flight (tripple buffered)
static const int MaxBuffersInFlight = 3;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

@interface WindowViewController : NSViewController<MTKViewDelegate> 
{
	@public id<MTLTexture> m_texture_buffers[MaxBuffersInFlight]; 
	@public int m_current_buffer;
	@public void* m_draw_buffer;
	@public int m_width;
	@public int m_height;
	// Used for syncing with CPU/GPU
	@public dispatch_semaphore_t m_semaphore;
}

@end
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

@interface OSXWindowFrameView : NSView
{
#if defined(USE_METAL_API)
	@private NSTrackingArea* trackingArea;
#endif
}

@end

