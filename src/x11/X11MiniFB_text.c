#include "X11MiniFB_text.h"

#include "MiniFB_internal.h"

//-------------------------------------
bool
x11_text_init(SWindowData_X11 *window_data_specific) {
    // Both are set up when both are built, because an input method is a service that may not
    // be running, and that is decided per event rather than per build. It failing here is not
    // fatal either: there is always a keysym path underneath it.
#if defined(MINIFB_X11_USE_XIM)
    x11_text_xim_init(window_data_specific);
#endif

#if defined(MINIFB_HAS_XKBCOMMON)
    return x11_text_xkb_init(window_data_specific);
#elif defined(MINIFB_X11_USE_XIM)
    return true;
#else
    kUnused(window_data_specific);
    return false;
#endif
}

//-------------------------------------
void
x11_text_destroy(SWindowData_X11 *window_data_specific) {
#if defined(MINIFB_X11_USE_XIM)
    x11_text_xim_destroy(window_data_specific);
#endif
#if defined(MINIFB_HAS_XKBCOMMON)
    x11_text_xkb_destroy(window_data_specific);
#endif
}

//-------------------------------------
void
x11_text_focus(SWindowData_X11 *window_data_specific, bool focused) {
#if defined(MINIFB_HAS_XKBCOMMON)
    // A half typed sequence belongs to the visit that started it. Wayland drops its own here.
    if (focused == false) {
        mfb_xkb_compose_reset(&window_data_specific->compose);
    }
#endif

#if defined(MINIFB_X11_USE_XIM)
    x11_text_xim_focus(window_data_specific, focused);
#endif

#if !defined(MINIFB_HAS_XKBCOMMON) && !defined(MINIFB_X11_USE_XIM)
    kUnused(window_data_specific);
    kUnused(focused);
#endif
}

//-------------------------------------
bool
x11_text_filter(XEvent *event) {
#if defined(MINIFB_X11_USE_XIM)
    // The input method has to see every event to compose at all, and some of them clear the
    // keycode while filtering, so it is put back.
    unsigned int keycode  = event->xkey.keycode;
    bool         filtered = XFilterEvent(event, None) == True;

    event->xkey.keycode = keycode;

    return filtered;
#else
    kUnused(event);
    return false;
#endif
}

//-------------------------------------
void
x11_text_dispatch(SWindowData *window_data, SWindowData_X11 *window_data_specific, XEvent *event) {
#if defined(MINIFB_X11_USE_XIM)
    if (window_data_specific->ic != NULL) {
        x11_text_xim_dispatch(window_data, window_data_specific, event);
        return;
    }
#endif

#if defined(MINIFB_HAS_XKBCOMMON)
    x11_text_xkb_dispatch(window_data, window_data_specific, event);
#elif defined(MINIFB_X11_USE_XIM)
    // Without xkbcommon the keysym path inside the input method file is all there is.
    x11_text_xim_dispatch(window_data, window_data_specific, event);
#else
    kUnused(window_data);
    kUnused(window_data_specific);
    kUnused(event);
#endif
}
