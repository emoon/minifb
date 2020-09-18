#pragma once

#include <MiniFB_enums.h>
#include <stdint.h>
#include <X11/Xlib.h>
#if defined(USE_OPENGL_API)
#include <GL/glx.h>
#endif

typedef struct {
    Window              window;
        
    Display             *display;
    int                 screen;
    GC                  gc;
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
