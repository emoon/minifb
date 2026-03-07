#pragma once

#include <stdbool.h>
#include <X11/Xlib.h>

typedef struct {
    bool has_xresources;
    float x_xresources;
    float y_xresources;

    bool has_xsettings;
    float x_xsettings;
    float y_xsettings;

    bool has_xrandr_window;
    float x_xrandr_window;
    float y_xrandr_window;

    bool has_xrandr_any;
    float x_xrandr_any;
    float y_xrandr_any;

    bool has_display_mm;
    float x_display_mm;
    float y_display_mm;
} SX11ScaleMethods;

void
mfb_x11_query_scale_methods(Display *display, Window window, int screen, SX11ScaleMethods *methods);
