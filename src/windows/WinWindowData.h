#pragma once

#include <MiniFB_enums.h>
#include <stdint.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>


typedef struct {
    HWND        window;
    WNDCLASS    s_wc;
    HDC         s_hdc;
    BITMAPINFO  *s_bitmapInfo;
    bool        s_mouse_inside;

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
    bool        close;
} SWindowData;
