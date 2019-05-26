#pragma once

#include <MiniFB_ex_enums.h>

@class OSXWindow;

typedef struct {
    OSXWindow   *window;
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
} SWindowData;
