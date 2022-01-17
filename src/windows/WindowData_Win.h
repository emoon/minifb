#pragma once

#include <MiniFB_enums.h>
//#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef struct {
    HWND                window;
    WNDCLASS            wc;
    HDC                 hdc;
#if defined(USE_OPENGL_API)
    HGLRC               hGLRC;
    uint32_t            text_id;
#else
    BITMAPINFO          *bitmapInfo;
#endif
    struct mfb_timer    *timer;
    bool                mouse_inside;
} SWindowData_Win;
