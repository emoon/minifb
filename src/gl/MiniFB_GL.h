#pragma once

#if defined(USE_OPENGL_API)

    #include <WindowData.h>

    bool create_GL_context(SWindowData *window_data);
    void destroy_GL_context(SWindowData *window_data);
    void init_GL(SWindowData *window_data);
    void redraw_GL(SWindowData *window_data, const void *pixels);
    void resize_GL(SWindowData *window_data);
    
#endif
