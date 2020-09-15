#pragma once

#include <MiniFB_enums.h>
#include <WindowData.h>

#if defined(USE_METAL_API)
    #include <MetalKit/MetalKit.h>
#endif

@class OSXWindow;
@class OSXViewDelegate;

typedef struct Vertex {
    float x, y, z, w;
} Vertex;

typedef struct {
    OSXWindow           *window;
    OSXViewDelegate     *viewController;
    struct mfb_timer    *timer;

#if defined(USE_METAL_API)
    struct {
        Vertex                      vertices[4];
    } metal;
#endif
} SWindowData_OSX;
