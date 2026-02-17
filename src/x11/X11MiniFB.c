#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>

// I cannot find a way to get dpi under VirtualBox
//#include <X11/Xresource.h>
//#include <X11/extensions/Xrandr.h>
#include <xkbcommon/xkbcommon.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <poll.h>
#include <sched.h>
#include <errno.h>
#include <locale.h>
#include <limits.h>
#include <MiniFB.h>
#include <MiniFB_internal.h>
#include "WindowData.h"
#include "WindowData_X11.h"

#if defined(USE_OPENGL_API)
    #include <gl/MiniFB_GL.h>
#endif

static Atom s_delete_window_atom;
static bool s_x11_locale_checked;

//-------------------------------------
static bool
compute_rgba_layout(uint32_t width, uint32_t height, uint32_t *stride_out, size_t *total_bytes_out) {
    if (width == 0 || height == 0) {
        return false;
    }
    if (width > UINT32_MAX / 4u) {
        return false;
    }

    uint32_t stride = width * 4u;
    if (stride > (uint32_t) INT_MAX) {
        return false;
    }

    if (total_bytes_out != NULL) {
        if ((size_t) height > (SIZE_MAX / (size_t) stride)) {
            return false;
        }
        *total_bytes_out = (size_t) stride * (size_t) height;
    }

    if (stride_out != NULL) {
        *stride_out = stride;
    }

    return true;
}

//-------------------------------------
void init_keycodes(SWindowData_X11 *window_data_specific);
Cursor create_blank_cursor(Display *display, Window window);

extern void
stretch_image(uint32_t *srcImage, uint32_t srcX, uint32_t srcY, uint32_t srcWidth, uint32_t srcHeight, uint32_t srcPitch,
              uint32_t *dstImage, uint32_t dstX, uint32_t dstY, uint32_t dstWidth, uint32_t dstHeight, uint32_t dstPitch);

//-------------------------------------
int translate_key(int scancode);
int translate_mod(int state);
int translate_mod_ex(int key, int state, int is_pressed);
void destroy_window_data(SWindowData *window_data);

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
static void
emit_codepoint_with_dead_state(SWindowData *window_data, SWindowData_X11 *window_data_specific, uint32_t codepoint) {
    if (window_data == NULL || window_data_specific == NULL || codepoint == 0) {
        return;
    }

    if (window_data_specific->pending_dead_keysym != NoSymbol) {
        uint32_t composed = compose_dead_codepoint(window_data_specific->pending_dead_keysym, codepoint);
        window_data_specific->pending_dead_keysym = NoSymbol;
        if (composed != 0) {
            codepoint = composed;
        }
    }

    kCall(char_input_func, codepoint);
}

//-------------------------------------
static bool
utf8_decode_next(const unsigned char *bytes, size_t length, size_t *index, uint32_t *codepoint) {
    if (bytes == NULL || index == NULL || codepoint == NULL || *index >= length) {
        return false;
    }

    unsigned char c0 = bytes[*index];
    if (c0 < 0x80) {
        *codepoint = c0;
        *index += 1;
        return true;
    }

    if ((c0 & 0xe0) == 0xc0 && *index + 1 < length) {
        unsigned char c1 = bytes[*index + 1];
        if ((c1 & 0xc0) == 0x80) {
            uint32_t cp = ((uint32_t) (c0 & 0x1f) << 6) | (uint32_t) (c1 & 0x3f);
            if (cp >= 0x80) {
                *codepoint = cp;
                *index += 2;
                return true;
            }
        }
    }
    else if ((c0 & 0xf0) == 0xe0 && *index + 2 < length) {
        unsigned char c1 = bytes[*index + 1];
        unsigned char c2 = bytes[*index + 2];
        if ((c1 & 0xc0) == 0x80 && (c2 & 0xc0) == 0x80) {
            uint32_t cp = ((uint32_t) (c0 & 0x0f) << 12) |
                          ((uint32_t) (c1 & 0x3f) << 6) |
                          (uint32_t) (c2 & 0x3f);
            if (cp >= 0x800 && !(cp >= 0xd800 && cp <= 0xdfff)) {
                *codepoint = cp;
                *index += 3;
                return true;
            }
        }
    }
    else if ((c0 & 0xf8) == 0xf0 && *index + 3 < length) {
        unsigned char c1 = bytes[*index + 1];
        unsigned char c2 = bytes[*index + 2];
        unsigned char c3 = bytes[*index + 3];
        if ((c1 & 0xc0) == 0x80 && (c2 & 0xc0) == 0x80 && (c3 & 0xc0) == 0x80) {
            uint32_t cp = ((uint32_t) (c0 & 0x07) << 18) |
                          ((uint32_t) (c1 & 0x3f) << 12) |
                          ((uint32_t) (c2 & 0x3f) << 6) |
                          (uint32_t) (c3 & 0x3f);
            if (cp >= 0x10000 && cp <= 0x10ffff) {
                *codepoint = cp;
                *index += 4;
                return true;
            }
        }
    }

    // Skip invalid lead byte sequence and continue parsing.
    *index += 1;
    return false;
}

static void
dispatch_text_input(SWindowData *window_data, SWindowData_X11 *window_data_specific, XEvent *event) {
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
                mfb_log(MFB_LOG_WARNING, "X11MiniFB warning: failed to allocate buffer for Xutf8LookupString.");
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
static void
process_event(SWindowData *window_data, XEvent *event) {
    switch (event->type) {
        case KeyPress:
        case KeyRelease:
        {
            SWindowData_X11 *window_data_specific = (SWindowData_X11 *) window_data->specific;
            if ((event->type == KeyRelease) &&
                XEventsQueued(window_data_specific->display, QueuedAfterReading)) {
                XEvent nev;
                XPeekEvent(window_data_specific->display, &nev);

                if ((nev.type == KeyPress) && (nev.xkey.time == event->xkey.time) &&
                    (nev.xkey.keycode == event->xkey.keycode)) {
                    /* Key wasn’t actually released */
                    return;
                }
            }

            mfb_key key_code      = (mfb_key) translate_key(event->xkey.keycode);
            int is_pressed        = (event->type == KeyPress);
            window_data->mod_keys = translate_mod_ex(key_code, event->xkey.state, is_pressed);

            if (key_code != KB_KEY_UNKNOWN) {
                window_data->key_status[key_code] = (uint8_t) is_pressed;
            }
            kCall(keyboard_func, key_code, (mfb_key_mod) window_data->mod_keys, is_pressed);

            if (event->type == KeyPress) {
                dispatch_text_input(window_data, window_data_specific, event);
            }
        }
        break;

        case ButtonPress:
        case ButtonRelease:
        {
            mfb_mouse_button button = (mfb_mouse_button) event->xbutton.button;
            int          is_pressed = (event->type == ButtonPress);
            window_data->mod_keys   = translate_mod(event->xkey.state);

            // Swap mouse right and middle for parity with other platforms:
            // https://github.com/emoon/minifb/issues/65
            switch (button) {
                case Button2:
                    button = Button3;
                    break;
                case Button3:
                    button = Button2;
                    break;
            }

            switch (button) {
                case Button1:
                case Button2:
                case Button3:
                    window_data->mouse_button_status[button & 0x07] = is_pressed;
                    kCall(mouse_btn_func, button, (mfb_key_mod) window_data->mod_keys, is_pressed);
                    break;

                case Button4:
                    window_data->mouse_wheel_y = 1.0f;
                    kCall(mouse_wheel_func, (mfb_key_mod) window_data->mod_keys, 0.0f, window_data->mouse_wheel_y);
                    break;
                case Button5:
                    window_data->mouse_wheel_y = -1.0f;
                    kCall(mouse_wheel_func, (mfb_key_mod) window_data->mod_keys, 0.0f, window_data->mouse_wheel_y);
                    break;

                case 6:
                    window_data->mouse_wheel_x = 1.0f;
                    kCall(mouse_wheel_func, (mfb_key_mod) window_data->mod_keys, window_data->mouse_wheel_x, 0.0f);
                    break;
                case 7:
                    window_data->mouse_wheel_x = -1.0f;
                    kCall(mouse_wheel_func, (mfb_key_mod) window_data->mod_keys, window_data->mouse_wheel_x, 0.0f);
                    break;

                default:
                    window_data->mouse_button_status[(button - 4) & 0x07] = is_pressed;
                    kCall(mouse_btn_func, (mfb_mouse_button) (button - 4), (mfb_key_mod) window_data->mod_keys, is_pressed);
                    break;
            }
        }
        break;

        case MotionNotify:
            window_data->mouse_pos_x = event->xmotion.x;
            window_data->mouse_pos_y = event->xmotion.y;
            kCall(mouse_move_func, event->xmotion.x, event->xmotion.y);
            break;

        case ConfigureNotify:
        {
            window_data->window_width  = event->xconfigure.width;
            window_data->window_height = event->xconfigure.height;
            resize_dst(window_data, event->xconfigure.width, event->xconfigure.height);

#if defined(USE_OPENGL_API)
            resize_GL(window_data);
#else
            SWindowData_X11 *window_data_specific = (SWindowData_X11 *) window_data->specific;
            if (window_data_specific->image_scaler != NULL) {
                window_data_specific->image_scaler->data = NULL;
                XDestroyImage(window_data_specific->image_scaler);
                window_data_specific->image_scaler        = NULL;
                window_data_specific->image_scaler_width  = 0;
                window_data_specific->image_scaler_height = 0;
            }
            XClearWindow(window_data_specific->display, window_data_specific->window);
#endif
            kCall(resize_func, window_data->window_width, window_data->window_height);
        }
        break;

        case EnterNotify:
        case LeaveNotify:
        break;

        case FocusIn:
            window_data->is_active = true;
            if (window_data->specific) {
                SWindowData_X11 *window_data_specific = (SWindowData_X11 *) window_data->specific;
                if (window_data_specific->ic != NULL) {
                    XSetICFocus(window_data_specific->ic);
                }
            }
            kCall(active_func, true);
            break;

        case FocusOut:
            window_data->is_active = false;
            if (window_data->specific) {
                SWindowData_X11 *window_data_specific = (SWindowData_X11 *) window_data->specific;
                if (window_data_specific->ic != NULL) {
                    XUnsetICFocus(window_data_specific->ic);
                }
            }
            kCall(active_func, false);
            break;

        case DestroyNotify:
            window_data->close = true;
            return;
            break;

        case ClientMessage:
        {
            if ((Atom)event->xclient.data.l[0] == s_delete_window_atom) {
                if (window_data) {
                    bool destroy = false;

                    // Obtain a confirmation of close
                    if (!window_data->close_func || window_data->close_func((struct mfb_window*)window_data)) {
                        destroy = true;
                    }

                    if (destroy) {
                        window_data->close = true;
                        return;
                    }
                }
            }
        }
        break;
    }
}

//-------------------------------------
static inline void
update_events(SWindowData *window_data, Display *display) {
    XEvent event;

    while (XPending(display) > 0) {
        XNextEvent(display, &event);
        process_event(window_data, &event);
    }
}

//-------------------------------------
struct mfb_window *
mfb_open_ex(const char *title, unsigned width, unsigned height, unsigned flags) {
    int depth, i, formatCount, convDepth = -1;
    XPixmapFormatValues* formats;
    XSetWindowAttributes windowAttributes;
    XSizeHints sizeHints;
    Visual* visual;
    uint32_t initial_stride = 0;
    size_t initial_total_bytes = 0;

    if (width == 0 || height == 0) {
        mfb_log(MFB_LOG_ERROR, "X11MiniFB error: invalid window size %ux%u.", width, height);
        return NULL;
    }
    if (!compute_rgba_layout(width, height, &initial_stride, &initial_total_bytes)) {
        mfb_log(MFB_LOG_ERROR, "X11MiniFB error: window size %ux%u is too large for X11 image layout.", width, height);
        return NULL;
    }

    SWindowData *window_data = (SWindowData *) malloc(sizeof(SWindowData));
    if (!window_data) {
        mfb_log(MFB_LOG_ERROR, "X11MiniFB error: failed to allocate SWindowData.");
        return NULL;
    }
    memset(window_data, 0, sizeof(SWindowData));

    SWindowData_X11 *window_data_specific = (SWindowData_X11 *) malloc(sizeof(SWindowData_X11));
    if (!window_data_specific) {
        mfb_log(MFB_LOG_ERROR, "X11MiniFB error: failed to allocate SWindowData_X11.");
        free(window_data);
        return NULL;
    }
    memset(window_data_specific, 0, sizeof(SWindowData_X11));
    window_data->specific = window_data_specific;

    window_data_specific->display = XOpenDisplay(0);
    if (!window_data_specific->display) {
        mfb_log(MFB_LOG_ERROR, "X11MiniFB error: XOpenDisplay failed.");
        free(window_data);
        free(window_data_specific);
        return NULL;
    }

    if (!s_x11_locale_checked) {
        const char *locale = setlocale(LC_CTYPE, NULL);
        if (locale == NULL || strcmp(locale, "C") == 0 || strcmp(locale, "POSIX") == 0) {
            mfb_log(MFB_LOG_WARNING, "X11MiniFB warning: LC_CTYPE locale is not configured for UTF-8; IME/dead-key input may be limited.");
        }
        s_x11_locale_checked = true;
    }

    if (XSetLocaleModifiers("") == NULL) {
        mfb_log(MFB_LOG_WARNING, "X11MiniFB warning: XSetLocaleModifiers failed; input method support may be limited.");
    }

    init_keycodes(window_data_specific);

    window_data_specific->screen = DefaultScreen(window_data_specific->display);

    visual   = DefaultVisual(window_data_specific->display, window_data_specific->screen);
    formats  = XListPixmapFormats(window_data_specific->display, &formatCount);
    depth    = DefaultDepth(window_data_specific->display, window_data_specific->screen);
    if (formats == NULL || formatCount <= 0) {
        mfb_log(MFB_LOG_ERROR, "X11MiniFB error: XListPixmapFormats returned no formats.");
        XCloseDisplay(window_data_specific->display);
        free(window_data_specific);
        free(window_data);
        return NULL;
    }

    Window defaultRootWindow = DefaultRootWindow(window_data_specific->display);

    for (i = 0; i < formatCount; ++i) {
        if (depth == formats[i].depth) {
            convDepth = formats[i].bits_per_pixel;
            break;
        }
    }

    XFree(formats);

    // We only support 32-bit right now
    if (convDepth != 32) {
        mfb_log(MFB_LOG_ERROR, "X11MiniFB error: unsupported visual depth conversion (%d bpp), expected 32 bpp.", convDepth);
        XCloseDisplay(window_data_specific->display);
        free(window_data_specific);
        free(window_data);
        return NULL;
    }

    int screenWidth  = DisplayWidth(window_data_specific->display, window_data_specific->screen);
    int screenHeight = DisplayHeight(window_data_specific->display, window_data_specific->screen);

    windowAttributes.border_pixel     = BlackPixel(window_data_specific->display, window_data_specific->screen);
    windowAttributes.background_pixel = BlackPixel(window_data_specific->display, window_data_specific->screen);
    windowAttributes.backing_store    = NotUseful;

    int posX, posY;
    int windowWidth, windowHeight;

    window_data->window_width  = width;
    window_data->window_height = height;
    window_data->buffer_width  = width;
    window_data->buffer_height = height;
    window_data->buffer_stride = initial_stride;
    calc_dst_factor(window_data, width, height);

    if (flags & WF_FULLSCREEN_DESKTOP) {
        posX         = 0;
        posY         = 0;
        windowWidth  = screenWidth;
        windowHeight = screenHeight;
    }
    else {
        posX         = (screenWidth  - width)  / 2;
        posY         = (screenHeight - height) / 2;
        windowWidth  = width;
        windowHeight = height;
    }

    window_data_specific->window = XCreateWindow(
                    window_data_specific->display,
                    defaultRootWindow,
                    posX, posY,
                    windowWidth, windowHeight,
                    0,
                    depth,
                    InputOutput,
                    visual,
                    CWBackPixel | CWBorderPixel | CWBackingStore,
                    &windowAttributes);
    if (!window_data_specific->window) {
        mfb_log(MFB_LOG_ERROR, "X11MiniFB error: XCreateWindow failed.");
        XCloseDisplay(window_data_specific->display);
        free(window_data_specific);
        free(window_data);
        return NULL;
    }

    XSelectInput(window_data_specific->display, window_data_specific->window,
        KeyPressMask | KeyReleaseMask
        | ButtonPressMask | ButtonReleaseMask | PointerMotionMask
        | StructureNotifyMask | ExposureMask
        | FocusChangeMask
        | EnterWindowMask | LeaveWindowMask
    );

    window_data_specific->im = XOpenIM(window_data_specific->display, NULL, NULL, NULL);
    if (window_data_specific->im == NULL) {
        mfb_log(MFB_LOG_WARNING, "X11MiniFB warning: XOpenIM failed; falling back to basic keysym text input.");
    }
    else {
        window_data_specific->ic = XCreateIC(window_data_specific->im,
                                             XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
                                             XNClientWindow, window_data_specific->window,
                                             XNFocusWindow, window_data_specific->window,
                                             NULL);
        if (window_data_specific->ic == NULL) {
            mfb_log(MFB_LOG_WARNING, "X11MiniFB warning: XCreateIC failed; falling back to basic keysym text input.");
            XCloseIM(window_data_specific->im);
            window_data_specific->im = NULL;
        }
    }

    XStoreName(window_data_specific->display, window_data_specific->window, title);

    if (flags & WF_BORDERLESS) {
        struct StyleHints {
            unsigned long   flags;
            unsigned long   functions;
            unsigned long   decorations;
            long            inputMode;
            unsigned long   status;
        } sh = {
            .flags       = 2,
            .functions   = 0,
            .decorations = 0,
            .inputMode   = 0,
            .status      = 0,
        };
        Atom sh_p = XInternAtom(window_data_specific->display, "_MOTIF_WM_HINTS", True);
        if (sh_p == None) {
            mfb_log(MFB_LOG_WARNING, "X11MiniFB warning: _MOTIF_WM_HINTS atom is unavailable; borderless hint may be ignored.");
        }
        else {
            XChangeProperty(window_data_specific->display, window_data_specific->window, sh_p, sh_p, 32, PropModeReplace, (unsigned char*)&sh, 5);
        }
    }

    if (flags & WF_ALWAYS_ON_TOP) {
        Atom sa_p = XInternAtom(window_data_specific->display, "_NET_WM_STATE_ABOVE", False);
        Atom state_atom = XInternAtom(window_data_specific->display, "_NET_WM_STATE", False);
        if (sa_p == None || state_atom == None) {
            mfb_log(MFB_LOG_WARNING, "X11MiniFB warning: always-on-top WM atoms are unavailable; request may be ignored.");
        }
        else {
            XChangeProperty(window_data_specific->display, window_data_specific->window, state_atom, XA_ATOM, 32, PropModeReplace, (unsigned char *)&sa_p, 1);
        }
    }

    if (flags & WF_FULLSCREEN) {
        Atom sf_p = XInternAtom(window_data_specific->display, "_NET_WM_STATE_FULLSCREEN", True);
        Atom state_atom = XInternAtom(window_data_specific->display, "_NET_WM_STATE", True);
        if (sf_p == None || state_atom == None) {
            mfb_log(MFB_LOG_WARNING, "X11MiniFB warning: fullscreen WM atoms are unavailable; fullscreen request may be ignored.");
        }
        else {
            XChangeProperty(window_data_specific->display, window_data_specific->window, state_atom, XA_ATOM, 32, PropModeReplace, (unsigned char*)&sf_p, 1);
        }
    }

    sizeHints.flags      = PPosition | PMinSize | PMaxSize;
    sizeHints.x          = 0;
    sizeHints.y          = 0;
    sizeHints.min_width  = width;
    sizeHints.min_height = height;
    if (flags & WF_RESIZABLE) {
        sizeHints.max_width  = screenWidth;
        sizeHints.max_height = screenHeight;
    }
    else {
        sizeHints.max_width  = width;
        sizeHints.max_height = height;
    }

    s_delete_window_atom = XInternAtom(window_data_specific->display, "WM_DELETE_WINDOW", False);
    if (s_delete_window_atom == None) {
        mfb_log(MFB_LOG_WARNING, "X11MiniFB warning: WM_DELETE_WINDOW atom unavailable; close requests may not be delivered.");
    }
    else {
        XSetWMProtocols(window_data_specific->display, window_data_specific->window, &s_delete_window_atom, 1);
    }

#if defined(USE_OPENGL_API)
    if (create_GL_context(window_data) == false) {
        mfb_log(MFB_LOG_ERROR, "X11MiniFB error: create_GL_context failed.");
        destroy_window_data(window_data);
        return NULL;
    }

#else
    window_data_specific->image = XCreateImage(window_data_specific->display, CopyFromParent, depth, ZPixmap, 0, NULL, width, height, 32, (int) initial_stride);
    if (window_data_specific->image == NULL) {
        mfb_log(MFB_LOG_ERROR, "X11MiniFB error: XCreateImage failed.");
        destroy_window_data(window_data);
        return NULL;
    }
#endif

    XSetWMNormalHints(window_data_specific->display, window_data_specific->window, &sizeHints);
    XClearWindow(window_data_specific->display, window_data_specific->window);
    XMapRaised(window_data_specific->display, window_data_specific->window);
    XFlush(window_data_specific->display);

    window_data_specific->gc = DefaultGC(window_data_specific->display, window_data_specific->screen);

    window_data_specific->timer = mfb_timer_create();
    if (window_data_specific->timer == NULL) {
        mfb_log(MFB_LOG_ERROR, "X11MiniFB error: mfb_timer_create failed.");
        destroy_window_data(window_data);
        return NULL;
    }

    window_data->is_cursor_visible = true;
    window_data_specific->invis_cursor = create_blank_cursor(window_data_specific->display, window_data_specific->window);
    if (window_data_specific->invis_cursor == None) {
        mfb_log(MFB_LOG_WARNING, "X11MiniFB warning: failed to create invisible cursor; cursor hiding may not work.");
    }

    mfb_set_keyboard_callback((struct mfb_window *) window_data, keyboard_default);

    #if defined(USE_OPENGL_API)
        mfb_log(MFB_LOG_DEBUG, "Window created using OpenGL API");
    #else
        mfb_log(MFB_LOG_DEBUG, "Window created using X11 API");
    #endif

    window_data->is_initialized = true;
    return (struct mfb_window *) window_data;
}

//-------------------------------------
mfb_update_state
mfb_update_ex(struct mfb_window *window, void *buffer, unsigned width, unsigned height) {
    SWindowData *window_data = (SWindowData *) window;
    if (window_data ==  NULL) {
        mfb_log(MFB_LOG_DEBUG, "mfb_update_ex: invalid window");
        return STATE_INVALID_WINDOW;
    }

    // Early exit
    if (window_data->close) {
        mfb_log(MFB_LOG_DEBUG, "mfb_update_ex: window requested close");
        destroy_window_data(window_data);
        return STATE_EXIT;
    }

    if (buffer == NULL) {
        mfb_log(MFB_LOG_DEBUG, "mfb_update_ex: invalid buffer");
        return STATE_INVALID_BUFFER;
    }

    SWindowData_X11 *window_data_specific = (SWindowData_X11 *) window_data->specific;
    if (window_data_specific ==  NULL) {
        mfb_log(MFB_LOG_DEBUG, "mfb_update_ex: invalid window specific data");
        return STATE_INVALID_WINDOW;
    }

    Display *display = window_data_specific->display;
    if (display == NULL) {
        mfb_log(MFB_LOG_ERROR, "X11MiniFB error: missing X11 display in mfb_update_ex.");
        return STATE_INVALID_WINDOW;
    }

    if (width == 0 || height == 0) {
        mfb_log(MFB_LOG_DEBUG, "mfb_update_ex: invalid buffer size %ux%u", width, height);
        return STATE_INVALID_BUFFER;
    }

#if !defined(USE_OPENGL_API)
    bool different_size = false;
#endif

    if (window_data->buffer_width != width || window_data->buffer_height != height) {
        uint32_t new_stride = 0;
        size_t new_total_bytes = 0;
        if (!compute_rgba_layout(width, height, &new_stride, &new_total_bytes)) {
            mfb_log(MFB_LOG_ERROR, "X11MiniFB error: invalid buffer layout for size %ux%u.", width, height);
            return STATE_INVALID_BUFFER;
        }
        window_data->buffer_width  = width;
        window_data->buffer_stride = new_stride;
        window_data->buffer_height = height;
#if !defined(USE_OPENGL_API)
        different_size = true;
#endif
    }

#if !defined(USE_OPENGL_API)

    if (different_size || window_data->buffer_width != window_data->dst_width || window_data->buffer_height != window_data->dst_height) {
        if (window_data_specific->image_scaler_width != window_data->dst_width || window_data_specific->image_scaler_height != window_data->dst_height) {
            if (window_data_specific->image_scaler != NULL) {
                window_data_specific->image_scaler->data = NULL;
                XDestroyImage(window_data_specific->image_scaler);
            }
            if (window_data_specific->image_buffer != NULL) {
                free(window_data_specific->image_buffer);
                window_data_specific->image_buffer = NULL;
            }
            int depth = DefaultDepth(display, window_data_specific->screen);
            uint32_t scaler_stride = 0;
            size_t scaler_size = 0;
            if (!compute_rgba_layout(window_data->dst_width, window_data->dst_height, &scaler_stride, &scaler_size)) {
                mfb_log(MFB_LOG_ERROR, "X11MiniFB error: invalid scaler layout for size %ux%u.", window_data->dst_width, window_data->dst_height);
                return STATE_INTERNAL_ERROR;
            }

            window_data_specific->image_buffer = malloc(scaler_size);
            if (window_data_specific->image_buffer == NULL) {
                mfb_log(MFB_LOG_ERROR, "X11MiniFB error: failed to allocate image scaler buffer.");
                return STATE_INTERNAL_ERROR;
            }
            window_data_specific->image_scaler_width  = window_data->dst_width;
            window_data_specific->image_scaler_height = window_data->dst_height;
            window_data_specific->image_scaler = XCreateImage(display, CopyFromParent, depth, ZPixmap, 0, NULL, window_data_specific->image_scaler_width, window_data_specific->image_scaler_height, 32, (int) scaler_stride);
            if (window_data_specific->image_scaler == NULL) {
                mfb_log(MFB_LOG_ERROR, "X11MiniFB error: XCreateImage failed for scaler image.");
                free(window_data_specific->image_buffer);
                window_data_specific->image_buffer = NULL;
                window_data_specific->image_scaler_width  = 0;
                window_data_specific->image_scaler_height = 0;
                return STATE_INTERNAL_ERROR;
            }
        }
    }

    if (window_data_specific->image_scaler != NULL) {
        stretch_image((uint32_t *) buffer, 0, 0, window_data->buffer_width, window_data->buffer_height, window_data->buffer_width,
                      (uint32_t *) window_data_specific->image_buffer, 0, 0, window_data->dst_width, window_data->dst_height, window_data->dst_width);
        window_data_specific->image_scaler->data = (char *) window_data_specific->image_buffer;
        XPutImage(display, window_data_specific->window, window_data_specific->gc, window_data_specific->image_scaler, 0, 0, window_data->dst_offset_x, window_data->dst_offset_y, window_data->dst_width, window_data->dst_height);
    }
    else {
        if (window_data_specific->image == NULL) {
            mfb_log(MFB_LOG_ERROR, "X11MiniFB error: missing base XImage in mfb_update_ex.");
            return STATE_INTERNAL_ERROR;
        }
        window_data_specific->image->data = (char *) buffer;
        XPutImage(display, window_data_specific->window, window_data_specific->gc, window_data_specific->image, 0, 0, window_data->dst_offset_x, window_data->dst_offset_y, window_data->dst_width, window_data->dst_height);
    }

#else

    redraw_GL(window_data, buffer);

#endif

    XFlush(display);
    update_events(window_data, display);
    if (window_data->close) {
        mfb_log(MFB_LOG_DEBUG, "mfb_update_ex: window closed after event processing");
        destroy_window_data(window_data);
        return STATE_EXIT;
    }

    return STATE_OK;
}

//-------------------------------------
mfb_update_state
mfb_update_events(struct mfb_window *window) {
    SWindowData *window_data = (SWindowData *) window;
    if (window_data ==  NULL) {
        mfb_log(MFB_LOG_DEBUG, "mfb_update_events: invalid window");
        return STATE_INVALID_WINDOW;
    }

    // Early exit
    if (window_data->close) {
        mfb_log(MFB_LOG_DEBUG, "mfb_update_events: window requested close");
        destroy_window_data(window_data);
        return STATE_EXIT;
    }

    SWindowData_X11 *window_data_specific = (SWindowData_X11 *) window_data->specific;
    if (window_data_specific ==  NULL) {
        mfb_log(MFB_LOG_DEBUG, "mfb_update_events: invalid window specific data");
        return STATE_INVALID_WINDOW;
    }

    Display *display = window_data_specific->display;
    if (display == NULL) {
        mfb_log(MFB_LOG_ERROR, "X11MiniFB error: missing X11 display in mfb_update_events.");
        return STATE_INVALID_WINDOW;
    }
    XFlush(display);

    update_events(window_data, display);
    if (window_data->close) {
        mfb_log(MFB_LOG_DEBUG, "mfb_update_events: window closed after event processing");
        destroy_window_data(window_data);
        return STATE_EXIT;
    }

    return STATE_OK;
}

//-------------------------------------
extern double   g_time_for_frame;
extern bool     g_use_hardware_sync;

//-------------------------------------
bool
mfb_wait_sync(struct mfb_window *window) {
    SWindowData *window_data = (SWindowData *) window;
    if (window_data == NULL) {
        mfb_log(MFB_LOG_DEBUG, "mfb_wait_sync: invalid window");
        return false;
    }
    if (window_data->close) {
        mfb_log(MFB_LOG_DEBUG, "mfb_wait_sync: window requested close");
        destroy_window_data(window_data);
        return false;
    }

    SWindowData_X11 *window_data_specific = (SWindowData_X11 *) window_data->specific;
    if (window_data_specific == NULL) {
        mfb_log(MFB_LOG_DEBUG, "mfb_wait_sync: invalid window specific data");
        return false;
    }
    if (window_data_specific->display == NULL) {
        mfb_log(MFB_LOG_ERROR, "X11MiniFB error: mfb_wait_sync has a null X11 display handle.");
        return false;
    }
    if (window_data_specific->timer == NULL) {
        mfb_log(MFB_LOG_ERROR, "X11MiniFB error: mfb_wait_sync missing frame timer state.");
        return false;
    }

    Display *display = window_data_specific->display;
    const int fd = ConnectionNumber(display);

    XFlush(display); // send any pending requests before we potentially sleep

    update_events(window_data, display);
    if (window_data->close) {
        mfb_log(MFB_LOG_DEBUG, "mfb_wait_sync: window closed after event processing");
        destroy_window_data(window_data);
        return false;
    }

    // Hardware sync: no software pacing
    if (g_use_hardware_sync) {
        return true;
    }

    // Software pacing: Wait only the remaining time; wake on input
    for (;;) {
        double elapsed_time = mfb_timer_now(window_data_specific->timer);
        if (elapsed_time >= g_time_for_frame)
            break;

        double remaining_ms = (g_time_for_frame - elapsed_time) * 1000.0;

        // Leave ~1 ms margin to avoid oversleep
        if (remaining_ms > 1.5) {
            int timeout_ms = (int) (remaining_ms - 1.0);
            if (timeout_ms < 0)
                timeout_ms = 0;

            struct pollfd pfd;
            pfd.fd = fd;
            pfd.events = POLLIN;
            pfd.revents = 0;

            if (poll(&pfd, 1, timeout_ms) < 0 && errno != EINTR) {
                mfb_log(MFB_LOG_ERROR, "X11MiniFB error: poll failed in mfb_wait_sync (%s).", strerror(errno));
                return false;
            }
        }
        else {
            sched_yield(); // or nanosleep((const struct timespec){0,0}, NULL);
        }

        update_events(window_data, display);

        if (window_data->close) {
            mfb_log(MFB_LOG_DEBUG, "mfb_wait_sync: window closed while waiting for frame sync");
            destroy_window_data(window_data);
            return false;
        }
    }

    mfb_timer_compensated_reset(window_data_specific->timer);
    return true;
}

//-------------------------------------
void
destroy_window_data(SWindowData *window_data)  {
    if (window_data != NULL) {
        if (window_data->specific != NULL) {
            SWindowData_X11   *window_data_specific = (SWindowData_X11 *) window_data->specific;
            Display *display = window_data_specific->display;

#if defined(USE_OPENGL_API)
            destroy_GL_context(window_data);
#else
            if (window_data_specific->image != NULL) {
                window_data_specific->image->data = NULL;
                XDestroyImage(window_data_specific->image);
            }
#endif

#if !defined(USE_OPENGL_API)
            if (window_data_specific->image_scaler != NULL) {
                window_data_specific->image_scaler->data = NULL;
                XDestroyImage(window_data_specific->image_scaler);
            }

            if (window_data_specific->image_buffer != NULL) {
                free(window_data_specific->image_buffer);
            }
#endif

            if (window_data_specific->ic != NULL) {
                XDestroyIC(window_data_specific->ic);
            }
            if (window_data_specific->im != NULL) {
                XCloseIM(window_data_specific->im);
            }
            if (display != NULL && window_data_specific->window != 0) {
                XDestroyWindow(display, window_data_specific->window);
            }

            mfb_timer_destroy(window_data_specific->timer);
            if (display != NULL && window_data_specific->invis_cursor != 0) {
                XFreeCursor(display, window_data_specific->invis_cursor);
            }
            if (display != NULL) {
                XCloseDisplay(display);
            }

            memset(window_data_specific, 0, sizeof(SWindowData_X11));
            free(window_data_specific);
        }
        memset(window_data, 0, sizeof(SWindowData));
        free(window_data);
   }
}

//-------------------------------------
extern short int g_keycodes[512];

//-------------------------------------
static int
translateKeyCodeB(int keySym) {

    switch (keySym) {
        case XK_KP_0:           return KB_KEY_KP_0;
        case XK_KP_1:           return KB_KEY_KP_1;
        case XK_KP_2:           return KB_KEY_KP_2;
        case XK_KP_3:           return KB_KEY_KP_3;
        case XK_KP_4:           return KB_KEY_KP_4;
        case XK_KP_5:           return KB_KEY_KP_5;
        case XK_KP_6:           return KB_KEY_KP_6;
        case XK_KP_7:           return KB_KEY_KP_7;
        case XK_KP_8:           return KB_KEY_KP_8;
        case XK_KP_9:           return KB_KEY_KP_9;
        case XK_KP_Separator:
        case XK_KP_Decimal:     return KB_KEY_KP_DECIMAL;
        case XK_KP_Equal:       return KB_KEY_KP_EQUAL;
        case XK_KP_Enter:       return KB_KEY_KP_ENTER;
    }

    return KB_KEY_UNKNOWN;
}

//-------------------------------------
static int
translateKeyCodeA(int keySym) {
    switch (keySym) {
        case XK_Escape:         return KB_KEY_ESCAPE;
        case XK_Tab:            return KB_KEY_TAB;
        case XK_Shift_L:        return KB_KEY_LEFT_SHIFT;
        case XK_Shift_R:        return KB_KEY_RIGHT_SHIFT;
        case XK_Control_L:      return KB_KEY_LEFT_CONTROL;
        case XK_Control_R:      return KB_KEY_RIGHT_CONTROL;
        case XK_Meta_L:
        case XK_Alt_L:          return KB_KEY_LEFT_ALT;
        case XK_Mode_switch:      // Mapped to Alt_R on many keyboards
        case XK_ISO_Level3_Shift: // AltGr on at least some machines
        case XK_Meta_R:
        case XK_Alt_R:          return KB_KEY_RIGHT_ALT;
        case XK_Super_L:        return KB_KEY_LEFT_SUPER;
        case XK_Super_R:        return KB_KEY_RIGHT_SUPER;
        case XK_Menu:           return KB_KEY_MENU;
        case XK_Num_Lock:       return KB_KEY_NUM_LOCK;
        case XK_Caps_Lock:      return KB_KEY_CAPS_LOCK;
        case XK_Print:          return KB_KEY_PRINT_SCREEN;
        case XK_Scroll_Lock:    return KB_KEY_SCROLL_LOCK;
        case XK_Pause:          return KB_KEY_PAUSE;
        case XK_Delete:         return KB_KEY_DELETE;
        case XK_BackSpace:      return KB_KEY_BACKSPACE;
        case XK_Return:         return KB_KEY_ENTER;
        case XK_Home:           return KB_KEY_HOME;
        case XK_End:            return KB_KEY_END;
        case XK_Page_Up:        return KB_KEY_PAGE_UP;
        case XK_Page_Down:      return KB_KEY_PAGE_DOWN;
        case XK_Insert:         return KB_KEY_INSERT;
        case XK_Left:           return KB_KEY_LEFT;
        case XK_Right:          return KB_KEY_RIGHT;
        case XK_Down:           return KB_KEY_DOWN;
        case XK_Up:             return KB_KEY_UP;
        case XK_F1:             return KB_KEY_F1;
        case XK_F2:             return KB_KEY_F2;
        case XK_F3:             return KB_KEY_F3;
        case XK_F4:             return KB_KEY_F4;
        case XK_F5:             return KB_KEY_F5;
        case XK_F6:             return KB_KEY_F6;
        case XK_F7:             return KB_KEY_F7;
        case XK_F8:             return KB_KEY_F8;
        case XK_F9:             return KB_KEY_F9;
        case XK_F10:            return KB_KEY_F10;
        case XK_F11:            return KB_KEY_F11;
        case XK_F12:            return KB_KEY_F12;
        case XK_F13:            return KB_KEY_F13;
        case XK_F14:            return KB_KEY_F14;
        case XK_F15:            return KB_KEY_F15;
        case XK_F16:            return KB_KEY_F16;
        case XK_F17:            return KB_KEY_F17;
        case XK_F18:            return KB_KEY_F18;
        case XK_F19:            return KB_KEY_F19;
        case XK_F20:            return KB_KEY_F20;
        case XK_F21:            return KB_KEY_F21;
        case XK_F22:            return KB_KEY_F22;
        case XK_F23:            return KB_KEY_F23;
        case XK_F24:            return KB_KEY_F24;
        case XK_F25:            return KB_KEY_F25;

        // Numeric keypad
        case XK_KP_Divide:      return KB_KEY_KP_DIVIDE;
        case XK_KP_Multiply:    return KB_KEY_KP_MULTIPLY;
        case XK_KP_Subtract:    return KB_KEY_KP_SUBTRACT;
        case XK_KP_Add:         return KB_KEY_KP_ADD;

        // These should have been detected in secondary keysym test above!
        case XK_KP_Insert:      return KB_KEY_KP_0;
        case XK_KP_End:         return KB_KEY_KP_1;
        case XK_KP_Down:        return KB_KEY_KP_2;
        case XK_KP_Page_Down:   return KB_KEY_KP_3;
        case XK_KP_Left:        return KB_KEY_KP_4;
        case XK_KP_Right:       return KB_KEY_KP_6;
        case XK_KP_Home:        return KB_KEY_KP_7;
        case XK_KP_Up:          return KB_KEY_KP_8;
        case XK_KP_Page_Up:     return KB_KEY_KP_9;
        case XK_KP_Delete:      return KB_KEY_KP_DECIMAL;
        case XK_KP_Equal:       return KB_KEY_KP_EQUAL;
        case XK_KP_Enter:       return KB_KEY_KP_ENTER;

        // Last resort: Check for printable keys (should not happen if the XKB
        // extension is available). This will give a layout dependent mapping
        // (which is wrong, and we may miss some keys, especially on non-US
        // keyboards), but it's better than nothing...
        case XK_a:              return KB_KEY_A;
        case XK_b:              return KB_KEY_B;
        case XK_c:              return KB_KEY_C;
        case XK_d:              return KB_KEY_D;
        case XK_e:              return KB_KEY_E;
        case XK_f:              return KB_KEY_F;
        case XK_g:              return KB_KEY_G;
        case XK_h:              return KB_KEY_H;
        case XK_i:              return KB_KEY_I;
        case XK_j:              return KB_KEY_J;
        case XK_k:              return KB_KEY_K;
        case XK_l:              return KB_KEY_L;
        case XK_m:              return KB_KEY_M;
        case XK_n:              return KB_KEY_N;
        case XK_o:              return KB_KEY_O;
        case XK_p:              return KB_KEY_P;
        case XK_q:              return KB_KEY_Q;
        case XK_r:              return KB_KEY_R;
        case XK_s:              return KB_KEY_S;
        case XK_t:              return KB_KEY_T;
        case XK_u:              return KB_KEY_U;
        case XK_v:              return KB_KEY_V;
        case XK_w:              return KB_KEY_W;
        case XK_x:              return KB_KEY_X;
        case XK_y:              return KB_KEY_Y;
        case XK_z:              return KB_KEY_Z;
        case XK_1:              return KB_KEY_1;
        case XK_2:              return KB_KEY_2;
        case XK_3:              return KB_KEY_3;
        case XK_4:              return KB_KEY_4;
        case XK_5:              return KB_KEY_5;
        case XK_6:              return KB_KEY_6;
        case XK_7:              return KB_KEY_7;
        case XK_8:              return KB_KEY_8;
        case XK_9:              return KB_KEY_9;
        case XK_0:              return KB_KEY_0;
        case XK_space:          return KB_KEY_SPACE;
        case XK_minus:          return KB_KEY_MINUS;
        case XK_equal:          return KB_KEY_EQUAL;
        case XK_bracketleft:    return KB_KEY_LEFT_BRACKET;
        case XK_bracketright:   return KB_KEY_RIGHT_BRACKET;
        case XK_backslash:      return KB_KEY_BACKSLASH;
        case XK_semicolon:      return KB_KEY_SEMICOLON;
        case XK_apostrophe:     return KB_KEY_APOSTROPHE;
        case XK_dead_acute:     return KB_KEY_APOSTROPHE;
        case XK_dead_diaeresis: return KB_KEY_APOSTROPHE;
        case XK_grave:          return KB_KEY_GRAVE_ACCENT;
        case XK_dead_grave:     return KB_KEY_GRAVE_ACCENT;
        case XK_dead_circumflex:return KB_KEY_GRAVE_ACCENT;
        case XK_dead_tilde:     return KB_KEY_GRAVE_ACCENT;
        case XK_comma:          return KB_KEY_COMMA;
        case XK_period:         return KB_KEY_PERIOD;
        case XK_slash:          return KB_KEY_SLASH;
        case XK_less:           return KB_KEY_WORLD_1; // At least in some layouts...
        default:                break;
    }

    return KB_KEY_UNKNOWN;
}

//-------------------------------------
void
init_keycodes(SWindowData_X11 *window_data_specific) {
    size_t  i;
    int     keySym;

    // Clear keys
    for (i = 0; i < sizeof(g_keycodes) / sizeof(g_keycodes[0]); ++i)
        g_keycodes[i] = KB_KEY_UNKNOWN;

    // Valid key code range is  [8,255], according to the Xlib manual
    for (i=8; i<=255; ++i) {
        // Try secondary keysym, for numeric keypad keys
         keySym  = XkbKeycodeToKeysym(window_data_specific->display, i, 0, 1);
         g_keycodes[i] = translateKeyCodeB(keySym);
         if (g_keycodes[i] == KB_KEY_UNKNOWN) {
            keySym = XkbKeycodeToKeysym(window_data_specific->display, i, 0, 0);
            g_keycodes[i] = translateKeyCodeA(keySym);
         }
    }
}

//-------------------------------------
int
translate_key(int scancode) {
    if (scancode < 0 || scancode > 255)
        return KB_KEY_UNKNOWN;

    return g_keycodes[scancode];
}

//-------------------------------------
int
translate_mod(int state) {
    int mod_keys = 0;

    if (state & ShiftMask)
        mod_keys |= KB_MOD_SHIFT;
    if (state & ControlMask)
        mod_keys |= KB_MOD_CONTROL;
    if (state & Mod1Mask)
        mod_keys |= KB_MOD_ALT;
    if (state & Mod4Mask)
        mod_keys |= KB_MOD_SUPER;
    if (state & LockMask)
        mod_keys |= KB_MOD_CAPS_LOCK;
    if (state & Mod2Mask)
        mod_keys |= KB_MOD_NUM_LOCK;

    return mod_keys;
}

//-------------------------------------
int
translate_mod_ex(int key, int state, int is_pressed) {
    int mod_keys = 0;

    mod_keys = translate_mod(state);

    switch (key)
    {
        case KB_KEY_LEFT_SHIFT:
        case KB_KEY_RIGHT_SHIFT:
            if (is_pressed)
                mod_keys |= KB_MOD_SHIFT;
            else
                mod_keys &= ~KB_MOD_SHIFT;
            break;

        case KB_KEY_LEFT_CONTROL:
        case KB_KEY_RIGHT_CONTROL:
            if (is_pressed)
                mod_keys |= KB_MOD_CONTROL;
            else
                mod_keys &= ~KB_MOD_CONTROL;
            break;

        case KB_KEY_LEFT_ALT:
        case KB_KEY_RIGHT_ALT:
            if (is_pressed)
                mod_keys |= KB_MOD_ALT;
            else
                mod_keys &= ~KB_MOD_ALT;
            break;

        case KB_KEY_LEFT_SUPER:
        case KB_KEY_RIGHT_SUPER:
            if (is_pressed)
                mod_keys |= KB_MOD_SUPER;
            else
                mod_keys &= ~KB_MOD_SUPER;
            break;
    }

    return mod_keys;
}

//-------------------------------------
bool
mfb_set_viewport(struct mfb_window *window, unsigned offset_x, unsigned offset_y, unsigned width, unsigned height)  {
    SWindowData *window_data = (SWindowData *) window;

    if (window_data == NULL) {
        mfb_log(MFB_LOG_ERROR, "X11MiniFB error: mfb_set_viewport called with a null window pointer.");
        return false;
    }

    if (offset_x + width > window_data->window_width) {
        mfb_log(MFB_LOG_ERROR, "X11MiniFB error: viewport exceeds window width (offset_x=%u, width=%u, window_width=%u).",
                offset_x, width, window_data->window_width);
        return false;
    }
    if (offset_y + height > window_data->window_height) {
        mfb_log(MFB_LOG_ERROR, "X11MiniFB error: viewport exceeds window height (offset_y=%u, height=%u, window_height=%u).",
                offset_y, height, window_data->window_height);
        return false;
    }

    window_data->dst_offset_x = offset_x;
    window_data->dst_offset_y = offset_y;
    window_data->dst_width    = width;
    window_data->dst_height   = height;
    calc_dst_factor(window_data, window_data->window_width, window_data->window_height);

    return true;
}

//-------------------------------------
void
mfb_get_monitor_scale(struct mfb_window *window, float *scale_x, float *scale_y) {
    float x = 96.0, y = 96.0;

    if (window != NULL) {
        //SWindowData     *window_data     = (SWindowData *) window;
        //SWindowData_X11 *window_data_specific = (SWindowData_X11 *) window_data->specific;

        // I cannot find a way to get dpi under VirtualBox
        // XrmGetResource "Xft.dpi", "Xft.Dpi"
        // XRRGetOutputInfo
        // DisplayWidthMM, DisplayHeightMM
        // All returning invalid values or 0
    }

    if (scale_x) {
        *scale_x = x / 96.0f;
        if (*scale_x == 0) {
            *scale_x = 1.0f;
        }
    }

    if (scale_y) {
        *scale_y = y / 96.0f;
        if (*scale_y == 0) {
            *scale_y = 1.0f;
        }
    }
}

//-------------------------------------
Cursor
create_blank_cursor(Display *display, Window window) {
    static char data[1] = {0};
    Pixmap pixmap = XCreateBitmapFromData(display, window, data, 1, 1);

    XColor dummy;
    Cursor invis_cursor =  XCreatePixmapCursor(display, pixmap, pixmap, &dummy, &dummy, 0, 0);
    XFreePixmap(display, pixmap);

    return invis_cursor;
}

//-------------------------------------
void
mfb_show_cursor(struct mfb_window *window, bool show) {
    SWindowData *window_data = (SWindowData *) window;
    if (window_data == NULL) {
        mfb_log(MFB_LOG_ERROR, "X11MiniFB error: mfb_show_cursor called with a null window pointer.");
        return;
    }
    SWindowData_X11 *window_data_specific = (SWindowData_X11 *) window_data->specific;
    if (window_data_specific == NULL) {
        mfb_log(MFB_LOG_ERROR, "X11MiniFB error: mfb_show_cursor missing X11-specific window data.");
        return;
    }
    if (window_data_specific->display == NULL) {
        mfb_log(MFB_LOG_ERROR, "X11MiniFB error: mfb_show_cursor has a null X11 display handle.");
        return;
    }

    if (window_data->is_cursor_visible == show)
        return;

    window_data->is_cursor_visible = show;

    // stupid. very stupid. really stupid.
    // could be way better if i wasn't too stubborn to use xfixes

    if (show) {
        XDefineCursor(window_data_specific->display, window_data_specific->window, None);
    }
    else {
        XDefineCursor(window_data_specific->display, window_data_specific->window, window_data_specific->invis_cursor);
    }
}
