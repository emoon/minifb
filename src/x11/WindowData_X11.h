#pragma once

#include <MiniFB_enums.h>
#include <stdint.h>
#include <X11/Xlib.h>


typedef struct {
    Window      window;

    Display     *display;
    int         screen;
    GC          gc;
    XImage      *image;

    void        *image_buffer;
    XImage      *image_scaler;
    uint32_t    image_scaler_width;
    uint32_t    image_scaler_height;
} SWindowData_X11;
