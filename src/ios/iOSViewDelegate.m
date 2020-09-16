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
@implementation iOSViewDelegate {
    SWindowData                 *window_data;
    SWindowData_IOS             *window_data_ios;
    
    id<MTLDevice>               metal_device;
    id<MTLLibrary>              metal_library;

    dispatch_semaphore_t        semaphore;
    id<MTLCommandQueue>         command_queue;

    id<MTLRenderPipelineState>  pipeline_state;
    id<MTLTexture>              texture_buffer;

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
    memcpy(window_data_ios->vertices, s_vertices, sizeof(s_vertices));

    MTLTextureDescriptor    *td;
    td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                            width:window_data->buffer_width
                                                           height:window_data->buffer_height
                                                        mipmapped:false];

    // Create the texture from the device by using the descriptor
    texture_buffer = [metal_device newTextureWithDescriptor:td];
}

//-------------------------------------
- (void) resizeTextures {
    MTLTextureDescriptor    *td;
    td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                            width:window_data->buffer_width
                                                           height:window_data->buffer_height
                                                        mipmapped:false];

    // Create the texture from the device by using the descriptor
    [texture_buffer release];
    texture_buffer = [metal_device newTextureWithDescriptor:td];
}

//-------------------------------------
- (void) drawInMTKView:(nonnull MTKView *) view {
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
    [texture_buffer replaceRegion:region mipmapLevel:0 withBytes:window_data->draw_buffer bytesPerRow:window_data->buffer_stride];

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

        //[renderEncoder setFragmentTexture:texture_buffers[current_buffer] atIndex:0];
        [renderEncoder setFragmentTexture:texture_buffer atIndex:0];

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
- (void) mtkView:(nonnull MTKView *)view drawableSizeWillChange:(CGSize)size {
    (void) view;
    // Respond to drawable size or orientation changes here
    float scale = [UIScreen mainScreen].scale;

    window_data->window_width  = size.width  * scale;
    window_data->window_height = size.height * scale;
    resize_dst(window_data, size.width, size.height);

    kCall(resize_func, size.width, size.height);
}

@end
