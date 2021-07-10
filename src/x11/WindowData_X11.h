#pragma once

#include "../../include/MiniFB_enums.h"
#include <stdint.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
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
    
    Window   root;
    Atom     XdndSelection;
    Atom     XdndAware;
    Atom     XdndEnter;
    Atom     XdndLeave;
    Atom     XdndTypeList;
    Atom     XdndPosition;
    Atom     XdndActionCopy;
    Atom     XdndStatus;
    Atom     XdndDrop;
    Atom     XdndFinished;
    Atom     UTF8_STRING;
    int      drop_x;
    int      drop_y;
    struct{
        Window   sourceWindow;
        char    *string;
        char    *type1;
        char    *type2;
        char    *type3;
    } xdnd;
} SWindowData_X11;
