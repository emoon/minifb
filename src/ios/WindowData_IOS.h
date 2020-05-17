#pragma once

#include <MiniFB_enums.h>
#include <WindowData.h>
#include <MetalKit/MetalKit.h>

typedef struct Vertex {
    float x, y, z, w;
} Vertex;

typedef struct {
    Vertex                      vertices[4];
} SWindowData_IOS;
