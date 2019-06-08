#import "OSXWindowFrameView.h"
#import "OSXWindow.h"
#include "WindowData_OSX.h"
#include <MiniFB_internal.h>

#if defined(USE_METAL_API)
#import <MetalKit/MetalKit.h>

extern id<MTLDevice>  g_metal_device;
extern id<MTLLibrary> g_library;

extern Vertex gVertices[4];

@implementation WindowViewController

- (void)mtkView:(nonnull MTKView *)view drawableSizeWillChange:(CGSize)size
{
	(void)view;
	(void)size;
    // resize
}

- (void)drawInMTKView:(nonnull MTKView *)view
{
    OSXWindow   *window = (OSXWindow *) view.window;
    if(window->window_data == 0x0) {
        return;
    }

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
    SWindowData_OSX *window_data_osx = (SWindowData_OSX *) window->window_data->specific;
    id<MTLCommandBuffer> commandBuffer = [window_data_osx->metal.command_queue commandBuffer];
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
		renderPassDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 1.0);

        // Create a render command encoder so we can render into something
        id<MTLRenderCommandEncoder> renderEncoder =
        [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
        renderEncoder.label = @"minifb_command_encoder";

        // Set render command encoder state
        OSXWindow     *window          = (OSXWindow *) view.window;
        SWindowData_OSX *window_data_osx = (SWindowData_OSX *) window->window_data->specific;
        [renderEncoder setRenderPipelineState:window_data_osx->metal.pipeline_state];

        [renderEncoder setVertexBytes:gVertices
                       length:sizeof(gVertices)
                      atIndex:0];

        [renderEncoder setFragmentTexture:m_texture_buffers[m_current_buffer] atIndex:0];

        // Draw the vertices of our quads
        // [renderEncoder drawPrimitives:MTLPrimitiveTypeTriangle
        //                   vertexStart:0
        //                   vertexCount:3];
        [renderEncoder drawPrimitives:MTLPrimitiveTypeTriangleStrip
                          vertexStart:0
                          vertexCount:4];

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
- (void)updateTrackingAreas
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

    if(!window_data)
        return;

    SWindowData_OSX *window_data_osx = (SWindowData_OSX *) window_data->specific;    
	if (!window_data_osx || !window_data_osx->window || !window_data->draw_buffer)
		return;

	CGContextRef context = [[NSGraphicsContext currentContext] graphicsPort];

	CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
	CGDataProviderRef provider = CGDataProviderCreateWithData(0x0, window_data->draw_buffer, window_data->buffer_width * window_data->buffer_height * 4, 0x0); 

	CGImageRef img = CGImageCreate(window_data->buffer_width, window_data->buffer_height, 8, 32, window_data->buffer_width * 4, space, kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder32Little, 
								   provider, 0x0, false, kCGRenderingIntentDefault);

    const CGFloat components[] = {0.0f, 0.0f, 0.0f, 1.0f};
    const CGColorRef black = CGColorCreate(space, components);

	CGColorSpaceRelease(space);
	CGDataProviderRelease(provider);

    if(window_data->dst_offset_x != 0 || window_data->dst_offset_y != 0 || window_data->dst_width != window_data->window_width || window_data->dst_height != window_data->window_height) {
        CGContextSetFillColorWithColor(context, black);
        CGContextFillRect(context, CGRectMake(0, 0, window_data->window_width, window_data->window_height));
    }
    
	CGContextDrawImage(context, CGRectMake(window_data->dst_offset_x, window_data->dst_offset_y, window_data->dst_width, window_data->dst_height), img);

	CGImageRelease(img);
}
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

- (BOOL)acceptsFirstMouse:(NSEvent *)event
{
    (void)event;
    return YES;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

- (void)mouseDown:(NSEvent*)event
{
    (void)event;
    kCall(mouse_btn_func, MOUSE_BTN_1, window_data->mod_keys, true);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

- (void)mouseUp:(NSEvent*)event
{
    (void)event;
    kCall(mouse_btn_func, MOUSE_BTN_1, window_data->mod_keys, false);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

- (void)rightMouseDown:(NSEvent*)event
{
    (void)event;
    kCall(mouse_btn_func, MOUSE_BTN_2, window_data->mod_keys, true);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

- (void)rightMouseUp:(NSEvent*)event
{
    (void)event;
    kCall(mouse_btn_func, MOUSE_BTN_1, window_data->mod_keys, false);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

- (void)otherMouseDown:(NSEvent *)event
{
    (void)event;
    kCall(mouse_btn_func, [event buttonNumber], window_data->mod_keys, true);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

- (void)otherMouseUp:(NSEvent *)event
{
    (void)event;
    kCall(mouse_btn_func, [event buttonNumber], window_data->mod_keys, false);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

- (void)scrollWheel:(NSEvent *)event
{
    kCall(mouse_wheel_func, window_data->mod_keys, [event deltaX], [event deltaY]);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

- (void)mouseDragged:(NSEvent *)event
{
    [self mouseMoved:event];
}

- (void)rightMouseDragged:(NSEvent *)event
{
    [self mouseMoved:event];
}

- (void)otherMouseDragged:(NSEvent *)event
{
    [self mouseMoved:event];
}

- (void)mouseMoved:(NSEvent *)event
{
    NSPoint point = [event locationInWindow];
    //NSPoint localPoint = [self convertPoint:point fromView:nil];
    kCall(mouse_move_func, point.x, point.y);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

- (void)mouseExited:(NSEvent *)event
{
    (void)event;
    //printf("mouse exit\n");
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

- (void)mouseEntered:(NSEvent *)event
{
    (void)event;
    //printf("mouse enter\n");
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

- (BOOL)canBecomeKeyView
{
    return YES;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

- (NSView *)nextValidKeyView
{
    return self;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

- (NSView *)previousValidKeyView
{
    return self;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

- (BOOL)acceptsFirstResponder
{
    return YES;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

- (void)viewDidMoveToWindow
{
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    [super dealloc];
}

@end

