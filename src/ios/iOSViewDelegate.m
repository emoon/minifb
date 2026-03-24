//
//  Renderer.m
//  MiniFB
//
//  Created by Carlos Aragones on 22/04/2020.
//  Copyright © 2020 Carlos Aragones. All rights reserved.
//

#import <simd/simd.h>
#import <ModelIO/ModelIO.h>
#include <math.h>

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
get_metal_error_description(NSError *error) {
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
static void
build_viewport_vertices(const SWindowData *window_data, Vertex out_vertices[4]) {
    if (out_vertices == NULL) {
        return;
    }

    float x1 = -1.0f;
    float y1 = -1.0f;
    float x2 =  1.0f;
    float y2 =  1.0f;

    if (window_data != NULL && window_data->window_width > 0 && window_data->window_height > 0) {
        const float inv_width  = 1.0f / (float) window_data->window_width;
        const float inv_height = 1.0f / (float) window_data->window_height;

        x1 =  ((float) window_data->dst_offset_x * inv_width)  * 2.0f - 1.0f;
        y1 =  ((float) window_data->dst_offset_y * inv_height) * 2.0f - 1.0f;
        x2 = (((float) window_data->dst_offset_x + (float) window_data->dst_width)  * inv_width)  * 2.0f - 1.0f;
        y2 = (((float) window_data->dst_offset_y + (float) window_data->dst_height) * inv_height) * 2.0f - 1.0f;
    }

    out_vertices[0].x = x1;
    out_vertices[0].y = y1;
    out_vertices[0].z = 0.0f;
    out_vertices[0].w = 1.0f;

    out_vertices[1].x = x1;
    out_vertices[1].y = y2;
    out_vertices[1].z = 0.0f;
    out_vertices[1].w = 1.0f;

    out_vertices[2].x = x2;
    out_vertices[2].y = y1;
    out_vertices[2].z = 0.0f;
    out_vertices[2].w = 1.0f;

    out_vertices[3].x = x2;
    out_vertices[3].y = y2;
    out_vertices[3].z = 0.0f;
    out_vertices[3].w = 1.0f;
}

//-------------------------------------
@implementation iOSViewDelegate {
    SWindowData                 *window_data;
    SWindowData_IOS             *window_data_specific;

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
    if (view == nil || windowData == NULL || windowData->specific == NULL) {
        MFB_LOG(MFB_LOG_ERROR, "iOSViewDelegate: initWithMetalKitView received invalid state.");
        return nil;
    }

    self = [super init];
    if (self) {
        window_data     = windowData;
        window_data_specific = (SWindowData_IOS *) windowData->specific;

        view.colorPixelFormat = MTLPixelFormatBGRA8Unorm;
        view.sampleCount      = 1;

        metal_device  = view.device;
        if (metal_device == nil) {
            MFB_LOG(MFB_LOG_ERROR, "iOSViewDelegate: MTKView has no Metal device.");
#if !__has_feature(objc_arc)
            [self release];
#endif
            return nil;
        }

        current_buffer = (uint8_t)(MaxBuffersInFlight - 1);

        // Used for syncing the CPU and GPU
        semaphore = dispatch_semaphore_create(MaxBuffersInFlight);
        if (semaphore == nil) {
            MFB_LOG(MFB_LOG_ERROR, "iOSViewDelegate: failed to create frame semaphore.");
#if !__has_feature(objc_arc)
            [self release];
#endif
            return nil;
        }

        // Setup command queue
        command_queue = [metal_device newCommandQueue];
        if (command_queue == nil) {
            MFB_LOG(MFB_LOG_ERROR, "iOSViewDelegate: failed to create command queue.");
#if !__has_feature(objc_arc)
            [self release];
#endif
            return nil;
        }

        if ([self _createShaders] == false) {
#if !__has_feature(objc_arc)
            [self release];
#endif
            return nil;
        }

        if ([self _createAssets] == false) {
#if !__has_feature(objc_arc)
            [self release];
#endif
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

#if !__has_feature(objc_arc)
    [options release];
#endif

    if (error || !metal_library) {
        MFB_LOG(MFB_LOG_ERROR, "iOSViewDelegate: unable to create shaders: %s", get_metal_error_description(error));
        return false;
    }

    id<MTLFunction> vertex_shader_func   = [metal_library newFunctionWithName:@"vertFunc"];
    id<MTLFunction> fragment_shader_func = [metal_library newFunctionWithName:@"fragFunc"];

    if (!vertex_shader_func) {
        MFB_LOG(MFB_LOG_ERROR, "iOSViewDelegate: unable to find vertex function 'vertFunc'.");
#if !__has_feature(objc_arc)
        [fragment_shader_func release];
#endif
        return false;
    }

    if (!fragment_shader_func) {
        MFB_LOG(MFB_LOG_ERROR, "iOSViewDelegate: unable to find fragment function 'fragFunc'.");

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
        MFB_LOG(MFB_LOG_ERROR, "iOSViewDelegate: failed to create pipeline state: %s", get_metal_error_description(error));
        return false;
    }

    return true;
}

//-------------------------------------
- (bool) _createAssets {
    if (window_data == NULL || window_data_specific == NULL || metal_device == nil) {
        MFB_LOG(MFB_LOG_ERROR, "iOSViewDelegate: invalid state while creating assets.");
        return false;
    }

    build_viewport_vertices(window_data, window_data_specific->vertices);

    MTLTextureDescriptor    *td;
    td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                            width:window_data->buffer_width
                                                           height:window_data->buffer_height
                                                        mipmapped:false];

    // Create all triple-buffered textures
    for (int i = 0; i < MaxBuffersInFlight; ++i) {
        texture_buffers[i] = [metal_device newTextureWithDescriptor:td];
        if (texture_buffers[i] == nil) {
            MFB_LOG(MFB_LOG_ERROR, "iOSViewDelegate: failed to create texture buffer %d.", i);
#if !__has_feature(objc_arc)
            for (int j = 0; j < i; ++j) {
                [texture_buffers[j] release];
                texture_buffers[j] = nil;
            }
#endif
            return false;
        }
    }

    return true;
}

//-------------------------------------
- (bool) resizeTextures {
    if (window_data == NULL || window_data_specific == NULL || metal_device == nil) {
        MFB_LOG(MFB_LOG_ERROR, "iOSViewDelegate: resizeTextures called with invalid window state.");
        return false;
    }

    MTLTextureDescriptor    *td;
    td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                            width:window_data->buffer_width
                                                           height:window_data->buffer_height
                                                        mipmapped:false];

    // Create new textures first (outside the lock - Metal allocation can take time)
    id<MTLTexture> new_textures[MaxBuffersInFlight];
    for (int i = 0; i < MaxBuffersInFlight; ++i) {
        new_textures[i] = [metal_device newTextureWithDescriptor:td];
        if (!new_textures[i]) {
#if !__has_feature(objc_arc)
            for (int j = 0; j < i; ++j) {
                [new_textures[j] release];
            }
#endif
            return false;
        }
    }

    // Swap texture_buffers under lock to prevent drawInMTKView from using
    // a released texture between the release and the assignment.
    os_unfair_lock_lock(&window_data_specific->buffer_lock);
    for (int i = 0; i < MaxBuffersInFlight; ++i) {
        id<MTLTexture> old = texture_buffers[i];
        texture_buffers[i] = new_textures[i];
#if !__has_feature(objc_arc)
        [old release];
#endif
    }
    os_unfair_lock_unlock(&window_data_specific->buffer_lock);

    return true;
}

//-------------------------------------
- (void) drawInMTKView:(nonnull MTKView *) view {
    if (window_data == NULL || window_data_specific == NULL ||
        window_data->draw_buffer == NULL || window_data->buffer_width == 0 || window_data->buffer_height == 0 ||
        command_queue == nil || pipeline_state == nil) {
        return;
    }

    // Wait to ensure only MaxBuffersInFlight number of frames are getting proccessed
    // by any stage in the Metal pipeline (App, Metal, Drivers, GPU, etc)
    if (dispatch_semaphore_wait(semaphore, DISPATCH_TIME_NOW) != 0) {
        return; // GPU is still busy, skip this frame instead of blocking
    }

    current_buffer = (current_buffer + 1) % MaxBuffersInFlight;

    // Create a new command buffer for each render pass to the current drawable
    id<MTLCommandBuffer> commandBuffer = [command_queue commandBuffer];
    if (commandBuffer == nil || texture_buffers[current_buffer] == nil) {
        MFB_LOG(MFB_LOG_WARNING, "iOSViewDelegate: skipping frame due to unavailable command buffer or texture.");
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

    // Copy the bytes from our data object into the current texture slot.
    // Lock to prevent mfb_update_ex from freeing/replacing draw_buffer mid-copy.
    os_unfair_lock_lock(&window_data_specific->buffer_lock);
    if (window_data->draw_buffer != NULL) {
        MTLRegion region = { { 0, 0, 0 }, { window_data->buffer_width, window_data->buffer_height, 1 } };
        [texture_buffers[current_buffer] replaceRegion:region mipmapLevel:0 withBytes:window_data->draw_buffer bytesPerRow:window_data->buffer_stride];
    }
    os_unfair_lock_unlock(&window_data_specific->buffer_lock);

    // Delay getting the currentRenderPassDescriptor until absolutely needed. This avoids
    // holding onto the drawable and blocking the display pipeline any longer than necessary
    MTLRenderPassDescriptor* renderPassDescriptor = view.currentRenderPassDescriptor;
    if (renderPassDescriptor != nil) {
        //renderPassDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 1.0);

        // Create a render command encoder so we can render into something
        id<MTLRenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
        if (renderEncoder == nil) {
            MFB_LOG(MFB_LOG_WARNING, "iOSViewDelegate: failed to create render encoder.");
            [commandBuffer commit];
            return;
        }
        renderEncoder.label = @"minifb_command_encoder";

        // Set render command encoder state
        Vertex vertices[4];
        build_viewport_vertices(window_data, vertices);
        [renderEncoder setRenderPipelineState:pipeline_state];
        [renderEncoder setVertexBytes:vertices length:sizeof(vertices) atIndex:0];

        [renderEncoder setFragmentTexture:texture_buffers[current_buffer] atIndex:0];

        // Draw the vertices of our quads
        [renderEncoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];

        // We're done encoding commands
        [renderEncoder endEncoding];

        // Schedule a present once the framebuffer is complete using the current drawable
        id<CAMetalDrawable> drawable = view.currentDrawable;
        if (drawable != nil) {
            [commandBuffer presentDrawable:drawable];
        }
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
    // size is already in PIXELS (Metal drawable size, not logical points).
    // Do NOT multiply by screen scale here - that would double the value.
    uint32_t drawable_width  = (uint32_t) lround(size.width);
    uint32_t drawable_height = (uint32_t) lround(size.height);

    window_data->window_width  = drawable_width;
    window_data->window_height = drawable_height;
    resize_dst(window_data, drawable_width, drawable_height);
    build_viewport_vertices(window_data, window_data_specific->vertices);

    // Defer the resize callback to the main thread via mfb_update_ex / mfb_update_events,
    // which are called from user code. This avoids invoking the callback from the
    // CADisplayLink render thread.
    window_data->must_resize_context = true;
}

@end
