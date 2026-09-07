#pragma once

#include "WindowData.h"
#include "WindowData_X11.h"

#include <X11/Xlib.h>
#include <stdbool.h>

//-------------------------------------
// Where the text of a key press comes from. Three builds are possible and they differ only
// here: xkbcommon on its own, an input method in front of xkbcommon, and an input method
// alone on a system without xkbcommon. The rest of the backend calls these and never learns
// which one is underneath.
//-------------------------------------
bool x11_text_init(SWindowData_X11 *window_data_specific);
void x11_text_destroy(SWindowData_X11 *window_data_specific);
void x11_text_focus(SWindowData_X11 *window_data_specific, bool focused);

// True when an input method consumed the event as part of a composition, so only the
// physical key is left to report. Always false without one, which consumes nothing.
bool x11_text_filter(XEvent *event);

void x11_text_dispatch(SWindowData *window_data, SWindowData_X11 *window_data_specific, XEvent *event);

#if defined(MINIFB_HAS_XKBCOMMON)
bool x11_text_xkb_init(SWindowData_X11 *window_data_specific);
void x11_text_xkb_destroy(SWindowData_X11 *window_data_specific);
void x11_text_xkb_dispatch(SWindowData *window_data, SWindowData_X11 *window_data_specific, XEvent *event);
#endif

#if defined(MINIFB_X11_USE_XIM)
bool x11_text_xim_init(SWindowData_X11 *window_data_specific);
void x11_text_xim_destroy(SWindowData_X11 *window_data_specific);
void x11_text_xim_focus(SWindowData_X11 *window_data_specific, bool focused);
void x11_text_xim_dispatch(SWindowData *window_data, SWindowData_X11 *window_data_specific, XEvent *event);
#endif
