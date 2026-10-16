#include "OSXViewDelegate.h"

#if defined(USE_METAL_API)

#import <MetalKit/MetalKit.h>
#include <MiniFB_internal.h>

#if defined(MINIFB_USE_MMGR)
    #include "mmgr.h"
#endif

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
#define kShaderString(src)  #src
#define kShader(inc, src)   @inc kShaderString(src)

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
    fragFunc(VertexOutput input [[stage_in]], texture2d<half> colorTexture [[ texture(0) ]],
             sampler textureSampler [[ sampler(0) ]]) {

        // Sample the texture to obtain a color
        const half4 colorSample = colorTexture.sample(textureSampler, input.texcoord);

        // We return the color of the texture
        return float4(colorSample);
    };
);

//-------------------------------------
// A frame still on the GPU signals the semaphore when it completes. Freeing the semaphore before
// that makes libdispatch abort, and the late signal would write to freed memory, so wait for
// those frames. Returns false if the GPU does not finish them in time.
//-------------------------------------
static bool
wait_for_frames_in_flight(dispatch_semaphore_t semaphore) {
    long acquired = 0;
    while (acquired < MaxBuffersInFlight &&
           dispatch_semaphore_wait(semaphore, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC)) == 0) {
        ++acquired;
    }

    // libdispatch also aborts when the value ends below the one the semaphore was created with.
    for (long i = 0; i < acquired; ++i) {
        dispatch_semaphore_signal(semaphore);
    }

    return acquired == MaxBuffersInFlight;
}

//-------------------------------------
@implementation OSXViewDelegate

//-------------------------------------
- (id) initWithWindowData:(SWindowData *) windowData {
    if (windowData == NULL || windowData->specific == NULL) {
        MFB_LOG(MFB_LOG_ERROR, "OSXViewDelegate: initWithWindowData called with invalid window data.");
        return nil;
    }

    self = [super init];
    if (self) {
        window_data     = windowData;
        window_data_specific = (SWindowData_OSX *) windowData->specific;
        current_buffer  = -1;

        metal_device = MTLCreateSystemDefaultDevice();
        if (!metal_device) {
            MFB_LOG(MFB_LOG_ERROR, "OSXViewDelegate: Metal is not supported on this device.");
            MFB_OBJC_RELEASE(self);
            return nil;
        }

        // Used for syncing the CPU and GPU
        semaphore = dispatch_semaphore_create(MaxBuffersInFlight);
        if (!semaphore) {
            MFB_LOG(MFB_LOG_ERROR, "OSXViewDelegate: unable to create Metal frame semaphore.");
            MFB_OBJC_RELEASE(self);
            return nil;
        }

        // Setup command queue
        command_queue = [metal_device newCommandQueue];
        if (!command_queue) {
            MFB_LOG(MFB_LOG_ERROR, "OSXViewDelegate: unable to create Metal command queue.");
            MFB_OBJC_RELEASE(self);
            return nil;
        }

        // MacOS Mojave is ignoring view.preferredFramesPerSecond
        // MacOS Big Sur is ignoring commandBuffer:presentDrawable:afterMinimumDuration:
        //id<MTLCommandBuffer> commandBuffer = [command_queue commandBuffer];
        //if ([commandBuffer respondsToSelector:@selector(presentDrawable:afterMinimumDuration:)]) {
        //    g_use_hardware_sync  = true;
        //}

        if (![self _createShaders]) {
            MFB_OBJC_RELEASE(self);
            return nil;
        }
        if (![self _createAssets]) {
            MFB_OBJC_RELEASE(self);
            return nil;
        }
    }
    return self;
}

//-------------------------------------
- (bool) _createShaders {
    NSError *error = 0x0;
    MTLCompileOptions *options = [[MTLCompileOptions alloc] init];

    metal_library = [metal_device newLibraryWithSource:g_shader_src
                                               options:options
                                                 error:&error
    ];
    MFB_OBJC_RELEASE(options);
    if (error || !metal_library) {
        MFB_LOG(MFB_LOG_ERROR, "OSXViewDelegate: unable to create shaders (%s).",
                (error && [error localizedDescription] != nil) ? [[error localizedDescription] UTF8String] : "unknown error");
        return false;
    }

    id<MTLFunction> vertex_shader_func = [metal_library newFunctionWithName:@"vertFunc"];
    if (!vertex_shader_func) {
        MFB_LOG(MFB_LOG_ERROR, "OSXViewDelegate: unable to get vertex shader function 'vertFunc'.");
        return false;
    }

    id<MTLFunction> fragment_shader_func = [metal_library newFunctionWithName:@"fragFunc"];
    if (!fragment_shader_func) {
        MFB_LOG(MFB_LOG_ERROR, "OSXViewDelegate: unable to get fragment shader function 'fragFunc'.");
        MFB_OBJC_RELEASE(vertex_shader_func);
        return false;
    }

    // Create a reusable pipeline state
    MTLRenderPipelineDescriptor *pipelineStateDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
    pipelineStateDescriptor.label = @"MiniFB_pipeline";
    pipelineStateDescriptor.vertexFunction = vertex_shader_func;
    pipelineStateDescriptor.fragmentFunction = fragment_shader_func;
    pipelineStateDescriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;

    pipeline_state = [metal_device newRenderPipelineStateWithDescriptor:pipelineStateDescriptor error:&error];
    MFB_OBJC_RELEASE(pipelineStateDescriptor);
    MFB_OBJC_RELEASE(vertex_shader_func);
    MFB_OBJC_RELEASE(fragment_shader_func);
    if (!pipeline_state) {
        MFB_LOG(MFB_LOG_ERROR, "OSXViewDelegate: failed to create pipeline state (%s).",
                (error && [error localizedDescription] != nil) ? [[error localizedDescription] UTF8String] : "unknown error");
        return false;
    }

    return true;
}

//-------------------------------------
- (bool) _createAssets {
    if (window_data == NULL || window_data_specific == NULL) {
        MFB_LOG(MFB_LOG_ERROR, "OSXViewDelegate: invalid window state while creating assets.");
        return false;
    }

    static Vertex s_vertices[4] = {
        {-1.0, -1.0, 0, 1},
        {-1.0,  1.0, 0, 1},
        { 1.0, -1.0, 0, 1},
        { 1.0,  1.0, 0, 1},
    };
    memcpy(window_data_specific->metal.vertices, s_vertices, sizeof(s_vertices));

    MTLSamplerDescriptor *sampler_descriptor = [[MTLSamplerDescriptor alloc] init];
    sampler_descriptor.sAddressMode = MTLSamplerAddressModeClampToEdge;
    sampler_descriptor.tAddressMode = MTLSamplerAddressModeClampToEdge;
    sampler_descriptor.minFilter = MTLSamplerMinMagFilterNearest;
    sampler_descriptor.magFilter = MTLSamplerMinMagFilterNearest;
    nearest_sampler = [metal_device newSamplerStateWithDescriptor:sampler_descriptor];
    sampler_descriptor.minFilter = MTLSamplerMinMagFilterLinear;
    sampler_descriptor.magFilter = MTLSamplerMinMagFilterLinear;
    bilinear_sampler = [metal_device newSamplerStateWithDescriptor:sampler_descriptor];

#if !__has_feature(objc_arc)
    [sampler_descriptor release];
#endif

    if (nearest_sampler == nil || bilinear_sampler == nil) {
        MFB_LOG(MFB_LOG_ERROR, "Cannot create Metal scaling samplers.");
        return false;
    }

    MTLTextureDescriptor    *td;
    td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                            width:window_data->buffer_width
                                                           height:window_data->buffer_height
                                                        mipmapped:false];

    // Create the texture from the device by using the descriptor
    for (size_t i = 0; i < MaxBuffersInFlight; ++i) {
        texture_buffers[i] = [metal_device newTextureWithDescriptor:td];
        if (texture_buffers[i] == nil) {
            MFB_LOG(MFB_LOG_ERROR, "OSXViewDelegate: unable to create Metal texture buffer %zu.", i);
            for (size_t j = 0; j < i; ++j) {
                MFB_OBJC_RELEASE(texture_buffers[j]);
                texture_buffers[j] = nil;
            }
            return false;
        }
    }

    return true;
}

//-------------------------------------
- (bool) resizeTextures {
    if (window_data == NULL || metal_device == nil) {
        MFB_LOG(MFB_LOG_ERROR, "OSXViewDelegate: resizeTextures called with invalid window state.");
        return false;
    }

    MTLTextureDescriptor    *td;
    td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                            width:window_data->buffer_width
                                                           height:window_data->buffer_height
                                                        mipmapped:false];

    id<MTLTexture> new_textures[MaxBuffersInFlight] = { nil };

    // Create replacement textures first, then swap atomically to avoid partial state.
    for (size_t i = 0; i < MaxBuffersInFlight; ++i) {
        new_textures[i] = [metal_device newTextureWithDescriptor:td];
        if (new_textures[i] == nil) {
            MFB_LOG(MFB_LOG_ERROR, "OSXViewDelegate: unable to resize Metal texture buffer %zu.", i);
            for (size_t j = 0; j < i; ++j) {
                MFB_OBJC_RELEASE(new_textures[j]);
                new_textures[j] = nil;
            }
            return false;
        }
    }

    for (size_t i = 0; i < MaxBuffersInFlight; ++i) {
        MFB_OBJC_RELEASE(texture_buffers[i]);
        texture_buffers[i] = new_textures[i];
    }

    return true;
}

//-------------------------------------
- (void) drawInMTKView:(nonnull MTKView *) view {
    if (window_data == NULL || window_data_specific == NULL ||
        window_data->draw_buffer == NULL || window_data->buffer_width == 0 || window_data->buffer_height == 0) {
        return;
    }

    mfb_stretch_mode stretch_mode;
    @synchronized(self) {
        stretch_mode = window_data->stretch_mode;
    }

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

    // Avoid blocking the main thread during live resize when GPU presents can stall.
    // If no in-flight slot is available, skip this frame and keep the UI responsive.
    if (dispatch_semaphore_wait(semaphore, DISPATCH_TIME_NOW) != 0) {
        return;
    }

    current_buffer = (current_buffer + 1) % MaxBuffersInFlight;

    // Create a new command buffer for each render pass to the current drawable
    id<MTLCommandBuffer> commandBuffer = [command_queue commandBuffer];
    if (commandBuffer == nil || texture_buffers[current_buffer] == nil) {
        MFB_LOG(MFB_LOG_WARNING, "OSXViewDelegate: skipping frame because command buffer or texture is unavailable.");
        dispatch_semaphore_signal(semaphore);
        return;
    }
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
    MTLRenderPassDescriptor *renderPassDescriptor = view.currentRenderPassDescriptor;
    if (renderPassDescriptor != nil) {
        renderPassDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 1.0);

        // Create a render command encoder so we can render into something
        id<MTLRenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
        if (renderEncoder == nil) {
            MFB_LOG(MFB_LOG_ERROR, "OSXViewDelegate: failed to create Metal render encoder.");
            [commandBuffer commit];
            return;
        }
        renderEncoder.label = @"minifb_command_encoder";

        // Set render command encoder state
        [renderEncoder setRenderPipelineState:pipeline_state];
        [renderEncoder setVertexBytes:window_data_specific->metal.vertices length:sizeof(window_data_specific->metal.vertices) atIndex:0];

        [renderEncoder setFragmentTexture:texture_buffers[current_buffer] atIndex:0];
        [renderEncoder setFragmentSamplerState:(stretch_mode == MFB_STRETCH_BILINEAR ? bilinear_sampler : nearest_sampler) atIndex:0];

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
            id<CAMetalDrawable> drawable = view.currentDrawable;
            if (drawable == nil) {
                MFB_LOG(MFB_LOG_WARNING, "OSXViewDelegate: skipping present because currentDrawable is nil.");
            }
            else {
                [commandBuffer presentDrawable:drawable];
            }
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

//-------------------------------------
- (void)dealloc {
    bool frames_done = (semaphore == nil || wait_for_frames_in_flight(semaphore) == true);
    if (frames_done == false) {
        MFB_LOG(MFB_LOG_WARNING, "OSXViewDelegate: the GPU did not finish its frames in time, so the frame semaphore is kept.");
    }

    for (size_t i = 0; i < MaxBuffersInFlight; ++i) {
        MFB_OBJC_RELEASE(texture_buffers[i]);
        texture_buffers[i] = nil;
    }

    MFB_OBJC_RELEASE(nearest_sampler);
    nearest_sampler = nil;
    MFB_OBJC_RELEASE(bilinear_sampler);
    bilinear_sampler = nil;

    MFB_OBJC_RELEASE(pipeline_state);
    pipeline_state = nil;

    MFB_OBJC_RELEASE(command_queue);
    command_queue = nil;

    MFB_OBJC_RELEASE(metal_library);
    metal_library = nil;

    MFB_OBJC_RELEASE(metal_device);
    metal_device = nil;

    // Under ARC, dispatch objects are Objective-C objects and ARC releases them.
#if !(__has_feature(objc_arc) && OS_OBJECT_USE_OBJC)
    if (semaphore != nil && frames_done == true) {
        dispatch_release(semaphore);
    }
#endif
    semaphore = nil;

#if !__has_feature(objc_arc)
    [super dealloc];
#endif
}

@end

#endif
