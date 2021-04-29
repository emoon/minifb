#include "OSXViewDelegate.h"

#if defined(USE_METAL_API)

#import <MetalKit/MetalKit.h>

extern double   g_time_for_frame;
extern bool     g_use_hardware_sync;
//--
bool    g_target_fps_changed = true;

//-------------------------------------
void
set_target_fps_aux() {
    g_target_fps_changed = true;
}

//-------------------------------------
#define kShader(inc, src)    @inc#src

NSString *g_shader_src = kShader(
    "#include <metal_stdlib>\n",
    using namespace metal;

    //-------------
    struct VertexOutput {
        float4 pos [[position]];
        float2 texcoord;
    };

    //-------------
    struct Vertex {
        float4 position [[position]];
    };

    //-------------
    vertex VertexOutput
    vertFunc(unsigned int vID[[vertex_id]], const device Vertex *pos [[ buffer(0) ]]) {
        VertexOutput out;

        out.pos = pos[vID].position;

        out.texcoord.x = (float) (vID / 2);
        out.texcoord.y = 1.0 - (float) (vID % 2);

        return out;
    }

    //-------------
    fragment float4
    fragFunc(VertexOutput input [[stage_in]], texture2d<half> colorTexture [[ texture(0) ]]) {
        constexpr sampler textureSampler(mag_filter::nearest, min_filter::nearest);

        // Sample the texture to obtain a color
        const half4 colorSample = colorTexture.sample(textureSampler, input.texcoord);

        // We return the color of the texture
        return float4(colorSample);
    };
);

//-------------------------------------
@implementation OSXViewDelegate

//-------------------------------------
- (id) initWithWindowData:(SWindowData *) windowData {
    self = [super init];
    if (self) {
        window_data     = windowData;
        window_data_osx = (SWindowData_OSX *) windowData->specific;

        metal_device = MTLCreateSystemDefaultDevice();
        if (!metal_device) {
            NSLog(@"Metal is not supported on this device");
            return 0x0;
        }

        // Used for syncing the CPU and GPU
        semaphore = dispatch_semaphore_create(MaxBuffersInFlight);

        // Setup command queue
        command_queue = [metal_device newCommandQueue];

        // MacOS Mojave is ignoring view.preferredFramesPerSecond
        // MacOS Big Sur is ignoring commandBuffer:presentDrawable:afterMinimumDuration:
        //id<MTLCommandBuffer> commandBuffer = [command_queue commandBuffer];
        //if ([commandBuffer respondsToSelector:@selector(presentDrawable:afterMinimumDuration:)]) {
        //    g_use_hardware_sync  = true;
        //}

        [self _createShaders];
        [self _createAssets];
    }
    return self;
}

//-------------------------------------
- (bool) _createShaders {
    NSError *error = 0x0;

    metal_library = [metal_device newLibraryWithSource:g_shader_src
                                               options:[[MTLCompileOptions alloc] init]
                                                 error:&error
    ];
    if (error || !metal_library) {
        NSLog(@"Unable to create shaders %@", error);
        return false;
    }

    id<MTLFunction> vertex_shader_func   = [metal_library newFunctionWithName:@"vertFunc"];
    id<MTLFunction> fragment_shader_func = [metal_library newFunctionWithName:@"fragFunc"];

    if (!vertex_shader_func) {
        NSLog(@"Unable to get vertFunc!\n");
        return false;
    }

    if (!fragment_shader_func) {
        NSLog(@"Unable to get fragFunc!\n");
        return false;
    }

    // Create a reusable pipeline state
    MTLRenderPipelineDescriptor *pipelineStateDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
    pipelineStateDescriptor.label = @"MiniFB_pipeline";
    pipelineStateDescriptor.vertexFunction = vertex_shader_func;
    pipelineStateDescriptor.fragmentFunction = fragment_shader_func;
    pipelineStateDescriptor.colorAttachments[0].pixelFormat = 80; //bgra8Unorm;

    pipeline_state = [metal_device newRenderPipelineStateWithDescriptor:pipelineStateDescriptor error:&error];
    if (!pipeline_state) {
        NSLog(@"Failed to created pipeline state, error %@", error);
        return false;
    }

    return true;
}

//-------------------------------------
- (void) _createAssets {
    static Vertex s_vertices[4] = {
        {-1.0, -1.0, 0, 1},
        {-1.0,  1.0, 0, 1},
        { 1.0, -1.0, 0, 1},
        { 1.0,  1.0, 0, 1},
    };
    memcpy(window_data_osx->metal.vertices, s_vertices, sizeof(s_vertices));

    MTLTextureDescriptor    *td;
    td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                            width:window_data->buffer_width
                                                           height:window_data->buffer_height
                                                        mipmapped:false];

    // Create the texture from the device by using the descriptor
    for (size_t i = 0; i < MaxBuffersInFlight; ++i) {
        texture_buffers[i] = [metal_device newTextureWithDescriptor:td];
    }
}

//-------------------------------------
- (void) resizeTextures {
    MTLTextureDescriptor    *td;
    td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                            width:window_data->buffer_width
                                                           height:window_data->buffer_height
                                                        mipmapped:false];

    // Create the texture from the device by using the descriptor
    for (size_t i = 0; i < MaxBuffersInFlight; ++i) {
        [texture_buffers[i] release];
        texture_buffers[i] = [metal_device newTextureWithDescriptor:td];
    }
}

//-------------------------------------
- (void) drawInMTKView:(nonnull MTKView *) view {
    if (g_target_fps_changed) {
        // MacOS is ignoring this :(
        if (g_time_for_frame == 0) {
            // Contrary to what is stated in the documentation,
            // 0 means that it does not update. Like pause.
            view.preferredFramesPerSecond = 9999;
        }
        else {
            view.preferredFramesPerSecond = (int) (1.0 / g_time_for_frame);
        }
        g_target_fps_changed = false;
    }

    // Wait to ensure only MaxBuffersInFlight number of frames are getting proccessed
    // by any stage in the Metal pipeline (App, Metal, Drivers, GPU, etc)
    dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);

    current_buffer = (current_buffer + 1) % MaxBuffersInFlight;

    // Create a new command buffer for each render pass to the current drawable
    id<MTLCommandBuffer> commandBuffer = [command_queue commandBuffer];
    commandBuffer.label = @"minifb_command_buffer";

    // Add completion hander which signals semaphore when Metal and the GPU has fully
    // finished processing the commands we're encoding this frame.  This indicates when the
    // dynamic buffers filled with our vertices, that we're writing to this frame, will no longer
    // be needed by Metal and the GPU, meaning we can overwrite the buffer contents without
    // corrupting the rendering.
    __block dispatch_semaphore_t block_sema = semaphore;
    [commandBuffer addCompletedHandler:^(id<MTLCommandBuffer> buffer) {
        (void)buffer;
        dispatch_semaphore_signal(block_sema);
    }];

    // Copy the bytes from our data object into the texture
    MTLRegion region = { { 0, 0, 0 }, { window_data->buffer_width, window_data->buffer_height, 1 } };
    [texture_buffers[current_buffer] replaceRegion:region mipmapLevel:0 withBytes:window_data->draw_buffer bytesPerRow:window_data->buffer_stride];

    // Delay getting the currentRenderPassDescriptor until absolutely needed. This avoids
    // holding onto the drawable and blocking the display pipeline any longer than necessary
    MTLRenderPassDescriptor* renderPassDescriptor = view.currentRenderPassDescriptor;
    if (renderPassDescriptor != nil) {
		renderPassDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 1.0);

        // Create a render command encoder so we can render into something
        id<MTLRenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
        renderEncoder.label = @"minifb_command_encoder";

        // Set render command encoder state
        [renderEncoder setRenderPipelineState:pipeline_state];
        [renderEncoder setVertexBytes:window_data_osx->metal.vertices length:sizeof(window_data_osx->metal.vertices) atIndex:0];

        [renderEncoder setFragmentTexture:texture_buffers[current_buffer] atIndex:0];

        // Draw the vertices of our quads
        [renderEncoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];

        // We're done encoding commands
        [renderEncoder endEncoding];

        // Schedule a present once the framebuffer is complete using the current drawable
        //if ([commandBuffer respondsToSelector:@selector(presentDrawable:afterMinimumDuration:)]) {
        //    // MacOS Big Sur is ignoring this
        //    [commandBuffer presentDrawable:view.currentDrawable afterMinimumDuration:g_time_for_frame];
        //}
        //else {
            [commandBuffer presentDrawable:view.currentDrawable];
        //}
    }

    // Finalize rendering here & push the command buffer to the GPU
    [commandBuffer commit];
}

//-------------------------------------
- (void) mtkView:(nonnull MTKView *)view drawableSizeWillChange:(CGSize)size {
	(void)view;
	(void)size;
    // resize
}

@end

#endif
