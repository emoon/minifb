#include "X11MiniFB_text.h"

#include "MiniFB_internal.h"
#include "MiniFB_xkb.h"

#include <X11/XKBlib.h>

//-------------------------------------
bool
x11_text_xkb_init(SWindowData_X11 *window_data_specific) {
    window_data_specific->xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (window_data_specific->xkb_context == NULL) {
        MFB_LOG(MFB_LOG_WARNING, "X11MiniFB: xkb_context_new failed; text input will be unavailable.");
        return false;
    }

    // A locale with no Compose file costs the dead keys and nothing else, so the rest of the
    // text pipeline is still worth having.
    mfb_xkb_compose_init(&window_data_specific->compose, window_data_specific->xkb_context);

    return true;
}

//-------------------------------------
void
x11_text_xkb_destroy(SWindowData_X11 *window_data_specific) {
    // The compose state holds a reference to the context, so it goes first.
    mfb_xkb_compose_destroy(&window_data_specific->compose);

    if (window_data_specific->xkb_context != NULL) {
        xkb_context_unref(window_data_specific->xkb_context);
        window_data_specific->xkb_context = NULL;
    }
}

//-------------------------------------
// The keysym comes from XKB rather than from the event on its own, because the layout group,
// the third level of a key and what a lock does to it all live in the server's keymap.
//-------------------------------------
void
x11_text_xkb_dispatch(SWindowData *window_data, SWindowData_X11 *window_data_specific, XEvent *event) {
    KeySym       keysym        = NoSymbol;
    unsigned int consumed_mods = 0;

    if (XkbLookupKeySym(window_data_specific->display,
                        (KeyCode) event->xkey.keycode,
                        event->xkey.state,
                        &consumed_mods,
                        &keysym) == False) {
        return;
    }

    if (mfb_xkb_compose_feed(&window_data_specific->compose, window_data, (xkb_keysym_t) keysym) == true) {
        return;
    }

    uint32_t codepoint = xkb_keysym_to_utf32((xkb_keysym_t) keysym);
    if (codepoint != 0) {
        mfb_dispatch_char_input(window_data, codepoint);
    }
}
