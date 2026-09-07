#pragma once

#include <MiniFB_enums.h>
#include <stdint.h>
#include <X11/Xlib.h>
#if defined(MINIFB_HAS_XKBCOMMON)
#include "MiniFB_xkb.h"
#endif
#if defined(USE_OPENGL_API)
#include <GL/glx.h>
#endif

typedef struct {
    Window              window;

    Display             *display;
#if defined(MINIFB_X11_USE_XIM)
    XIM                 im;
    XIC                 ic;
    KeySym              pending_dead_keysym;
#endif
#if defined(MINIFB_HAS_XKBCOMMON)
    struct xkb_context  *xkb_context;
    SXkbCompose         compose;
#endif
    int                 screen;
    GC                  gc;
	Cursor              invis_cursor;
#if defined(USE_OPENGL_API)
    GLXContext          context;
    uint32_t            text_id;
#else
    XImage              *image;
    void                *image_buffer;
    XImage              *image_scaler;
    uint32_t            image_scaler_width;
    uint32_t            image_scaler_height;
#endif

    struct mfb_timer   *timer;
} SWindowData_X11;
