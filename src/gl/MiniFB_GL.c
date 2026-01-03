#if defined(USE_OPENGL_API)

#include "MiniFB_GL.h"
#include "MiniFB_internal.h"
#if defined(_WIN32) || defined(WIN32)
    #include <windows/WindowData_Win.h>
    #include <gl/gl.h>
#elif defined(__linux__) || defined(linux)
    #include <x11/WindowData_X11.h>
    #include <GL/gl.h>
    #include <GL/glx.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//#define kUse_Clean_UP
#if defined(kUse_Clean_UP)
    #define UseCleanUp(x) x
#else
    #define UseCleanUp(x)
#endif

extern double   g_time_for_frame;
extern bool     g_use_hardware_sync;

//-------------------------------------
static bool
CheckGLExtension(const char *name) {
    static const char *extensions = NULL;

    if (extensions == NULL) {
#if defined(_WIN32) || defined(WIN32)
        // TODO: This is deprecated on OpenGL 3+.
        // Use glGetIntegerv(GL_NUM_EXTENSIONS, &n) and glGetStringi(GL_EXTENSIONS, index)
        extensions = (const char *) glGetString(GL_EXTENSIONS);
#elif defined(__linux__) || defined(linux)
        Display *display = glXGetCurrentDisplay();

        extensions = glXQueryExtensionsString(display, DefaultScreen(display));
#endif
    }

    if (extensions != NULL) {
        const char *start = extensions;
        const char *end, *where;
        while(1) {
            where = strstr(start, name);
            if (where == NULL)
                return false;

            end = where + strlen(name);
            if (where == start || *(where - 1) == ' ') {
                if (*end == ' ' || *end == 0)
                    break;
            }

            start = end;
        }
    }

    return true;
}

//-------------------------------------
#if defined(_WIN32) || defined(WIN32)
bool
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
        return false;
    }

    if (SetPixelFormat(hDC, pixelFormat, &pfd) != TRUE) {
        MessageBox(WindowFromDC(hDC), "SetPixelFormat failed.", "Error", MB_ICONERROR | MB_OK);
        return false;
    }

    return true;
}

typedef BOOL (WINAPI * PFNWGLSWAPINTERVALEXTPROC)(int);
typedef int (WINAPI * PFNWGLGETSWAPINTERVALEXTPROC)(void);
PFNWGLSWAPINTERVALEXTPROC       SwapIntervalEXT    = NULL;
PFNWGLGETSWAPINTERVALEXTPROC    GetSwapIntervalEXT = NULL;

#elif defined(__linux__) || defined(linux)

bool
setup_pixel_format(SWindowData_X11 *window_data_specific) {
    GLint glxAttribs[] = {
        GLX_RGBA,
        GLX_DOUBLEBUFFER,
        GLX_DEPTH_SIZE,     24,
        GLX_STENCIL_SIZE,   8,
        GLX_RED_SIZE,       8,
        GLX_GREEN_SIZE,     8,
        GLX_BLUE_SIZE,      8,
        GLX_DEPTH_SIZE,     24,
        GLX_STENCIL_SIZE,   8,
        GLX_SAMPLE_BUFFERS, 0,
        GLX_SAMPLES,        0,
        None
    };

    XVisualInfo* visualInfo = glXChooseVisual(window_data_specific->display, window_data_specific->screen, glxAttribs);
    if (visualInfo == 0) {
        fprintf(stderr, "Could not create correct visual window.\n");
        XCloseDisplay(window_data_specific->display);
        return false;
    }
    window_data_specific->context = glXCreateContext(window_data_specific->display, visualInfo, NULL, GL_TRUE);

    return true;
}

typedef void (*PFNGLXSWAPINTERVALEXTPROC)(Display*,GLXDrawable,int);
PFNGLXSWAPINTERVALEXTPROC   SwapIntervalEXT = NULL;

#endif

//-------------------------------------
bool
create_GL_context(SWindowData *window_data) {
#if defined(_WIN32) || defined(WIN32)
    SWindowData_Win *window_data_specific = (SWindowData_Win *) window_data->specific;

    if (setup_pixel_format(window_data_specific->hdc) == false)
        return false;

    window_data_specific->hGLRC = wglCreateContext(window_data_specific->hdc);
    wglMakeCurrent(window_data_specific->hdc, window_data_specific->hGLRC);
    init_GL(window_data);

    SwapIntervalEXT    = (PFNWGLSWAPINTERVALEXTPROC)    wglGetProcAddress("wglSwapIntervalEXT");
    GetSwapIntervalEXT = (PFNWGLGETSWAPINTERVALEXTPROC) wglGetProcAddress("wglGetSwapIntervalEXT");
    set_target_fps_aux();

    return true;

#elif defined(__linux__) || defined(linux)
    SWindowData_X11 *window_data_specific = (SWindowData_X11 *) window_data->specific;

    GLint majorGLX, minorGLX = 0;
    glXQueryVersion(window_data_specific->display, &majorGLX, &minorGLX);
    if (majorGLX <= 1 && minorGLX < 2) {
        fprintf(stderr, "GLX 1.2 or greater is required.\n");
        XCloseDisplay(window_data_specific->display);
        return false;
    }
    else {
        //fprintf(stdout, "GLX version: %d.%d\n", majorGLX, minorGLX);
    }

    if (setup_pixel_format(window_data_specific) == false)
        return false;

    glXMakeCurrent(window_data_specific->display, window_data_specific->window, window_data_specific->context);

    //fprintf(stdout, "GL Vendor: %s\n", glGetString(GL_VENDOR));
    //fprintf(stdout, "GL Renderer: %s\n", glGetString(GL_RENDERER));
    //fprintf(stdout, "GL Version: %s\n", glGetString(GL_VERSION));
    //fprintf(stdout, "GL Shading Language: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));

    init_GL(window_data);

    if (CheckGLExtension("GLX_EXT_swap_control")) {
        SwapIntervalEXT = (PFNGLXSWAPINTERVALEXTPROC) glXGetProcAddress((const GLubyte *)"glXSwapIntervalEXT");
        set_target_fps_aux();
    }

    return true;
#endif
}

//-------------------------------------
void
destroy_GL_context(SWindowData *window_data) {
#if defined(_WIN32) || defined(WIN32)

    SWindowData_Win *window_data_specific = (SWindowData_Win *) window_data->specific;
    if (window_data_specific->hGLRC) {
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(window_data_specific->hGLRC);
        window_data_specific->hGLRC = 0;
    }

#elif defined(__linux__) || defined(linux)

    SWindowData_X11 *window_data_specific = (SWindowData_X11 *) window_data->specific;
    glXDestroyContext(window_data_specific->display, window_data_specific->context);

#endif
}

//-------------------------------------
#if defined(RGB)
    #undef RGB
#endif

#define TEXTURE0    0x84C0  // [ Core in gl 1.3, gles1 1.0, gles2 2.0, glsc2 2.0, Provided by GL_ARB_multitexture (gl) ]
#define RGB         0x1907  // [ Core in gl 1.0, gles1 1.0, gles2 2.0, glsc2 2.0 ]
#define RGBA        0x1908  // [ Core in gl 1.0, gles1 1.0, gles2 2.0, glsc2 2.0 ]
#define BGR         0x80E0  // [ Core in gl 1.2 ]
#define BGRA        0x80E1  // [ Core in gl 1.2, Provided by GL_ARB_vertex_array_bgra (gl|glcore) ]

//-------------------------------------
void
init_GL(SWindowData *window_data) {
#if defined(_WIN32) || defined(WIN32)

    SWindowData_Win *window_data_specific = (SWindowData_Win *) window_data->specific;

#elif defined(__linux__) || defined(linux)

    SWindowData_X11 *window_data_specific = (SWindowData_X11 *) window_data->specific;

#endif

    glViewport(0, 0, window_data->window_width, window_data->window_height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, window_data->window_width, window_data->window_height, 0, 2048, -2048);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);

    glEnable(GL_TEXTURE_2D);

    glGenTextures(1, &window_data_specific->text_id);
    //glActiveTexture(TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, window_data_specific->text_id);
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
// When the DPI or window state changes, Windows may emit several WM_SIZE events
// within the same frame. Each event would trigger a glViewport call, which can
// leave the OpenGL context in an inconsistent state (on some systems this causes
// the viewport to end up slightly offset).
//
// To avoid this, we do not apply the resize immediately. Instead, WM_SIZE sets a
// boolean flag, and just before drawing the next frame we perform a single
// glViewport update. This guarantees that glViewport is called only once per
// frame and fixes the offset issue.
//-------------------------------------
void
resize_GL(SWindowData *window_data) {
    if (window_data != NULL) {
        window_data->must_resize_context = true;
    }
}

//-------------------------------------
static void
effective_resize_GL(SWindowData *window_data) {
    window_data->must_resize_context = false;

    if (window_data->is_initialized) {

    #if defined(_WIN32) || defined(WIN32)

        SWindowData_Win *window_data_specific = (SWindowData_Win *) window_data->specific;
        wglMakeCurrent(window_data_specific->hdc, window_data_specific->hGLRC);

    #elif defined(__linux__) || defined(linux)

        SWindowData_X11 *window_data_specific = (SWindowData_X11 *) window_data->specific;
        glXMakeCurrent(window_data_specific->display, window_data_specific->window, window_data_specific->context);

    #endif

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
    if (window_data->must_resize_context) {
        effective_resize_GL(window_data);
    }
#if defined(_WIN32) || defined(WIN32)

    SWindowData_Win *window_data_specific = (SWindowData_Win *) window_data->specific;
    GLenum format = BGRA;

    wglMakeCurrent(window_data_specific->hdc, window_data_specific->hGLRC);

#elif defined(__linux__) || defined(linux)

    SWindowData_X11 *window_data_specific = (SWindowData_X11 *) window_data->specific;
    GLenum format = BGRA;

    glXMakeCurrent(window_data_specific->display, window_data_specific->window, window_data_specific->context);

#endif

    float x, y, w, h;

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

    UseCleanUp(glBindTexture(GL_TEXTURE_2D, window_data_specific->text_id));
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, window_data->buffer_width, window_data->buffer_height, 0, format, GL_UNSIGNED_BYTE, pixels);
    //glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, window_data->buffer_width, window_data->buffer_height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    UseCleanUp(glEnableClientState(GL_VERTEX_ARRAY));
    UseCleanUp(glEnableClientState(GL_TEXTURE_COORD_ARRAY));
    glVertexPointer(2, GL_FLOAT, 4 * sizeof(float), vertices);
    glTexCoordPointer(2, GL_FLOAT, 4 * sizeof(float), vertices + 2);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    UseCleanUp(glDisableClientState(GL_TEXTURE_COORD_ARRAY));
    UseCleanUp(glDisableClientState(GL_VERTEX_ARRAY));
    UseCleanUp(glBindTexture(GL_TEXTURE_2D, 0));

#if defined(_WIN32) || defined(WIN32)
    SwapBuffers(window_data_specific->hdc);
#elif defined(__linux__) || defined(linux)
    glXSwapBuffers(window_data_specific->display, window_data_specific->window);
#endif
}

//-------------------------------------
void
set_target_fps_aux() {
    // Assuming the monitor refresh rate is 60 hz
    int interval = (int) ((60.0 * g_time_for_frame) + 0.5);

#if defined(_WIN32) || defined(WIN32)

    if (SwapIntervalEXT != NULL) {
        bool success = SwapIntervalEXT(interval);
        if (GetSwapIntervalEXT != NULL) {
            int currentInterval = GetSwapIntervalEXT();
            if (interval != currentInterval) {
                fprintf(stderr, "Cannot set target swap interval.\n");
            }
            fprintf(stdout, "Current swap interval is %d\n", currentInterval);
        }
        else if (success == false) {
            fprintf(stderr, "Cannot set target swap interval.\n");
        }

        g_use_hardware_sync = true;
    }

#elif defined(__linux__) || defined(linux)

    #define kGLX_SWAP_INTERVAL_EXT               0x20F1
    #define kGLX_MAX_SWAP_INTERVAL_EXT           0x20F2

    if (SwapIntervalEXT != NULL) {
        Display         *dpy     = glXGetCurrentDisplay();
        GLXDrawable     drawable = glXGetCurrentDrawable();
        unsigned int    currentInterval, maxInterval;

        SwapIntervalEXT(dpy, drawable, interval);
        glXQueryDrawable(dpy, drawable, kGLX_SWAP_INTERVAL_EXT, &currentInterval);
        if (interval != (int)currentInterval) {
            glXQueryDrawable(dpy, drawable, kGLX_MAX_SWAP_INTERVAL_EXT, &maxInterval);
            fprintf(stderr, "Cannot set target swap interval. Current swap interval is %d (max: %d)\n", currentInterval, maxInterval);
        }

        g_use_hardware_sync = true;
    }

#endif
}

#endif
