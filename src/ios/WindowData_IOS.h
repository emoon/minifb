#pragma once

#include <MiniFB_enums.h>
#include <WindowData.h>

#include <MetalKit/MetalKit.h>

typedef struct Vertex {
    float x, y, z, w;
} Vertex;

typedef struct {
    id<MTLCommandQueue>         command_queue;
    id<MTLRenderPipelineState>  pipeline_state;
} SWindowData_IOS;
