#pragma once

#include <MiniFB_enums.h>
#include <stdint.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>


typedef struct {
    HWND                window;
    WNDCLASS            wc;
    HDC                 hdc;
    BITMAPINFO          *bitmapInfo;
    struct mfb_timer    *timer;
    bool                mouse_inside;
} SWindowData_Win;
