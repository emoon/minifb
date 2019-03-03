#import "OSXWindowFrameView.h"

#if defined(USE_METAL_API)
#import <MetalKit/MetalKit.h>

id<MTLDevice> g_metal_device;
id<MTLCommandQueue> g_command_queue;
id<MTLLibrary> g_library;
id<MTLRenderPipelineState> g_pipeline_state;

@implementation WindowViewController

-(void)mtkView:(nonnull MTKView *)view drawableSizeWillChange:(CGSize)size
{
	(void)view;
	(void)size;
    // resize
}

-(void)drawInMTKView:(nonnull MTKView *)view
{
    // Wait to ensure only MaxBuffersInFlight number of frames are getting proccessed
    //   by any stage in the Metal pipeline (App, Metal, Drivers, GPU, etc)
    dispatch_semaphore_wait(m_semaphore, DISPATCH_TIME_FOREVER);

    // Iterate through our Metal buffers, and cycle back to the first when we've written to MaxBuffersInFlight
    m_current_buffer = (m_current_buffer + 1) % MaxBuffersInFlight;

    // Calculate the number of bytes per row of our image.
    NSUInteger bytesPerRow = 4 * m_width;
    MTLRegion region = { { 0, 0, 0 }, { m_width, m_height, 1 } };

    // Copy the bytes from our data object into the texture
    [m_texture_buffers[m_current_buffer] replaceRegion:region
                mipmapLevel:0 withBytes:m_draw_buffer bytesPerRow:bytesPerRow];

    // Create a new command buffer for each render pass to the current drawable
    id<MTLCommandBuffer> commandBuffer = [g_command_queue commandBuffer];
    commandBuffer.label = @"minifb_command_buffer";

    // Add completion hander which signals _inFlightSemaphore when Metal and the GPU has fully
    //   finished processing the commands we're encoding this frame.  This indicates when the
    //   dynamic buffers filled with our vertices, that we're writing to this frame, will no longer
    //   be needed by Metal and the GPU, meaning we can overwrite the buffer contents without
    //   corrupting the rendering.
    __block dispatch_semaphore_t block_sema = m_semaphore;
    [commandBuffer addCompletedHandler:^(id<MTLCommandBuffer> buffer)
    {
    	(void)buffer;
        dispatch_semaphore_signal(block_sema);
    }];

    MTLRenderPassDescriptor* renderPassDescriptor = view.currentRenderPassDescriptor;

    if (renderPassDescriptor != nil)
    {
		renderPassDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(1.0, 0.0, 0.0, 1.0);

        // Create a render command encoder so we can render into something
        id<MTLRenderCommandEncoder> renderEncoder =
        [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
        renderEncoder.label = @"minifb_command_encoder";

        // Set render command encoder state
        [renderEncoder setRenderPipelineState:g_pipeline_state];

        [renderEncoder setFragmentTexture:m_texture_buffers[m_current_buffer] atIndex:0];

        // Draw the vertices of our quads
        [renderEncoder drawPrimitives:MTLPrimitiveTypeTriangle
                          vertexStart:0
                          vertexCount:3];

        // We're done encoding commands
        [renderEncoder endEncoding];

        // Schedule a present once the framebuffer is complete using the current drawable
        [commandBuffer presentDrawable:view.currentDrawable];
    }

    // Finalize rendering here & push the command buffer to the GPU
    [commandBuffer commit];
}
@end
#endif

@implementation OSXWindowFrameView

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if defined(USE_METAL_API)
-(void)updateTrackingAreas
{
    if(trackingArea != nil) {
        [self removeTrackingArea:trackingArea];
        [trackingArea release];
    }

    int opts = (NSTrackingMouseEnteredAndExited | NSTrackingActiveAlways);
    trackingArea = [ [NSTrackingArea alloc] initWithRect:[self bounds]
                                            options:opts
                                            owner:self
                                            userInfo:nil];
    [self addTrackingArea:trackingArea];
}
#else 
extern void* g_updateBuffer;
extern int g_width;
extern int g_height;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

- (NSRect)resizeRect
{
	const CGFloat resizeBoxSize = 16.0;
	const CGFloat contentViewPadding = 5.5;
	
	NSRect contentViewRect = [[self window] contentRectForFrameRect:[[self window] frame]];
	NSRect resizeRect = NSMakeRect(
		NSMaxX(contentViewRect) + contentViewPadding,
		NSMinY(contentViewRect) - resizeBoxSize - contentViewPadding,
		resizeBoxSize,
		resizeBoxSize);
	
	return resizeRect;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

- (void)drawRect:(NSRect)rect
{
	(void)rect;

	if (!g_updateBuffer)
		return;

	CGContextRef context = [[NSGraphicsContext currentContext] graphicsPort];

	CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
	CGDataProviderRef provider = CGDataProviderCreateWithData(NULL, g_updateBuffer, g_width * g_height * 4, NULL); 

	CGImageRef img = CGImageCreate(g_width, g_height, 8, 32, g_width * 4, space, kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder32Little, 
								   provider, NULL, false, kCGRenderingIntentDefault);

	CGColorSpaceRelease(space);
	CGDataProviderRelease(provider);

	CGContextDrawImage(context, CGRectMake(0, 0, g_width, g_height), img);

	CGImageRelease(img);
}
#endif

@end

