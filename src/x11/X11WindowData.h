#pragma once

#include <MiniFB_ex_enums.h>
#include <stdint.h>
#include <X11/Xlib.h>


typedef struct {
    Window      window;
    uint32_t    window_width;
    uint32_t    window_height;

    uint32_t    dst_offset_x;
    uint32_t    dst_offset_y;
    uint32_t    dst_width;
    uint32_t    dst_height;

    void        *draw_buffer;
    uint32_t    buffer_width;
    uint32_t    buffer_height;
    
    uint32_t    mod_keys;
    bool       close;

    Display     *display;
    int         screen;
    GC          gc;
    XImage      *image;

    void        *image_buffer;
    XImage      *image_scaler;
    uint32_t    image_scaler_width;
    uint32_t    image_scaler_height;
} SWindowData;
