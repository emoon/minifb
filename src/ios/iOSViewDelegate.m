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

//-------------------------------------
#define kShader(inc, src)    @inc#src

//-------------------------------------
enum { MaxBuffersInFlight = 3 };    // Number of textures in flight (tripple buffered)

id<MTLDevice>  g_metal_device = nil;
id<MTLLibrary> g_library      = nil;

//--
Vertex g_vertices[4] = {
    {-1.0, -1.0, 0, 1},
    {-1.0,  1.0, 0, 1},
    { 1.0, -1.0, 0, 1},
    { 1.0,  1.0, 0, 1},
};

//--
NSString *g_shader_src = kShader(
    "#include <metal_stdlib>\n",
    using namespace metal;

    //---------------------
    struct VertexOutput {
        float4 pos [[position]];
        float2 texcoord;
    };

    struct Vertex {
        float4 position [[position]];
    };

    //---------------------
    vertex VertexOutput
    vertFunc(unsigned int vID[[vertex_id]], const device Vertex *pos [[ buffer(0) ]]) {
        VertexOutput out;

        out.pos = pos[vID].position;

        out.texcoord.x = (float) (vID / 2);
        out.texcoord.y = 1.0 - (float) (vID % 2);

        return out;
    }

    //---------------------
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
    dispatch_semaphore_t        m_semaphore;
    id <MTLCommandQueue>        m_command_queue;

    id <MTLRenderPipelineState> m_pipeline_state;
    id <MTLTexture>             m_texture_buffer;

    uint8_t                     m_current_buffer;
}

//-------------------------------------
-(nonnull instancetype) initWithMetalKitView:(nonnull MTKView *) view windowData:(nonnull SWindowData *) windowData {
    self = [super init];
    if (self) {
        self->window_data = windowData;

        g_metal_device = view.device;

        view.colorPixelFormat = MTLPixelFormatBGRA8Unorm;
        view.sampleCount = 1;

        m_semaphore = dispatch_semaphore_create(MaxBuffersInFlight);
        m_command_queue = [g_metal_device newCommandQueue];

        [self _createShaders];
        [self _createAssets];
    }

    return self;
}

//-------------------------------------
- (bool) _createShaders {
    NSError *error = nil;
    
    g_library = [g_metal_device newLibraryWithSource:g_shader_src
                                             options:[[MTLCompileOptions alloc] init]
                                               error:&error
    ];
    if (error || !g_library) {
        NSLog(@"Unable to create shaders %@", error);
        return false;
    }

    id<MTLFunction> vertex_shader_func   = [g_library newFunctionWithName:@"vertFunc"];
    id<MTLFunction> fragment_shader_func = [g_library newFunctionWithName:@"fragFunc"];

    if (!vertex_shader_func) {
        NSLog(@"Unable to get vertFunc!\n");
        return false;
    }

    if (!fragment_shader_func) {
        NSLog(@"Unable to get fragFunc!\n");
        return false;
    }

    MTLRenderPipelineDescriptor *pipelineStateDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
    pipelineStateDescriptor.label = @"MiniFB_pipeline";
    pipelineStateDescriptor.vertexFunction = vertex_shader_func;
    pipelineStateDescriptor.fragmentFunction = fragment_shader_func;
    pipelineStateDescriptor.colorAttachments[0].pixelFormat = 80; //bgra8Unorm;

    m_pipeline_state = [g_metal_device newRenderPipelineStateWithDescriptor:pipelineStateDescriptor error:&error];
    if (!m_pipeline_state) {
        NSLog(@"Failed to created pipeline state, error %@", error);
        return false;
    }

    return true;
}

//-------------------------------------
- (void) _createAssets {
    MTLTextureDescriptor    *td;
    td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                            width:window_data->buffer_width
                                                           height:window_data->buffer_height
                                                        mipmapped:false];

    m_texture_buffer = [g_metal_device newTextureWithDescriptor:td];
}

//-------------------------------------
- (void) drawInMTKView:(nonnull MTKView *) view
{
    // Per frame updates here
    dispatch_semaphore_wait(m_semaphore, DISPATCH_TIME_FOREVER);

    m_current_buffer = (m_current_buffer + 1) % MaxBuffersInFlight;

    id <MTLCommandBuffer> commandBuffer = [m_command_queue commandBuffer];
    commandBuffer.label = @"minifb_command_buffer";

    __block dispatch_semaphore_t block_sema = m_semaphore;
    [commandBuffer addCompletedHandler:^(id<MTLCommandBuffer> buffer) {
        (void)buffer;
        dispatch_semaphore_signal(block_sema);
    }];

    // Copy the bytes from our data object into the texture
    MTLRegion region = { { 0, 0, 0 }, { window_data->buffer_width, window_data->buffer_height, 1 } };
    [m_texture_buffer replaceRegion:region mipmapLevel:0 withBytes:window_data->draw_buffer bytesPerRow:window_data->buffer_stride];

    // Delay getting the currentRenderPassDescriptor until absolutely needed. This avoids
    // holding onto the drawable and blocking the display pipeline any longer than necessary
    MTLRenderPassDescriptor* renderPassDescriptor = view.currentRenderPassDescriptor;
    if (renderPassDescriptor != nil) {
        //renderPassDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 1.0);

        // Create a render command encoder so we can render into something
        id<MTLRenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
        renderEncoder.label = @"minifb_command_encoder";

        // Set render command encoder state
        [renderEncoder setRenderPipelineState:m_pipeline_state];
        [renderEncoder setVertexBytes:g_vertices length:sizeof(g_vertices) atIndex:0];

        //[renderEncoder setFragmentTexture:m_texture_buffers[m_current_buffer] atIndex:0];
        [renderEncoder setFragmentTexture:m_texture_buffer atIndex:0];

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
    // Respond to drawable size or orientation changes here
}

@end
