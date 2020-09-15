#pragma once

#include <MiniFB_enums.h>
#include <WindowData.h>
#include <MetalKit/MetalKit.h>

@class iOSViewDelegate;

typedef struct Vertex {
    float x, y, z, w;
} Vertex;

typedef struct {
    iOSViewDelegate     *view_delegate;
    Vertex              vertices[4];
} SWindowData_IOS;
