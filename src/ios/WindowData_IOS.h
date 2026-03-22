#pragma once

#include <MiniFB_enums.h>
#include <MiniFB_internal.h>
#include <WindowData.h>
#include <MetalKit/MetalKit.h>
#include <os/lock.h>

@class iOSViewDelegate;

typedef struct Vertex {
    float x, y, z, w;
} Vertex;

typedef struct {
    iOSViewDelegate     *view_delegate;
    Vertex              vertices[4];
    struct mfb_timer    *timer;
    os_unfair_lock      buffer_lock;
} SWindowData_IOS;
