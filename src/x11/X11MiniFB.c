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
#include <unistd.h>
#include <MiniFB.h>
#include <MiniFB_internal.h>
#include "WindowData.h"
#include "WindowData_X11.h"

#if defined(USE_OPENGL_API)
    #include <gl/MiniFB_GL.h>
#endif

static Atom s_delete_window_atom;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void init_keycodes(SWindowData_X11 *window_data_x11);

extern void
stretch_image(uint32_t *srcImage, uint32_t srcX, uint32_t srcY, uint32_t srcWidth, uint32_t srcHeight, uint32_t srcPitch,
              uint32_t *dstImage, uint32_t dstX, uint32_t dstY, uint32_t dstWidth, uint32_t dstHeight, uint32_t dstPitch);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct mfb_window *
mfb_open_ex(const char *title, unsigned width, unsigned height, unsigned flags) {
    int depth, i, formatCount, convDepth = -1;
    XPixmapFormatValues* formats;
    XSetWindowAttributes windowAttributes;
    XSizeHints sizeHints;
    Visual* visual;

    SWindowData *window_data = (SWindowData *) malloc(sizeof(SWindowData));
    if (!window_data) {
        return 0x0;
    }
    memset(window_data, 0, sizeof(SWindowData));

    SWindowData_X11 *window_data_x11 = (SWindowData_X11 *) malloc(sizeof(SWindowData_X11));
    if (!window_data_x11) {
        free(window_data);
        return 0x0;
    }
    memset(window_data_x11, 0, sizeof(SWindowData_X11));
    window_data->specific = window_data_x11;

    window_data_x11->display = XOpenDisplay(0);
    if (!window_data_x11->display) {
        free(window_data);
        free(window_data_x11);
        return 0x0;
    }

    init_keycodes(window_data_x11);

    window_data_x11->screen = DefaultScreen(window_data_x11->display);

    visual   = DefaultVisual(window_data_x11->display, window_data_x11->screen);
    formats  = XListPixmapFormats(window_data_x11->display, &formatCount);
    depth    = DefaultDepth(window_data_x11->display, window_data_x11->screen);

    Window defaultRootWindow = DefaultRootWindow(window_data_x11->display);

    for (i = 0; i < formatCount; ++i)
    {
        if (depth == formats[i].depth)
        {
            convDepth = formats[i].bits_per_pixel;
            break;
        }
    }

    XFree(formats);

    // We only support 32-bit right now
    if (convDepth != 32)
    {
        XCloseDisplay(window_data_x11->display);
        return 0x0;
    }

    int screenWidth  = DisplayWidth(window_data_x11->display, window_data_x11->screen);
    int screenHeight = DisplayHeight(window_data_x11->display, window_data_x11->screen);

    windowAttributes.border_pixel     = BlackPixel(window_data_x11->display, window_data_x11->screen);
    windowAttributes.background_pixel = BlackPixel(window_data_x11->display, window_data_x11->screen);
    windowAttributes.backing_store    = NotUseful;

    int posX, posY;
    int windowWidth, windowHeight;

    window_data->window_width  = width;
    window_data->window_height = height;
    window_data->buffer_width  = width;
    window_data->buffer_height = height;
    window_data->buffer_stride = width * 4;
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

    window_data_x11->window = XCreateWindow(
                    window_data_x11->display,
                    defaultRootWindow,
                    posX, posY,
                    windowWidth, windowHeight,
                    0,
                    depth,
                    InputOutput,
                    visual,
                    CWBackPixel | CWBorderPixel | CWBackingStore,
                    &windowAttributes);
    if (!window_data_x11->window)
        return 0x0;

    XSelectInput(window_data_x11->display, window_data_x11->window,
        KeyPressMask | KeyReleaseMask
        | ButtonPressMask | ButtonReleaseMask | PointerMotionMask
        | StructureNotifyMask | ExposureMask
        | FocusChangeMask
        | EnterWindowMask | LeaveWindowMask
    );

    XStoreName(window_data_x11->display, window_data_x11->window, title);

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
        Atom sh_p = XInternAtom(window_data_x11->display, "_MOTIF_WM_HINTS", True);
        XChangeProperty(window_data_x11->display, window_data_x11->window, sh_p, sh_p, 32, PropModeReplace, (unsigned char*)&sh, 5);
    }

    if (flags & WF_ALWAYS_ON_TOP) {
        Atom sa_p = XInternAtom(window_data_x11->display, "_NET_WM_STATE_ABOVE", False);
        XChangeProperty(window_data_x11->display, window_data_x11->window, XInternAtom(window_data_x11->display, "_NET_WM_STATE", False), XA_ATOM, 32, PropModeReplace, (unsigned char *)&sa_p, 1);
    }

    if (flags & WF_FULLSCREEN) {
        Atom sf_p = XInternAtom(window_data_x11->display, "_NET_WM_STATE_FULLSCREEN", True);
        XChangeProperty(window_data_x11->display, window_data_x11->window, XInternAtom(window_data_x11->display, "_NET_WM_STATE", True), XA_ATOM, 32, PropModeReplace, (unsigned char*)&sf_p, 1);
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

    s_delete_window_atom = XInternAtom(window_data_x11->display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(window_data_x11->display, window_data_x11->window, &s_delete_window_atom, 1);

#if defined(USE_OPENGL_API)
    if(create_GL_context(window_data) == false) {
        return 0x0;
    }

#else
    window_data_x11->image = XCreateImage(window_data_x11->display, CopyFromParent, depth, ZPixmap, 0, 0x0, width, height, 32, width * 4);
#endif

    XSetWMNormalHints(window_data_x11->display, window_data_x11->window, &sizeHints);
    XClearWindow(window_data_x11->display, window_data_x11->window);
    XMapRaised(window_data_x11->display, window_data_x11->window);
    XFlush(window_data_x11->display);

    window_data_x11->gc = DefaultGC(window_data_x11->display, window_data_x11->screen);

    window_data_x11->timer = mfb_timer_create();

    mfb_set_keyboard_callback((struct mfb_window *) window_data, keyboard_default);

#if defined(_DEBUG) || defined(DEBUG)
    printf("Window created using X11 API\n");
#endif

    window_data->is_initialized = true;
    return (struct mfb_window *) window_data;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int translate_key(int scancode);
int translate_mod(int state);
int translate_mod_ex(int key, int state, int is_pressed);

static void
processEvent(SWindowData *window_data, XEvent *event) {
    switch (event->type) {
        case KeyPress:
        case KeyRelease:
        {
            SWindowData_X11 *window_data_x11 = (SWindowData_X11 *) window_data->specific;
            if ((event->type == KeyRelease) &&
                XEventsQueued(window_data_x11->display, QueuedAfterReading)) {
                XEvent nev;
                XPeekEvent(window_data_x11->display, &nev);

                if ((nev.type == KeyPress) && (nev.xkey.time == event->xkey.time) &&
                    (nev.xkey.keycode == event->xkey.keycode)) {
                    /* Key wasn’t actually released */
                    return;
                }
            }

            mfb_key key_code      = (mfb_key) translate_key(event->xkey.keycode);
            int is_pressed        = (event->type == KeyPress);
            window_data->mod_keys = translate_mod_ex(key_code, event->xkey.state, is_pressed);

            window_data->key_status[key_code] = is_pressed;
            kCall(keyboard_func, key_code, (mfb_key_mod) window_data->mod_keys, is_pressed);

            if(event->type == KeyPress) {
                KeySym keysym;
                XLookupString(&event->xkey, NULL, 0, &keysym, NULL);

                if ((keysym >= 0x0020 && keysym <= 0x007e) ||
                    (keysym >= 0x00a0 && keysym <= 0x00ff)) {
                    kCall(char_input_func, keysym);
                }
                else if ((keysym & 0xff000000) == 0x01000000) {
                    keysym = keysym & 0x00ffffff;
                }

                kCall(char_input_func, keysym);

                // TODO: Investigate a bit more the xkbcommon api

                // This does not seem to be working properly
                //unsigned int codepoint = xkb_state_key_get_utf32(state, keysym);
                //if (codepoint != 0)
                //    kCall(char_input_func, codepoint);
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
            SWindowData_X11 *window_data_x11 = (SWindowData_X11 *) window_data->specific;
            if(window_data_x11->image_scaler != 0x0) {
                window_data_x11->image_scaler->data = 0x0;
                XDestroyImage(window_data_x11->image_scaler);
                window_data_x11->image_scaler        = 0x0;
                window_data_x11->image_scaler_width  = 0;
                window_data_x11->image_scaler_height = 0;
            }
            XClearWindow(window_data_x11->display, window_data_x11->window);
#endif
            kCall(resize_func, window_data->window_width, window_data->window_height);
        }
        break;

        case EnterNotify:
        case LeaveNotify:
        break;

        case FocusIn:
            window_data->is_active = true;
            kCall(active_func, true);
            break;

        case FocusOut:
            window_data->is_active = false;
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

static void
processEvents(SWindowData *window_data) {
    XEvent          event;
    SWindowData_X11 *window_data_x11 = (SWindowData_X11 *) window_data->specific;

    while ((window_data->close == false) && XPending(window_data_x11->display)) {
        XNextEvent(window_data_x11->display, &event);
        processEvent(window_data, &event);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void destroy_window_data(SWindowData *window_data);

mfb_update_state
mfb_update_ex(struct mfb_window *window, void *buffer, unsigned width, unsigned height) {
    if (window == 0x0) {
        return STATE_INVALID_WINDOW;
    }

    SWindowData *window_data = (SWindowData *) window;
    if (window_data->close) {
        destroy_window_data(window_data);
        return STATE_EXIT;
    }

    if (buffer == 0x0) {
        return STATE_INVALID_BUFFER;
    }

#if !defined(USE_OPENGL_API)
    SWindowData_X11 *window_data_x11 = (SWindowData_X11 *) window_data->specific;
    bool different_size = false;
#endif

    if(window_data->buffer_width != width || window_data->buffer_height != height) {
        window_data->buffer_width  = width;
        window_data->buffer_stride = width * 4;
        window_data->buffer_height = height;
#if !defined(USE_OPENGL_API)
        different_size = true;
#endif
    }

#if !defined(USE_OPENGL_API)

    if (different_size || window_data->buffer_width != window_data->dst_width || window_data->buffer_height != window_data->dst_height) {
        if (window_data_x11->image_scaler_width != window_data->dst_width || window_data_x11->image_scaler_height != window_data->dst_height) {
            if (window_data_x11->image_scaler != 0x0) {
                window_data_x11->image_scaler->data = 0x0;
                XDestroyImage(window_data_x11->image_scaler);
            }
            if (window_data_x11->image_buffer != 0x0) {
                free(window_data_x11->image_buffer);
                window_data_x11->image_buffer = 0x0;
            }
            int depth = DefaultDepth(window_data_x11->display, window_data_x11->screen);
            window_data_x11->image_buffer = malloc(window_data->dst_width * window_data->dst_height * 4);
            if(window_data_x11->image_buffer == 0x0) {
                return STATE_INTERNAL_ERROR;
            }
            window_data_x11->image_scaler_width  = window_data->dst_width;
            window_data_x11->image_scaler_height = window_data->dst_height;
            window_data_x11->image_scaler = XCreateImage(window_data_x11->display, CopyFromParent, depth, ZPixmap, 0, 0x0, window_data_x11->image_scaler_width, window_data_x11->image_scaler_height, 32, window_data_x11->image_scaler_width * 4);
        }
    }

    if (window_data_x11->image_scaler != 0x0) {
        stretch_image((uint32_t *) buffer, 0, 0, window_data->buffer_width, window_data->buffer_height, window_data->buffer_width,
                      (uint32_t *) window_data_x11->image_buffer, 0, 0, window_data->dst_width, window_data->dst_height, window_data->dst_width);
        window_data_x11->image_scaler->data = (char *) window_data_x11->image_buffer;
        XPutImage(window_data_x11->display, window_data_x11->window, window_data_x11->gc, window_data_x11->image_scaler, 0, 0, window_data->dst_offset_x, window_data->dst_offset_y, window_data->dst_width, window_data->dst_height);
    }
    else {
        window_data_x11->image->data = (char *) buffer;
        XPutImage(window_data_x11->display, window_data_x11->window, window_data_x11->gc, window_data_x11->image, 0, 0, window_data->dst_offset_x, window_data->dst_offset_y, window_data->dst_width, window_data->dst_height);
    }
    XFlush(window_data_x11->display);

#else

    redraw_GL(window_data, buffer);

#endif

    processEvents(window_data);

    return STATE_OK;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

mfb_update_state
mfb_update_events(struct mfb_window *window) {
    if (window == 0x0) {
        return STATE_INVALID_WINDOW;
    }

    SWindowData *window_data = (SWindowData *) window;
    if (window_data->close) {
        destroy_window_data(window_data);
        return STATE_EXIT;
    }

    SWindowData_X11 *window_data_x11 = (SWindowData_X11 *) window_data->specific;
    XFlush(window_data_x11->display);
    processEvents(window_data);

    return STATE_OK;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extern double   g_time_for_frame;
extern bool     g_use_hardware_sync;

bool
mfb_wait_sync(struct mfb_window *window) {
    if (window == 0x0) {
        return false;
    }

    SWindowData *window_data = (SWindowData *) window;
    if (window_data->close) {
        destroy_window_data(window_data);
        return false;
    }

    if(g_use_hardware_sync) {
        return true;
    }

    SWindowData_X11 *window_data_x11 = (SWindowData_X11 *) window_data->specific;
    XFlush(window_data_x11->display);
    XEvent      event;
    double      current;
    uint32_t    millis = 1;
    while(1) {
        current = mfb_timer_now(window_data_x11->timer);
        if (current >= g_time_for_frame * 0.96) {
            mfb_timer_reset(window_data_x11->timer);
            return true;
        }
        else if(current >= g_time_for_frame * 0.8) {
            millis = 0;
        }

        usleep(millis * 1000);
        //sched_yield();

        if(millis == 1 && XEventsQueued(window_data_x11->display, QueuedAlready) > 0) {
            XNextEvent(window_data_x11->display, &event);
            processEvent(window_data, &event);

            if(window_data->close) {
                destroy_window_data(window_data);
                return false;
            }
        }
    }

    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void
destroy_window_data(SWindowData *window_data)  {
    if (window_data != 0x0) {
        if (window_data->specific != 0x0) {
            SWindowData_X11   *window_data_x11 = (SWindowData_X11 *) window_data->specific;

#if defined(USE_OPENGL_API)
            destroy_GL_context(window_data);
#else
            if (window_data_x11->image != 0x0) {
                window_data_x11->image->data = 0x0;
                XDestroyImage(window_data_x11->image);
                XDestroyWindow(window_data_x11->display, window_data_x11->window);
                XCloseDisplay(window_data_x11->display);
            }
#endif

            mfb_timer_destroy(window_data_x11->timer);
            memset(window_data_x11, 0, sizeof(SWindowData_X11));
            free(window_data_x11);
        }
        memset(window_data, 0, sizeof(SWindowData));
        free(window_data);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extern short int g_keycodes[512];

static int
translateKeyCodeB(int keySym) {

    switch (keySym)
    {
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

static int translateKeyCodeA(int keySym) {
    switch (keySym)
    {
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
        case XK_grave:          return KB_KEY_GRAVE_ACCENT;
        case XK_comma:          return KB_KEY_COMMA;
        case XK_period:         return KB_KEY_PERIOD;
        case XK_slash:          return KB_KEY_SLASH;
        case XK_less:           return KB_KEY_WORLD_1; // At least in some layouts...
        default:                break;
    }

    return KB_KEY_UNKNOWN;
}

void
init_keycodes(SWindowData_X11 *window_data_x11) {
    size_t  i;
    int     keySym;

    // Clear keys
    for (i = 0; i < sizeof(g_keycodes) / sizeof(g_keycodes[0]); ++i)
        g_keycodes[i] = KB_KEY_UNKNOWN;

    // Valid key code range is  [8,255], according to the Xlib manual
    for (i=8; i<=255; ++i) {
        // Try secondary keysym, for numeric keypad keys
         keySym  = XkbKeycodeToKeysym(window_data_x11->display, i, 0, 1);
         g_keycodes[i] = translateKeyCodeB(keySym);
         if (g_keycodes[i] == KB_KEY_UNKNOWN) {
            keySym = XkbKeycodeToKeysym(window_data_x11->display, i, 0, 0);
            g_keycodes[i] = translateKeyCodeA(keySym);
         }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int
translate_key(int scancode) {
    if (scancode < 0 || scancode > 255)
        return KB_KEY_UNKNOWN;

    return g_keycodes[scancode];
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool
mfb_set_viewport(struct mfb_window *window, unsigned offset_x, unsigned offset_y, unsigned width, unsigned height)  {
    SWindowData *window_data = (SWindowData *) window;

    if (offset_x + width > window_data->window_width) {
        return false;
    }
    if (offset_y + height > window_data->window_height) {
        return false;
    }

    window_data->dst_offset_x = offset_x;
    window_data->dst_offset_y = offset_y;
    window_data->dst_width    = width;
    window_data->dst_height   = height;
    calc_dst_factor(window_data, window_data->window_width, window_data->window_height);

    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void
mfb_get_monitor_scale(struct mfb_window *window, float *scale_x, float *scale_y) {
    float x = 96.0, y = 96.0;

    if(window != 0x0) {
        //SWindowData     *window_data     = (SWindowData *) window;
        //SWindowData_X11 *window_data_x11 = (SWindowData_X11 *) window_data->specific;

        // I cannot find a way to get dpi under VirtualBox
        // XrmGetResource "Xft.dpi", "Xft.Dpi"
        // XRRGetOutputInfo
        // DisplayWidthMM, DisplayHeightMM
        // All returning invalid values or 0
    }

    if (scale_x) {
        *scale_x = x / 96.0f;
        if(*scale_x == 0) {
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
