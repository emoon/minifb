#if defined(USE_OPENGL_API)

#include "MiniFB_GL.h"
#if defined(_WIN32) || defined(WIN32)
    #include <windows/WindowData_Win.h>
#endif
#include <gl/gl.h>
#include <stdlib.h>

//#define kUse_Clean_UP
#if defined(kUse_Clean_UP)
    #define UseCleanUp(x) x
#else
    #define UseCleanUp(x)
#endif

//-------------------------------------
void setup_pixel_format(HDC hDC);

//-------------------------------------
void 
create_GL_context(SWindowData *window_data) {
    SWindowData_Win *window_data_win;

    window_data_win = (SWindowData_Win *) window_data->specific;
    setup_pixel_format(window_data_win->hdc);
    window_data_win->hGLRC = wglCreateContext(window_data_win->hdc);
    wglMakeCurrent(window_data_win->hdc, window_data_win->hGLRC);
    init_GL(window_data);
}

//-------------------------------------
void
setup_pixel_format(HDC hDC) {
    int pixelFormat;

    PIXELFORMATDESCRIPTOR pfd = {
        sizeof(PIXELFORMATDESCRIPTOR),  // size
        1,                              // version
        PFD_SUPPORT_OPENGL |            //
        PFD_DRAW_TO_WINDOW |            //
        PFD_DOUBLEBUFFER,               // support double-buffering
        PFD_TYPE_RGBA,                  // color type
        24,                             // preferred color depth
        0, 0, 0, 0, 0, 0,               // color and shift bits (ignored)
        0,                              // no alpha buffer
        0,                              // alpha bits (ignored)
        0,                              // no accumulation buffer
        0, 0, 0, 0,                     // accum bits (ignored)
        24,                             // depth buffer
        8,                              // no stencil buffer
        0,                              // no auxiliary buffers
        PFD_MAIN_PLANE,                 // main layer
        0,                              // reserved
        0, 0, 0,                        // no layer, visible, damage masks
    };

    pixelFormat = ChoosePixelFormat(hDC, &pfd);
    if (pixelFormat == 0) {
        MessageBox(WindowFromDC(hDC), "ChoosePixelFormat failed.", "Error", MB_ICONERROR | MB_OK);
        exit(1);
    }

    if (SetPixelFormat(hDC, pixelFormat, &pfd) != TRUE) {
        MessageBox(WindowFromDC(hDC), "SetPixelFormat failed.", "Error", MB_ICONERROR | MB_OK);
        exit(1);
    }
}

#define TEXTURE0    0x84C0

//-------------------------------------
void 
init_GL(SWindowData *window_data) {
    SWindowData_Win *window_data_win;

    window_data_win = (SWindowData_Win *) window_data->specific;

    glViewport(0, 0, window_data->window_width, window_data->window_height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, window_data->window_width, window_data->window_height, 0, 2048, -2048);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);

    glEnable(GL_TEXTURE_2D);

    glGenTextures(1, &window_data_win->text_id);
    //glActiveTexture(TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, window_data_win->text_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    UseCleanUp(glDisableClientState(GL_TEXTURE_COORD_ARRAY));
    UseCleanUp(glDisableClientState(GL_VERTEX_ARRAY));
    UseCleanUp(glBindTexture(GL_TEXTURE_2D, 0));
}

//-------------------------------------
void 
resize_GL(SWindowData *window_data) {
    if(window_data->is_initialized) {
        glViewport(0, 0, window_data->window_width, window_data->window_height);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, window_data->window_width, window_data->window_height, 0, 2048, -2048);

        glClear(GL_COLOR_BUFFER_BIT);
    }
}

//-------------------------------------
void 
redraw_GL(SWindowData *window_data, const void *pixels) {
    float           x, y, w, h;
    SWindowData_Win *window_data_ex;

    window_data_ex = (SWindowData_Win *) window_data->specific;

    x = (float) window_data->dst_offset_x;
    y = (float) window_data->dst_offset_y;
    w = (float) window_data->dst_offset_x + window_data->dst_width;
    h = (float) window_data->dst_offset_y + window_data->dst_height;

    float vertices[] = {
        x, y,
        0, 0, 

        w, y,
        1, 0,
        
        x, h,
        0, 1,

        w, h,
        1, 1,
    };

    glClear(GL_COLOR_BUFFER_BIT);

    UseCleanUp(glBindTexture(GL_TEXTURE_2D, window_data_ex->text_id));
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, window_data->buffer_width, window_data->buffer_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    //glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, window_data->buffer_width, window_data->buffer_height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    UseCleanUp(glEnableClientState(GL_VERTEX_ARRAY));
    UseCleanUp(glEnableClientState(GL_TEXTURE_COORD_ARRAY));
    glVertexPointer(2, GL_FLOAT, 4 * sizeof(float), vertices);
    glTexCoordPointer(2, GL_FLOAT, 4 * sizeof(float), vertices + 2);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    UseCleanUp(glDisableClientState(GL_TEXTURE_COORD_ARRAY));
    UseCleanUp(glDisableClientState(GL_VERTEX_ARRAY));
    UseCleanUp(glBindTexture(GL_TEXTURE_2D, 0));

    SwapBuffers(window_data_ex->hdc);
}

#endif