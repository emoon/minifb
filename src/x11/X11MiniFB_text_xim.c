#include "X11MiniFB_text.h"

#include "MiniFB_internal.h"
#include "MiniFB_utf8.h"

#include <X11/Xutil.h>
#include <X11/keysym.h>

#include <locale.h>
#include <stdlib.h>
#include <string.h>

//-------------------------------------
static uint32_t
keysym_to_codepoint(KeySym keysym) {
    // Direct Latin-1 keysyms.
    if ((keysym >= 0x0020 && keysym <= 0x007e) ||
        (keysym >= 0x00a0 && keysym <= 0x00ff)) {
        return (uint32_t) keysym;
    }

    // X11 Unicode keysym encoding: 0x01000000 | UCS-24.
    if ((keysym & 0xff000000UL) == 0x01000000UL) {
        uint32_t codepoint = (uint32_t) (keysym & 0x00ffffffUL);
        if (codepoint <= 0x10ffff && !(codepoint >= 0xd800 && codepoint <= 0xdfff)) {
            return codepoint;
        }
    }

    return 0;
}

//-------------------------------------
static bool
is_dead_keysym(KeySym keysym) {
    switch (keysym) {
        case XK_dead_grave:
        case XK_dead_acute:
        case XK_dead_circumflex:
        case XK_dead_tilde:
        case XK_dead_diaeresis:
            return true;
        default:
            return false;
    }
}

//-------------------------------------
static uint32_t
compose_dead_codepoint(KeySym dead_keysym, uint32_t codepoint) {
    switch (dead_keysym) {
        case XK_dead_acute:
            switch (codepoint) {
                case 'a': return 0x00e1; case 'A': return 0x00c1;
                case 'e': return 0x00e9; case 'E': return 0x00c9;
                case 'i': return 0x00ed; case 'I': return 0x00cd;
                case 'o': return 0x00f3; case 'O': return 0x00d3;
                case 'u': return 0x00fa; case 'U': return 0x00da;
                case 'y': return 0x00fd; case 'Y': return 0x00dd;
            }
            break;
        case XK_dead_grave:
            switch (codepoint) {
                case 'a': return 0x00e0; case 'A': return 0x00c0;
                case 'e': return 0x00e8; case 'E': return 0x00c8;
                case 'i': return 0x00ec; case 'I': return 0x00cc;
                case 'o': return 0x00f2; case 'O': return 0x00d2;
                case 'u': return 0x00f9; case 'U': return 0x00d9;
            }
            break;
        case XK_dead_diaeresis:
            switch (codepoint) {
                case 'a': return 0x00e4; case 'A': return 0x00c4;
                case 'e': return 0x00eb; case 'E': return 0x00cb;
                case 'i': return 0x00ef; case 'I': return 0x00cf;
                case 'o': return 0x00f6; case 'O': return 0x00d6;
                case 'u': return 0x00fc; case 'U': return 0x00dc;
                case 'y': return 0x00ff; case 'Y': return 0x0178;
            }
            break;
        case XK_dead_circumflex:
            switch (codepoint) {
                case 'a': return 0x00e2; case 'A': return 0x00c2;
                case 'e': return 0x00ea; case 'E': return 0x00ca;
                case 'i': return 0x00ee; case 'I': return 0x00ce;
                case 'o': return 0x00f4; case 'O': return 0x00d4;
                case 'u': return 0x00fb; case 'U': return 0x00db;
            }
            break;
        case XK_dead_tilde:
            switch (codepoint) {
                case 'a': return 0x00e3; case 'A': return 0x00c3;
                case 'n': return 0x00f1; case 'N': return 0x00d1;
                case 'o': return 0x00f5; case 'O': return 0x00d5;
            }
            break;
    }

    return 0;
}

//-------------------------------------
static uint32_t
dead_keysym_to_codepoint(KeySym dead_keysym) {
    switch (dead_keysym) {
        case XK_dead_grave:       return 0x0060; // `
        case XK_dead_acute:       return 0x00b4; // ´
        case XK_dead_circumflex:  return 0x005e; // ^
        case XK_dead_tilde:       return 0x007e; // ~
        case XK_dead_diaeresis:   return 0x00a8; // ¨
        default:                  return 0;
    }
}

//-------------------------------------
static void
emit_codepoint_with_dead_state(SWindowData *window_data, SWindowData_X11 *window_data_specific, uint32_t codepoint) {
    if (window_data == NULL || window_data_specific == NULL || codepoint == 0) {
        return;
    }

    if (window_data_specific->pending_dead_keysym != NoSymbol) {
        uint32_t composed = compose_dead_codepoint(window_data_specific->pending_dead_keysym, codepoint);
        if (composed != 0) {
            codepoint = composed;
        }
        else {
            uint32_t accent = dead_keysym_to_codepoint(window_data_specific->pending_dead_keysym);
            if (accent != 0) {
                mfb_dispatch_char_input(window_data, accent);
            }
        }
        window_data_specific->pending_dead_keysym = NoSymbol;
    }

    mfb_dispatch_char_input(window_data, codepoint);
}

//-------------------------------------
void
x11_text_xim_dispatch(SWindowData *window_data, SWindowData_X11 *window_data_specific, XEvent *event) {
    if (window_data == NULL || window_data_specific == NULL || event == NULL) {
        return;
    }

    if (window_data_specific->ic != NULL) {
        char  stack_buffer[64];
        char *text_buffer = stack_buffer;
        int   text_capacity = (int) sizeof(stack_buffer);
        KeySym keysym = NoSymbol;
        Status status = 0;
        int text_size = Xutf8LookupString(window_data_specific->ic, &event->xkey, text_buffer, text_capacity, &keysym, &status);

        if (status == XBufferOverflow) {
            text_capacity = text_size + 1;
            text_buffer = (char *) malloc((size_t) text_capacity);
            if (text_buffer == NULL) {
                MFB_LOG(MFB_LOG_WARNING, "X11MiniFB: failed to allocate buffer for Xutf8LookupString.");
                return;
            }

            text_size = Xutf8LookupString(window_data_specific->ic, &event->xkey, text_buffer, text_capacity, &keysym, &status);
        }

        if ((status == XLookupChars || status == XLookupBoth) && text_size > 0) {
            // Some XIMs still report dead-key keysyms while producing raw base chars.
            // Keep an explicit dead-key state to force expected composition behavior.
            if (is_dead_keysym(keysym)) {
                window_data_specific->pending_dead_keysym = keysym;
            }
            else {
                size_t index = 0;
                while (index < (size_t) text_size) {
                    uint32_t codepoint = 0;
                    if (utf8_decode_next((const unsigned char *) text_buffer, (size_t) text_size, &index, &codepoint) && codepoint != 0) {
                        emit_codepoint_with_dead_state(window_data, window_data_specific, codepoint);
                    }
                }
            }
        }
        else if ((status == XLookupKeySym || status == XLookupBoth) && is_dead_keysym(keysym)) {
            window_data_specific->pending_dead_keysym = keysym;
        }

        if (text_buffer != stack_buffer) {
            free(text_buffer);
        }
        return;
    }

    // Fallback when XIM/XIC is unavailable.
    KeySym keysym = NoSymbol;
    XLookupString(&event->xkey, NULL, 0, &keysym, NULL);
    if (is_dead_keysym(keysym)) {
        window_data_specific->pending_dead_keysym = keysym;
        return;
    }

    uint32_t codepoint = keysym_to_codepoint(keysym);
    if (codepoint == 0) {
        window_data_specific->pending_dead_keysym = NoSymbol;
        return;
    }

    emit_codepoint_with_dead_state(window_data, window_data_specific, codepoint);
}

//-------------------------------------
bool
x11_text_xim_init(SWindowData_X11 *window_data_specific) {
    // The locale modifiers name the input method to talk to, and XOpenIM reads them, so they
    // have to be set first.
    if (XSetLocaleModifiers("") == NULL) {
        MFB_LOG(MFB_LOG_WARNING, "X11MiniFB: XSetLocaleModifiers failed; input method support may be limited.");
    }

    window_data_specific->im = XOpenIM(window_data_specific->display, NULL, NULL, NULL);
    if (window_data_specific->im == NULL) {
        MFB_LOG(MFB_LOG_WARNING, "X11MiniFB: XOpenIM failed; falling back to basic keysym text input.");
        return false;
    }

    window_data_specific->ic = XCreateIC(window_data_specific->im,
                                         XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
                                         XNClientWindow, window_data_specific->window,
                                         XNFocusWindow, window_data_specific->window,
                                         NULL);
    if (window_data_specific->ic == NULL) {
        MFB_LOG(MFB_LOG_WARNING, "X11MiniFB: XCreateIC failed; falling back to basic keysym text input.");
        XCloseIM(window_data_specific->im);
        window_data_specific->im = NULL;
        return false;
    }

    return true;
}

//-------------------------------------
void
x11_text_xim_destroy(SWindowData_X11 *window_data_specific) {
    if (window_data_specific->ic != NULL) {
        XDestroyIC(window_data_specific->ic);
        window_data_specific->ic = NULL;
    }
    if (window_data_specific->im != NULL) {
        XCloseIM(window_data_specific->im);
        window_data_specific->im = NULL;
    }
}

//-------------------------------------
void
x11_text_xim_focus(SWindowData_X11 *window_data_specific, bool focused) {
    // The accent belonged to the visit that ended, so it cannot combine with what comes next.
    if (focused == false) {
        window_data_specific->pending_dead_keysym = NoSymbol;
    }

    if (window_data_specific->ic == NULL) {
        return;
    }

    if (focused == true) {
        XSetICFocus(window_data_specific->ic);
    }
    else {
        XUnsetICFocus(window_data_specific->ic);
    }
}
