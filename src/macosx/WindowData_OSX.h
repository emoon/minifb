#pragma once

#include <MiniFB_enums.h>
#include <WindowData.h>

#if defined(USE_METAL_API)
    #include <MetalKit/MetalKit.h>
#endif

@class OSXWindow;

typedef struct Vertex {
    float x, y, z, w;
} Vertex;

typedef struct {
    OSXWindow           *window;
    struct mfb_timer    *timer;

#if defined(USE_METAL_API)
    struct {
        Vertex                      vertices[4];
    } metal;
#endif
} SWindowData_OSX;
