//
//  Renderer.m
//  MiniFB
//
//  Created by Carlos Aragones on 22/04/2020.
//  Copyright © 2020 Carlos Aragones. All rights reserved.
//

#import <simd/simd.h>
#import <ModelIO/ModelIO.h>

#import "iOSViewDelegate.h"
#include "WindowData_IOS.h"
#include <MiniFB.h>
#include <MiniFB_ios.h>
#include <MiniFB_internal.h>

//-------------------------------------
#define kShader(inc, src)    @inc#src

//-------------------------------------
enum { MaxBuffersInFlight = 3 };    // Number of textures in flight (tripple buffered)

//--
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
static const char *
metal_error_description(NSError *error) {
    if (error == nil) {
        return "unknown error";
    }

    NSString *description = [error localizedDescription];
    if (description == nil) {
        return "unknown error";
    }

    const char *utf8 = [description UTF8String];
    return utf8 != NULL ? utf8 : "unknown error";
}

//-------------------------------------
@implementation iOSViewDelegate {
    SWindowData                 *window_data;
    SWindowData_IOS             *window_data_ios;

    id<MTLDevice>               metal_device;
    id<MTLLibrary>              metal_library;

    dispatch_semaphore_t        semaphore;
    id<MTLCommandQueue>         command_queue;

    id<MTLRenderPipelineState>  pipeline_state;
    id<MTLTexture>              texture_buffers[MaxBuffersInFlight];

    uint8_t                     current_buffer;
}

//-------------------------------------
-(nonnull instancetype) initWithMetalKitView:(nonnull MTKView *) view windowData:(nonnull SWindowData *) windowData {
    self = [super init];
    if (self) {
        window_data     = windowData;
        window_data_ios = (SWindowData_IOS *) windowData->specific;

        view.colorPixelFormat = MTLPixelFormatBGRA8Unorm;
        view.sampleCount      = 1;

        metal_device  = view.device;
        current_buffer = (uint8_t)(MaxBuffersInFlight - 1);

        // Used for syncing the CPU and GPU
        semaphore = dispatch_semaphore_create(MaxBuffersInFlight);

        // Setup command queue
        command_queue = [metal_device newCommandQueue];

        [self _createShaders];
        [self _createAssets];
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

#if !__has_feature(objc_arc)
    [options release];
#endif

    if (error || !metal_library) {
        mfb_log(MFB_LOG_ERROR, "iOSViewDelegate: unable to create shaders: %s", metal_error_description(error));
        return false;
    }

    id<MTLFunction> vertex_shader_func   = [metal_library newFunctionWithName:@"vertFunc"];
    id<MTLFunction> fragment_shader_func = [metal_library newFunctionWithName:@"fragFunc"];

    if (!vertex_shader_func) {
        mfb_log(MFB_LOG_ERROR, "iOSViewDelegate: unable to find vertex function 'vertFunc'.");
        return false;
    }

    if (!fragment_shader_func) {
        mfb_log(MFB_LOG_ERROR, "iOSViewDelegate: unable to find fragment function 'fragFunc'.");

#if !__has_feature(objc_arc)
        [vertex_shader_func release];
#endif

        return false;
    }

    // Create a reusable pipeline state
    MTLRenderPipelineDescriptor *pipelineStateDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
    pipelineStateDescriptor.label = @"MiniFB_pipeline";
    pipelineStateDescriptor.vertexFunction = vertex_shader_func;
    pipelineStateDescriptor.fragmentFunction = fragment_shader_func;
    pipelineStateDescriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;

    pipeline_state = [metal_device newRenderPipelineStateWithDescriptor:pipelineStateDescriptor error:&error];

#if !__has_feature(objc_arc)
    [pipelineStateDescriptor release];
    [vertex_shader_func release];
    [fragment_shader_func release];
#endif

    if (!pipeline_state) {
        mfb_log(MFB_LOG_ERROR, "iOSViewDelegate: failed to create pipeline state: %s", metal_error_description(error));
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
    memcpy(window_data_ios->vertices, s_vertices, sizeof(s_vertices));

    MTLTextureDescriptor    *td;
    td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                            width:window_data->buffer_width
                                                           height:window_data->buffer_height
                                                        mipmapped:false];

    // Create all triple-buffered textures
    for (int i = 0; i < MaxBuffersInFlight; ++i) {
        texture_buffers[i] = [metal_device newTextureWithDescriptor:td];
    }
}

//-------------------------------------
- (bool) resizeTextures {
    if (window_data == NULL || metal_device == nil) {
        mfb_log(MFB_LOG_ERROR, "iOSViewDelegate: resizeTextures called with invalid window state.");
        return false;
    }

    MTLTextureDescriptor    *td;
    td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                            width:window_data->buffer_width
                                                           height:window_data->buffer_height
                                                        mipmapped:false];

    // Create new textures first, then release old ones to avoid dangling pointers if creation fails
    id<MTLTexture> new_textures[MaxBuffersInFlight];
    for (int i = 0; i < MaxBuffersInFlight; ++i) {
        new_textures[i] = [metal_device newTextureWithDescriptor:td];
        if (!new_textures[i]) {
            // Release any textures already created in this attempt
#if !__has_feature(objc_arc)
            for (int j = 0; j < i; ++j) {
                [new_textures[j] release];
            }
#endif
            return false;
        }
    }
    for (int i = 0; i < MaxBuffersInFlight; ++i) {
#if !__has_feature(objc_arc)
        [texture_buffers[i] release];
#endif
        texture_buffers[i] = new_textures[i];
    }

    return true;
}

//-------------------------------------
- (void) drawInMTKView:(nonnull MTKView *) view {
    // Wait to ensure only MaxBuffersInFlight number of frames are getting proccessed
    // by any stage in the Metal pipeline (App, Metal, Drivers, GPU, etc)
    if (dispatch_semaphore_wait(semaphore, DISPATCH_TIME_NOW) != 0) {
        return; // GPU is still busy, skip this frame instead of blocking
    }

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

    // Copy the bytes from our data object into the current texture slot
    MTLRegion region = { { 0, 0, 0 }, { window_data->buffer_width, window_data->buffer_height, 1 } };
    [texture_buffers[current_buffer] replaceRegion:region mipmapLevel:0 withBytes:window_data->draw_buffer bytesPerRow:window_data->buffer_stride];

    // Delay getting the currentRenderPassDescriptor until absolutely needed. This avoids
    // holding onto the drawable and blocking the display pipeline any longer than necessary
    MTLRenderPassDescriptor* renderPassDescriptor = view.currentRenderPassDescriptor;
    if (renderPassDescriptor != nil) {
        //renderPassDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 1.0);

        // Create a render command encoder so we can render into something
        id<MTLRenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
        renderEncoder.label = @"minifb_command_encoder";

        // Set render command encoder state
        [renderEncoder setRenderPipelineState:pipeline_state];
        [renderEncoder setVertexBytes:window_data_ios->vertices length:sizeof(window_data_ios->vertices) atIndex:0];

        [renderEncoder setFragmentTexture:texture_buffers[current_buffer] atIndex:0];

        // Draw the vertices of our quads
        [renderEncoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];

        // We're done encoding commands
        [renderEncoder endEncoding];

        // Schedule a present once the framebuffer is complete using the current drawable
        [commandBuffer presentDrawable:view.currentDrawable];
    }

    // Finalize rendering here & push the command buffer to the GPU
    [commandBuffer commit];
}

//-------------------------------------
- (void) dealloc {
#if !__has_feature(objc_arc)
    for (int i = 0; i < MaxBuffersInFlight; ++i) {
        [texture_buffers[i] release];
        texture_buffers[i] = nil;
    }

    [pipeline_state release];
    pipeline_state = nil;

    [command_queue release];
    command_queue = nil;

    [metal_library release];
    metal_library = nil;

    #if !OS_OBJECT_USE_OBJC
    if (semaphore) {
        dispatch_release(semaphore);
    }
    #endif
    semaphore = nil;

    [super dealloc];
#endif
}

//-------------------------------------
- (void) mtkView:(nonnull MTKView *)view drawableSizeWillChange:(CGSize)size {
    (void) view;
    // Respond to drawable size or orientation changes here
    float scale = [UIScreen mainScreen].scale;

    window_data->window_width  = size.width  * scale;
    window_data->window_height = size.height * scale;
    resize_dst(window_data, size.width, size.height);

    // Defer the resize callback to the main thread via mfb_update_ex / mfb_update_events,
    // which are called from user code. This avoids invoking the callback from the
    // CADisplayLink render thread.
    window_data->must_resize_context = true;
}

@end
