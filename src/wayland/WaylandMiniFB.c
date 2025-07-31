#include <MiniFB.h>
#include "MiniFB_internal.h"
#include "MiniFB_enums.h"
#include "WindowData.h"
#include "WindowData_Way.h"

#include <wayland-client.h>
#include <wayland-cursor.h>
#include "generated/xdg-shell-client-protocol.h"
#include "generated/fractional-scale.h"

#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <linux/limits.h>
#include <linux/input.h>
#include <linux/input-event-codes.h>
#include <xkbcommon/xkbcommon.h>

#include <sys/mman.h>

static void stub() {}

static void
destroy_window_data(SWindowData *window_data)
{
    if (!window_data)
        return;

    SWindowData_Way *window_data_way = window_data->specific;
    if (window_data_way) {
        mfb_timer_destroy(window_data_way->timer);
        free(window_data_way);
    }
  
    free(window_data);
}

static void
destroy(SWindowData *window_data)
{
    if (!window_data)
        return;

    SWindowData_Way *window_data_way = window_data->specific;
    if (!window_data_way)
        goto skip_specific;

#define KILL(NAME)                                      \
    do                                                  \
    {                                                   \
        if (window_data_way->NAME)                      \
            wl_##NAME##_destroy(window_data_way->NAME); \
    } while (0);                                        \
    window_data_way->NAME = NULL;

    KILL(cursor_theme);
    xdg_surface_destroy(window_data_way->shell_surface);
    xdg_wm_base_destroy(window_data_way->shell);
    KILL(surface);
    wp_fractional_scale_v1_destroy(window_data_way->fractional_scale);
    if (window_data->draw_buffer) {
        wl_buffer_destroy(window_data->draw_buffer);
        window_data->draw_buffer = NULL;
    }
    KILL(shm_pool);
    KILL(shm);
    KILL(compositor);
    KILL(keyboard);
    KILL(seat);
    wp_fractional_scale_manager_v1_destroy(window_data_way->fractional_scale_manager);
    KILL(registry);
#undef KILL
    xkb_state_unref(window_data_way->xkb_state);
    wl_display_disconnect(window_data_way->display);
    close(window_data_way->fd);

skip_specific:
    destroy_window_data(window_data);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static mfb_key
key_from_raw(uint32_t raw)
{
    switch (raw) {
    case KEY_GRAVE:      return KB_KEY_GRAVE_ACCENT;
    case KEY_1:          return KB_KEY_1;
    case KEY_2:          return KB_KEY_2;
    case KEY_3:          return KB_KEY_3;
    case KEY_4:          return KB_KEY_4;
    case KEY_5:          return KB_KEY_5;
    case KEY_6:          return KB_KEY_6;
    case KEY_7:          return KB_KEY_7;
    case KEY_8:          return KB_KEY_8;
    case KEY_9:          return KB_KEY_9;
    case KEY_0:          return KB_KEY_0;
    case KEY_SPACE:      return KB_KEY_SPACE;
    case KEY_MINUS:      return KB_KEY_MINUS;
    case KEY_EQUAL:      return KB_KEY_EQUAL;
    case KEY_Q:          return KB_KEY_Q;
    case KEY_W:          return KB_KEY_W;
    case KEY_E:          return KB_KEY_E;
    case KEY_R:          return KB_KEY_R;
    case KEY_T:          return KB_KEY_T;
    case KEY_Y:          return KB_KEY_Y;
    case KEY_U:          return KB_KEY_U;
    case KEY_I:          return KB_KEY_I;
    case KEY_O:          return KB_KEY_O;
    case KEY_P:          return KB_KEY_P;
    case KEY_LEFTBRACE:  return KB_KEY_LEFT_BRACKET;
    case KEY_RIGHTBRACE: return KB_KEY_RIGHT_BRACKET;
    case KEY_A:          return KB_KEY_A;
    case KEY_S:          return KB_KEY_S;
    case KEY_D:          return KB_KEY_D;
    case KEY_F:          return KB_KEY_F;
    case KEY_G:          return KB_KEY_G;
    case KEY_H:          return KB_KEY_H;
    case KEY_J:          return KB_KEY_J;
    case KEY_K:          return KB_KEY_K;
    case KEY_L:          return KB_KEY_L;
    case KEY_SEMICOLON:  return KB_KEY_SEMICOLON;
    case KEY_APOSTROPHE: return KB_KEY_APOSTROPHE;
    case KEY_Z:          return KB_KEY_Z;
    case KEY_X:          return KB_KEY_X;
    case KEY_C:          return KB_KEY_C;
    case KEY_V:          return KB_KEY_V;
    case KEY_B:          return KB_KEY_B;
    case KEY_N:          return KB_KEY_N;
    case KEY_M:          return KB_KEY_M;
    case KEY_COMMA:      return KB_KEY_COMMA;
    case KEY_DOT:        return KB_KEY_PERIOD;
    case KEY_SLASH:      return KB_KEY_SLASH;
    case KEY_BACKSLASH:  return KB_KEY_BACKSLASH;
    case KEY_ESC:        return KB_KEY_ESCAPE;
    case KEY_TAB:        return KB_KEY_TAB;
    case KEY_LEFTSHIFT:  return KB_KEY_LEFT_SHIFT;
    case KEY_RIGHTSHIFT: return KB_KEY_RIGHT_SHIFT;
    case KEY_LEFTCTRL:   return KB_KEY_LEFT_CONTROL;
    case KEY_RIGHTCTRL:  return KB_KEY_RIGHT_CONTROL;
    case KEY_LEFTALT:    return KB_KEY_LEFT_ALT;
    case KEY_RIGHTALT:   return KB_KEY_RIGHT_ALT;
    case KEY_LEFTMETA:   return KB_KEY_LEFT_SUPER;
    case KEY_RIGHTMETA:  return KB_KEY_RIGHT_SUPER;
    case KEY_MENU:       return KB_KEY_MENU;
    case KEY_NUMLOCK:    return KB_KEY_NUM_LOCK;
    case KEY_CAPSLOCK:   return KB_KEY_CAPS_LOCK;
    case KEY_PRINT:      return KB_KEY_PRINT_SCREEN;
    case KEY_SCROLLLOCK: return KB_KEY_SCROLL_LOCK;
    case KEY_PAUSE:      return KB_KEY_PAUSE;
    case KEY_DELETE:     return KB_KEY_DELETE;
    case KEY_BACKSPACE:  return KB_KEY_BACKSPACE;
    case KEY_ENTER:      return KB_KEY_ENTER;
    case KEY_HOME:       return KB_KEY_HOME;
    case KEY_END:        return KB_KEY_END;
    case KEY_PAGEUP:     return KB_KEY_PAGE_UP;
    case KEY_PAGEDOWN:   return KB_KEY_PAGE_DOWN;
    case KEY_INSERT:     return KB_KEY_INSERT;
    case KEY_LEFT:       return KB_KEY_LEFT;
    case KEY_RIGHT:      return KB_KEY_RIGHT;
    case KEY_DOWN:       return KB_KEY_DOWN;
    case KEY_UP:         return KB_KEY_UP;
    case KEY_F1:         return KB_KEY_F1;
    case KEY_F2:         return KB_KEY_F2;
    case KEY_F3:         return KB_KEY_F3;
    case KEY_F4:         return KB_KEY_F4;
    case KEY_F5:         return KB_KEY_F5;
    case KEY_F6:         return KB_KEY_F6;
    case KEY_F7:         return KB_KEY_F7;
    case KEY_F8:         return KB_KEY_F8;
    case KEY_F9:         return KB_KEY_F9;
    case KEY_F10:        return KB_KEY_F10;
    case KEY_F11:        return KB_KEY_F11;
    case KEY_F12:        return KB_KEY_F12;
    case KEY_F13:        return KB_KEY_F13;
    case KEY_F14:        return KB_KEY_F14;
    case KEY_F15:        return KB_KEY_F15;
    case KEY_F16:        return KB_KEY_F16;
    case KEY_F17:        return KB_KEY_F17;
    case KEY_F18:        return KB_KEY_F18;
    case KEY_F19:        return KB_KEY_F19;
    case KEY_F20:        return KB_KEY_F20;
    case KEY_F21:        return KB_KEY_F21;
    case KEY_F22:        return KB_KEY_F22;
    case KEY_F23:        return KB_KEY_F23;
    case KEY_F24:        return KB_KEY_F24;
    case KEY_KPSLASH:    return KB_KEY_KP_DIVIDE;
    case KEY_KPDOT:      return KB_KEY_KP_MULTIPLY;
    case KEY_KPMINUS:    return KB_KEY_KP_SUBTRACT;
    case KEY_KPPLUS:     return KB_KEY_KP_ADD;
    case KEY_KP0:        return KB_KEY_KP_0;
    case KEY_KP1:        return KB_KEY_KP_1;
    case KEY_KP2:        return KB_KEY_KP_2;
    case KEY_KP3:        return KB_KEY_KP_3;
    case KEY_KP4:        return KB_KEY_KP_4;
    case KEY_KP5:        return KB_KEY_KP_5;
    case KEY_KP6:        return KB_KEY_KP_6;
    case KEY_KP7:        return KB_KEY_KP_7;
    case KEY_KP8:        return KB_KEY_KP_8;
    case KEY_KP9:        return KB_KEY_KP_9;
    case KEY_KPCOMMA:    return KB_KEY_KP_DECIMAL;
    case KEY_KPEQUAL:    return KB_KEY_KP_EQUAL;
    case KEY_KPENTER:    return KB_KEY_KP_ENTER;
    }

    return KB_KEY_UNKNOWN;
}

mfb_key
key_from_xkb_sym(uint32_t sym)
{
    switch (sym) {
    case XKB_KEY_grave:             return KB_KEY_GRAVE_ACCENT;
    case XKB_KEY_1:                 return KB_KEY_1;
    case XKB_KEY_2:                 return KB_KEY_2;
    case XKB_KEY_3:                 return KB_KEY_3;
    case XKB_KEY_4:                 return KB_KEY_4;
    case XKB_KEY_5:                 return KB_KEY_5;
    case XKB_KEY_6:                 return KB_KEY_6;
    case XKB_KEY_7:                 return KB_KEY_7;
    case XKB_KEY_8:                 return KB_KEY_8;
    case XKB_KEY_9:                 return KB_KEY_9;
    case XKB_KEY_0:                 return KB_KEY_0;
    case XKB_KEY_space:             return KB_KEY_SPACE;
    case XKB_KEY_minus:             return KB_KEY_MINUS;
    case XKB_KEY_equal:             return KB_KEY_EQUAL;
    case XKB_KEY_q: case XKB_KEY_Q: return KB_KEY_Q;
    case XKB_KEY_w: case XKB_KEY_W: return KB_KEY_W;
    case XKB_KEY_e: case XKB_KEY_E: return KB_KEY_E;
    case XKB_KEY_r: case XKB_KEY_R: return KB_KEY_R;
    case XKB_KEY_t: case XKB_KEY_T: return KB_KEY_T;
    case XKB_KEY_y: case XKB_KEY_Y: return KB_KEY_Y;
    case XKB_KEY_u: case XKB_KEY_U: return KB_KEY_U;
    case XKB_KEY_i: case XKB_KEY_I: return KB_KEY_I;
    case XKB_KEY_o: case XKB_KEY_O: return KB_KEY_O;
    case XKB_KEY_p: case XKB_KEY_P: return KB_KEY_P;
    case XKB_KEY_braceleft:         return KB_KEY_LEFT_BRACKET;
    case XKB_KEY_braceright:        return KB_KEY_RIGHT_BRACKET;
    case XKB_KEY_a: case XKB_KEY_A: return KB_KEY_A;
    case XKB_KEY_s: case XKB_KEY_S: return KB_KEY_S;
    case XKB_KEY_d: case XKB_KEY_D: return KB_KEY_D;
    case XKB_KEY_f: case XKB_KEY_F: return KB_KEY_F;
    case XKB_KEY_g: case XKB_KEY_G: return KB_KEY_G;
    case XKB_KEY_h: case XKB_KEY_H: return KB_KEY_H;
    case XKB_KEY_j: case XKB_KEY_J: return KB_KEY_J;
    case XKB_KEY_k: case XKB_KEY_K: return KB_KEY_K;
    case XKB_KEY_l: case XKB_KEY_L: return KB_KEY_L;
    case XKB_KEY_semicolon:         return KB_KEY_SEMICOLON;
    case XKB_KEY_apostrophe:        return KB_KEY_APOSTROPHE;
    case XKB_KEY_z: case XKB_KEY_Z: return KB_KEY_Z;
    case XKB_KEY_x: case XKB_KEY_X: return KB_KEY_X;
    case XKB_KEY_c: case XKB_KEY_C: return KB_KEY_C;
    case XKB_KEY_v: case XKB_KEY_V: return KB_KEY_V;
    case XKB_KEY_b: case XKB_KEY_B: return KB_KEY_B;
    case XKB_KEY_n: case XKB_KEY_N: return KB_KEY_N;
    case XKB_KEY_m: case XKB_KEY_M: return KB_KEY_M;
    case XKB_KEY_comma:             return KB_KEY_COMMA;
    case XKB_KEY_period:            return KB_KEY_PERIOD;
    case XKB_KEY_slash:             return KB_KEY_SLASH;
    case XKB_KEY_backslash:         return KB_KEY_BACKSLASH;
    case XKB_KEY_Escape:            return KB_KEY_ESCAPE;
    case XKB_KEY_Tab:               return KB_KEY_TAB;
    case XKB_KEY_Shift_L:           return KB_KEY_LEFT_SHIFT;
    case XKB_KEY_Shift_R:           return KB_KEY_RIGHT_SHIFT;
    case XKB_KEY_Control_L:         return KB_KEY_LEFT_CONTROL;
    case XKB_KEY_Control_R:         return KB_KEY_RIGHT_CONTROL;
    case XKB_KEY_Alt_L:             return KB_KEY_LEFT_ALT;
    case XKB_KEY_Alt_R:             return KB_KEY_RIGHT_ALT;
    case XKB_KEY_Super_L:           return KB_KEY_LEFT_SUPER;
    case XKB_KEY_Super_R:           return KB_KEY_RIGHT_SUPER;
    case XKB_KEY_Menu:              return KB_KEY_MENU;
    case XKB_KEY_Num_Lock:          return KB_KEY_NUM_LOCK;
    case XKB_KEY_Caps_Lock:         return KB_KEY_CAPS_LOCK;
    case XKB_KEY_Print:             return KB_KEY_PRINT_SCREEN;
    case XKB_KEY_Scroll_Lock:       return KB_KEY_SCROLL_LOCK;
    case XKB_KEY_Pause:             return KB_KEY_PAUSE;
    case XKB_KEY_Delete:            return KB_KEY_DELETE;
    case XKB_KEY_BackSpace:         return KB_KEY_BACKSPACE;
    case XKB_KEY_Return:            return KB_KEY_ENTER;
    case XKB_KEY_Home:              return KB_KEY_HOME;
    case XKB_KEY_End:               return KB_KEY_END;
    case XKB_KEY_Page_Up:           return KB_KEY_PAGE_UP;
    case XKB_KEY_Page_Down:         return KB_KEY_PAGE_DOWN;
    case XKB_KEY_Insert:            return KB_KEY_INSERT;
    case XKB_KEY_Left:              return KB_KEY_LEFT;
    case XKB_KEY_Right:             return KB_KEY_RIGHT;
    case XKB_KEY_Down:              return KB_KEY_DOWN;
    case XKB_KEY_Up:                return KB_KEY_UP;
    case XKB_KEY_F1:                return KB_KEY_F1;
    case XKB_KEY_F2:                return KB_KEY_F2;
    case XKB_KEY_F3:                return KB_KEY_F3;
    case XKB_KEY_F4:                return KB_KEY_F4;
    case XKB_KEY_F5:                return KB_KEY_F5;
    case XKB_KEY_F6:                return KB_KEY_F6;
    case XKB_KEY_F7:                return KB_KEY_F7;
    case XKB_KEY_F8:                return KB_KEY_F8;
    case XKB_KEY_F9:                return KB_KEY_F9;
    case XKB_KEY_F10:               return KB_KEY_F10;
    case XKB_KEY_F11:               return KB_KEY_F11;
    case XKB_KEY_F12:               return KB_KEY_F12;
    case XKB_KEY_F13:               return KB_KEY_F13;
    case XKB_KEY_F14:               return KB_KEY_F14;
    case XKB_KEY_F15:               return KB_KEY_F15;
    case XKB_KEY_F16:               return KB_KEY_F16;
    case XKB_KEY_F17:               return KB_KEY_F17;
    case XKB_KEY_F18:               return KB_KEY_F18;
    case XKB_KEY_F19:               return KB_KEY_F19;
    case XKB_KEY_F20:               return KB_KEY_F20;
    case XKB_KEY_F21:               return KB_KEY_F21;
    case XKB_KEY_F22:               return KB_KEY_F22;
    case XKB_KEY_F23:               return KB_KEY_F23;
    case XKB_KEY_F24:               return KB_KEY_F24;
    case XKB_KEY_KP_Divide:         return KB_KEY_KP_DIVIDE;
    case XKB_KEY_KP_Multiply:       return KB_KEY_KP_MULTIPLY;
    case XKB_KEY_KP_Subtract:       return KB_KEY_KP_SUBTRACT;
    case XKB_KEY_KP_Add:            return KB_KEY_KP_ADD;
    case XKB_KEY_KP_0:              return KB_KEY_KP_0;
    case XKB_KEY_KP_1:              return KB_KEY_KP_1;
    case XKB_KEY_KP_2:              return KB_KEY_KP_2;
    case XKB_KEY_KP_3:              return KB_KEY_KP_3;
    case XKB_KEY_KP_4:              return KB_KEY_KP_4;
    case XKB_KEY_KP_5:              return KB_KEY_KP_5;
    case XKB_KEY_KP_6:              return KB_KEY_KP_6;
    case XKB_KEY_KP_7:              return KB_KEY_KP_7;
    case XKB_KEY_KP_8:              return KB_KEY_KP_8;
    case XKB_KEY_KP_9:              return KB_KEY_KP_9;
    case XKB_KEY_KP_Decimal:        return KB_KEY_KP_DECIMAL;
    case XKB_KEY_KP_Equal:          return KB_KEY_KP_EQUAL;
    case XKB_KEY_KP_Enter:          return KB_KEY_KP_ENTER;
    }

    return KB_KEY_UNKNOWN;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool
mfb_set_viewport(struct mfb_window *window, unsigned offset_x, unsigned offset_y, unsigned width, unsigned height)
{
    SWindowData *window_data = (SWindowData *) window;

    if (offset_x + width > window_data->window_width)
        return false;
    if (offset_y + height > window_data->window_height)
        return false;

    // TODO: Not yet
    window_data->dst_offset_x = offset_x;
    window_data->dst_offset_y = offset_y;
    window_data->dst_width    = width;
    window_data->dst_height   = height;
    resize_dst(window_data, width, height);

    return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void
handle_ping(void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial)
{
    kUnused(data);

    xdg_wm_base_pong(xdg_wm_base, serial);
}

static struct xdg_wm_base_listener shell_listener = {
    .ping = handle_ping
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// This event provides a file descriptor to the client which can be memory-mapped
// to provide a keyboard mapping description.
// format: keymap format
// fd:     keymap file descriptor
// size:   keymap size, in bytes
static void
keyboard_keymap(void *data, struct wl_keyboard *keyboard, uint32_t format, int fd, uint32_t size)
{
    kUnused(data);
    kUnused(keyboard);
  
    SWindowData     *window_data     = data;
    SWindowData_Way *window_data_way = window_data->specific;

    if (format == 0)
        return;

    struct xkb_context *xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    char *memory = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
  
    struct xkb_keymap *xkb_keymap = xkb_keymap_new_from_string(xkb_context, memory, format, XKB_KEYMAP_COMPILE_NO_FLAGS);
  
    munmap(memory, size);
    xkb_context_unref(xkb_context);
    close(fd);

    window_data_way->xkb_state = xkb_state_new(xkb_keymap);
  
    xkb_keymap_unref(xkb_keymap);
}

// Notification that this seat's keyboard focus is on a certain surface.
// serial:  serial number of the enter event
// surface: surface gaining keyboard focus
// keys:    the currently pressed keys
static void
keyboard_enter(void *data, struct wl_keyboard *keyboard, uint32_t serial, struct wl_surface *surface, struct wl_array *keys)
{
    kUnused(keyboard);
    kUnused(serial);
    kUnused(surface);
    kUnused(keys);

    SWindowData *window_data = data;
    window_data->is_active = true;
    kCall(active_func, true);
}

// The leave notification is sent before the enter notification for the new focus.
// serial:  serial number of the leave event
// surface: surface that lost keyboard focus
static void
keyboard_leave(void *data, struct wl_keyboard *keyboard, uint32_t serial, struct wl_surface *surface)
{
    kUnused(keyboard);
    kUnused(serial);
    kUnused(surface);

    SWindowData *window_data = data;
    window_data->is_active = false;
    kCall(active_func, false);
}

// A key was pressed or released. The time argument is a timestamp with
// millisecond granularity, with an undefined base.
// serial: serial number of the key event
// time:   timestamp with millisecond granularity
// key:    key that produced the event
// state:  physical state of the key
static void
keyboard_key(void *data, struct wl_keyboard *keyboard, uint32_t serial, uint32_t time, uint32_t code, uint32_t state)
{
    kUnused(keyboard);
    kUnused(serial);
    kUnused(time);

    SWindowData     *window_data     = data;
    SWindowData_Way *window_data_way = window_data->specific;

    mfb_key key;
    if (window_data_way->xkb_state)
        key = key_from_xkb_sym(xkb_state_key_get_one_sym(window_data_way->xkb_state, code + 8));
    else
        key = key_from_raw(code);

    bool is_pressed = state != WL_KEYBOARD_KEY_STATE_RELEASED;

    window_data->key_status[key] = is_pressed;
    kCall(keyboard_func, key, (mfb_key_mod) window_data->mod_keys, is_pressed);
}

// Notifies clients that the modifier and/or group state has changed,
// and it should update its local state.
// serial:         serial number of the modifiers event
// mods_depressed: depressed modifiers
// mods_latched:   latched modifiers
// mods_locked:    locked modifiers
// group:          keyboard layout
static void
keyboard_modifiers(void *data, struct wl_keyboard *keyboard, uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group)
{
    kUnused(keyboard);
    kUnused(serial);

    SWindowData      *window_data     = data;
    SWindowData_Way  *window_data_way = window_data->specific;
    struct xkb_state *xkb_state       = window_data_way->xkb_state;

    xkb_state_update_mask(xkb_state, mods_depressed, mods_latched, mods_locked, 0, 0, group);

    window_data->mod_keys = 0;
    if (xkb_state_mod_name_is_active(xkb_state, XKB_MOD_NAME_SHIFT, XKB_STATE_MODS_EFFECTIVE))
        window_data->mod_keys |= KB_MOD_SHIFT;
    if (xkb_state_mod_name_is_active(xkb_state, XKB_MOD_NAME_CTRL, XKB_STATE_MODS_EFFECTIVE))
        window_data->mod_keys |= KB_MOD_CONTROL;
    if (xkb_state_mod_name_is_active(xkb_state, XKB_VMOD_NAME_ALT, XKB_STATE_MODS_EFFECTIVE))
        window_data->mod_keys |= KB_MOD_ALT;
    if (xkb_state_mod_name_is_active(xkb_state, XKB_VMOD_NAME_SUPER, XKB_STATE_MODS_EFFECTIVE))
        window_data->mod_keys |= KB_MOD_SUPER;
    if (xkb_state_mod_name_is_active(xkb_state, XKB_MOD_NAME_CAPS, XKB_STATE_MODS_EFFECTIVE))
        window_data->mod_keys |= KB_MOD_CAPS_LOCK;
    if (xkb_state_mod_name_is_active(xkb_state, XKB_VMOD_NAME_NUM, XKB_STATE_MODS_EFFECTIVE))
        window_data->mod_keys |= KB_MOD_NUM_LOCK;
}

static const struct
wl_keyboard_listener keyboard_listener = {
    .keymap      = keyboard_keymap,
    .enter       = keyboard_enter,
    .leave       = keyboard_leave,
    .key         = keyboard_key,
    .modifiers   = keyboard_modifiers,
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Notification that this seat's pointer is focused on a certain surface.
//
// When a seat's focus enters a surface, the pointer image is
// undefined and a client should respond to this event by setting
// an appropriate pointer image with the set_cursor request.
//
// serial:  serial number of the enter event
// surface: surface entered by the pointer
// sx:      surface-local x coordinate
// sy:      surface-local y coordinate
static void
pointer_enter(void *data, struct wl_pointer *pointer, uint32_t serial, struct wl_surface *surface, wl_fixed_t sx, wl_fixed_t sy)
{
    kUnused(surface);
    kUnused(sx);
    kUnused(sy);
  
    SWindowData     *window_data     = data;
    SWindowData_Way *window_data_way = window_data->specific;

    struct wl_cursor_image *image = window_data_way->default_cursor->images[0];
    struct wl_buffer *buffer = wl_cursor_image_get_buffer(image);

    wl_pointer_set_cursor(pointer, serial, window_data_way->cursor_surface, image->hotspot_x, image->hotspot_y);
    wl_surface_attach(window_data_way->cursor_surface, buffer, 0, 0);
    wl_surface_damage(window_data_way->cursor_surface, 0, 0, image->width, image->height);
    wl_surface_commit(window_data_way->cursor_surface);
}

// Notification of pointer location change.
//
// The arguments sx and sy are the location relative to the focused surface.
//
// time:  timestamp with millisecond granularity
// sx:    surface-local x coordinate
// sy:    surface-local y coordinate
static void
pointer_motion(void *data, struct wl_pointer *pointer, uint32_t time, wl_fixed_t sx, wl_fixed_t sy)
{
    kUnused(pointer);
    kUnused(time);

    SWindowData *window_data = data;
    window_data->mouse_pos_x = wl_fixed_to_int(sx);
    window_data->mouse_pos_y = wl_fixed_to_int(sy);
    kCall(mouse_move_func, window_data->mouse_pos_x, window_data->mouse_pos_y);
}

// Mouse button click and release notifications.
//
// The location of the click is given by the last motion or enter
// event. The time argument is a timestamp with millisecond
// granularity, with an undefined base.
//
// The button is a button code as defined in the Linux kernel's
// linux/input-event-codes.h header file, e.g. BTN_LEFT.
//
// Any 16-bit button code value is reserved for future additions to
// the kernel's event code list. All other button codes above
// 0xFFFF are currently undefined but may be used in future
// versions of this protocol.
//
// serial: serial number of the button event
// time:   timestamp with millisecond granularity
// button: button that produced the event
// state:  physical state of the button
static void
pointer_button(void *data, struct wl_pointer *pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t state)
{
    kUnused(pointer);
    kUnused(serial);
    kUnused(time);

    //printf("Pointer button '%d'(%d)\n", button, state);
    SWindowData *window_data = data;
    window_data->mouse_button_status[(button - BTN_MOUSE + 1) & 0x07] = (state == 1);
    kCall(mouse_btn_func, (mfb_mouse_button) (button - BTN_MOUSE + 1), (mfb_key_mod) window_data->mod_keys, state == 1);
}

//  Scroll and other axis notifications.
//
//  For scroll events (vertical and horizontal scroll axes), the
//  value parameter is the length of a vector along the specified
//  axis in a coordinate space identical to those of motion events,
//  representing a relative movement along the specified axis.
//
//  For devices that support movements non-parallel to axes multiple
//  axis events will be emitted.
//
//  When applicable, for example for touch pads, the server can
//  choose to emit scroll events where the motion vector is
//  equivalent to a motion event vector.
//
//  When applicable, a client can transform its content relative to
//  the scroll distance.
//
//  time:  timestamp with millisecond granularity
//  axis:  axis type
//  value: length of vector in surface-local coordinate space
static void
pointer_axis(void *data, struct wl_pointer *pointer, uint32_t time, uint32_t axis, wl_fixed_t value)
{
    kUnused(pointer);
    kUnused(time);
    kUnused(axis);

    SWindowData *window_data = data;
    if (axis == 0) {
        window_data->mouse_wheel_y = -(value / 256.0f);
        kCall(mouse_wheel_func, (mfb_key_mod) window_data->mod_keys, 0.0f, window_data->mouse_wheel_y);
    }
    else if (axis == 1) {
        window_data->mouse_wheel_x = -(value / 256.0f);
        kCall(mouse_wheel_func, (mfb_key_mod) window_data->mod_keys, window_data->mouse_wheel_x, 0.0f);
    }
}

static const struct
wl_pointer_listener pointer_listener = {
    .enter         = pointer_enter,
    .leave         = stub,
    .motion        = pointer_motion,
    .button        = pointer_button,
    .axis          = pointer_axis,
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void
seat_capabilities(void *data, struct wl_seat *seat, enum wl_seat_capability caps)
{
    SWindowData     *window_data     = data;
    SWindowData_Way *window_data_way = window_data->specific;

    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !window_data_way->keyboard)
    {
        window_data_way->keyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_add_listener(window_data_way->keyboard, &keyboard_listener, window_data);
    }
    else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && window_data_way->keyboard)
    {
        wl_keyboard_destroy(window_data_way->keyboard);
        window_data_way->keyboard = NULL;
    }

    if ((caps & WL_SEAT_CAPABILITY_POINTER) && !window_data_way->pointer)
    {
        window_data_way->pointer = wl_seat_get_pointer(seat);
        wl_pointer_add_listener(window_data_way->pointer, &pointer_listener, window_data);
    }
    else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && window_data_way->pointer)
    {
        wl_pointer_destroy(window_data_way->pointer);
        window_data_way->pointer = NULL;
    }
}

static const struct
wl_seat_listener seat_listener = {
    .capabilities = seat_capabilities,
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// pixel format description
//
// Informs the client about a valid pixel format that can be used
// for buffers. Known formats include argb8888 and xrgb8888.
//
// format: buffer pixel format
static void
shm_format(void *data, struct wl_shm *shm, uint32_t format)
{
    kUnused(shm);

    SWindowData     *window_data     = data;
    SWindowData_Way *window_data_way = window_data->specific;
    if (window_data_way->shm_format == -1u && format == WL_SHM_FORMAT_XRGB8888)
        window_data_way->shm_format = format;
}

static const struct
wl_shm_listener shm_listener = {
    .format = shm_format
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void
handle_toplevel_configure(void *data, struct xdg_toplevel *toplevel, int32_t width, int32_t height, struct wl_array *states)
{
    kUnused(states);

    SWindowData *window_data = data;
  
    bool update_width = width != 0 && (uint32_t) width != window_data->window_width;
    if (update_width)
        window_data->window_width = width;

    bool update_height = height != 0 && (uint32_t) height != window_data->window_height;
    if (update_height)
        window_data->window_height = height;

    if (update_width || update_height) {
        resize_dst(window_data, window_data->window_width, window_data->window_height);
        kCall(resize_func, window_data->window_width, window_data->window_height);
    }
}

static void
handle_toplevel_close(void *data, struct xdg_toplevel *xdg_toplevel)
{
    kUnused(xdg_toplevel);

    SWindowData *window_data = data;
    if (!window_data->close_func || window_data->close_func((struct mfb_window *) window_data))
        window_data->close = true;
}

static const struct xdg_toplevel_listener toplevel_listener = {
    .configure = handle_toplevel_configure,
    .close = handle_toplevel_close,
    .wm_capabilities = stub
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void
handle_shell_surface_configure(void *data, struct xdg_surface *shell_surface, uint32_t serial)
{
    xdg_surface_ack_configure(shell_surface, serial);
}

static const struct xdg_surface_listener shell_surface_listener = {
    .configure = handle_shell_surface_configure
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void
handle_fractional_scale_preferred_scale(void *data, struct wp_fractional_scale_v1 *fractional_scale, uint32_t scale)
{
    SWindowData     *window_data     = data;
    SWindowData_Way *window_data_way = window_data->specific;

    window_data_way->scale = scale / 120.0;
}

static const struct wp_fractional_scale_v1_listener fractional_scale_listener = {
    .preferred_scale = handle_fractional_scale_preferred_scale
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void
handle_surface_preferred_buffer_scale(void *data, struct wl_surface *surface, int32_t scale)
{
    SWindowData     *window_data     = data;
    SWindowData_Way *window_data_way = window_data->specific;

    window_data_way->scale = scale;
}

static const struct wl_surface_listener surface_listener = {
    .preferred_buffer_scale = handle_surface_preferred_buffer_scale
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void
registry_global(void *data, struct wl_registry *registry, uint32_t id, char const *iface, uint32_t version)
{
    kUnused(version);

    SWindowData     *window_data     = data;
    SWindowData_Way *window_data_way = window_data->specific;
    if (strcmp(iface, "wl_compositor") == 0)
        window_data_way->compositor = (struct wl_compositor *) wl_registry_bind(registry, id, &wl_compositor_interface, 1);
    else if (strcmp(iface, "xdg_wm_base") == 0)
        window_data_way->shell = (struct xdg_wm_base *) wl_registry_bind(registry, id, &xdg_wm_base_interface, 1);
    else if (strcmp(iface, "wl_shm") == 0)
        window_data_way->shm = (struct wl_shm *) wl_registry_bind(registry, id, &wl_shm_interface, 1);
    else if (strcmp(iface, "wl_seat") == 0)
        window_data_way->seat = (struct wl_seat *) wl_registry_bind(registry, id, &wl_seat_interface, 1);
    else if (strcmp(iface, "wp_fractional_scale_manager_v1") == 0)
        window_data_way->fractional_scale_manager = wl_registry_bind(registry, id, &wp_fractional_scale_manager_v1_interface, 1);
}

static const struct
wl_registry_listener registry_listener = {
    .global        = registry_global,
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct mfb_window *
mfb_open_ex(const char *title, unsigned width, unsigned height, unsigned flags)
{
    SWindowData *window_data = malloc(sizeof(SWindowData));
    if (!window_data) {
        return NULL;
    }
    memset(window_data, 0, sizeof(SWindowData));

    SWindowData_Way *window_data_way = malloc(sizeof(SWindowData_Way));
    if (!window_data_way)
        goto out;
    memset(window_data_way, 0, sizeof(SWindowData_Way));
    window_data->specific = window_data_way;

    window_data_way->shm_format = -1u;
    window_data_way->scale = 1;

    window_data_way->display = wl_display_connect(NULL);
    if (!window_data_way->display)
        goto out;
    window_data_way->registry = wl_display_get_registry(window_data_way->display);
    wl_registry_add_listener(window_data_way->registry, &registry_listener, window_data);

    if (wl_display_roundtrip(window_data_way->display) == -1)
        goto out;
    if (!window_data_way->compositor ||
        !window_data_way->shell ||
        !window_data_way->shm)
        goto out;

    const char *xcursor_theme = getenv("XCURSOR_THEME");
    const char *xcursor_size_string = getenv("XCURSOR_SIZE");
    int xcursor_size = 24;
    if (xcursor_size_string) {
        int size = (int)strtol(xcursor_size_string, NULL, 10);
        if (size > 0)
            xcursor_size = size;
    }

    window_data_way->cursor_theme = wl_cursor_theme_load(xcursor_theme, xcursor_size, window_data_way->shm);
    window_data_way->default_cursor = wl_cursor_theme_get_cursor(window_data_way->cursor_theme, "left_ptr");
    window_data_way->cursor_surface = wl_compositor_create_surface(window_data_way->compositor);

    xdg_wm_base_add_listener(window_data_way->shell, &shell_listener, NULL);
    wl_shm_add_listener(window_data_way->shm, &shm_listener, window_data);
    if (window_data_way->seat)
        wl_seat_add_listener(window_data_way->seat, &seat_listener, window_data);

    window_data->window_width  = width;
    window_data->window_height = height;

    window_data_way->surface = wl_compositor_create_surface(window_data_way->compositor);
    if (!window_data_way->surface)
        goto out;

    if (window_data_way->fractional_scale_manager) {
        window_data_way->fractional_scale = wp_fractional_scale_manager_v1_get_fractional_scale(window_data_way->fractional_scale_manager, window_data_way->surface);
        wp_fractional_scale_v1_add_listener(window_data_way->fractional_scale, &fractional_scale_listener, window_data);
    } else {
        // This doesn't seem to work for me, but it doesn't hurt to keep it
        wl_surface_add_listener(window_data_way->surface, &surface_listener, window_data);
    }

    window_data_way->shell_surface = xdg_wm_base_get_xdg_surface(window_data_way->shell, window_data_way->surface);
    if (!window_data_way->shell_surface)
        goto out;
    xdg_surface_add_listener(window_data_way->shell_surface, &shell_surface_listener, NULL);

    window_data_way->toplevel = xdg_surface_get_toplevel(window_data_way->shell_surface);
    if (!window_data_way->toplevel)
        goto out;
    xdg_toplevel_add_listener(window_data_way->toplevel, &toplevel_listener, window_data);

    xdg_toplevel_set_title(window_data_way->toplevel, title);
    if (!(flags & WF_RESIZABLE)) {
        xdg_toplevel_set_min_size(window_data_way->toplevel, width, height);
        xdg_toplevel_set_max_size(window_data_way->toplevel, width, height);
    }
    if (flags & WF_FULLSCREEN)
        xdg_toplevel_set_fullscreen(window_data_way->toplevel, NULL);
  
    wl_surface_commit(window_data_way->surface);

    if (wl_display_roundtrip(window_data_way->display) == -1)
        goto out;

    if (window_data_way->shm_format == -1u)
        goto out;

    width = window_data->window_width;
    height = window_data->window_height;

    char const *xdg_rt_dir = getenv("XDG_RUNTIME_DIR");
    char shmfile[PATH_MAX];
    uint32_t ret = snprintf(shmfile, sizeof(shmfile), "%s/WaylandMiniFB-SHM-XXXXXX", xdg_rt_dir);
    if (ret >= sizeof(shmfile))
        goto out;

    window_data_way->fd = mkstemp(shmfile);
    if (window_data_way->fd == -1)
        goto out;
    unlink(shmfile);

    window_data_way->pool_size = sizeof(uint32_t) * width * height;

    if (ftruncate(window_data_way->fd, window_data_way->pool_size) == -1)
        goto out;

    window_data_way->shm_ptr = (uint32_t *) mmap(NULL, window_data_way->pool_size, PROT_WRITE, MAP_SHARED, window_data_way->fd, 0);
    if (window_data_way->shm_ptr == MAP_FAILED)
        goto out;

    window_data->buffer_width  = width;
    window_data->buffer_height = height;
    window_data->buffer_stride = sizeof(uint32_t) * width;
    calc_dst_factor(window_data, width, height);

    window_data_way->shm_pool = wl_shm_create_pool(window_data_way->shm, window_data_way->fd, window_data_way->pool_size);
    window_data->draw_buffer  = wl_shm_pool_create_buffer(window_data_way->shm_pool, 0,
                                    window_data->buffer_width, window_data->buffer_height,
                                    window_data->buffer_stride, window_data_way->shm_format);

    window_data_way->timer = mfb_timer_create();
  
#if defined(_DEBUG) || defined(DEBUG)
    printf("Window created using Wayland API\n");
#endif

    window_data->is_initialized = true;
    return (struct mfb_window *) window_data;

out:
    close(window_data_way->fd);
    destroy(window_data);

    return NULL;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

mfb_update_state
mfb_update_ex(struct mfb_window *window, void *buffer, unsigned width, unsigned height)
{
    if (!window)
        return STATE_INVALID_WINDOW;

    SWindowData *window_data = (SWindowData *) window;
    if (window_data->close) {
        destroy(window_data);
        return STATE_EXIT;
    }

    if (!buffer)
        return STATE_INVALID_BUFFER;

    SWindowData_Way *window_data_way = window_data->specific;

    bool update_width = window_data->buffer_width != width;
    if (update_width) {
        window_data->buffer_width  = width;
        window_data->buffer_stride = sizeof(uint32_t) * width;
    }
  
    bool update_height = window_data->buffer_height != height;
    if (update_height)
        window_data->buffer_height = height;
  
    if (update_width || update_height) {
        uint32_t required_size = sizeof(uint32_t) * width * height;
        if (required_size > window_data_way->pool_size) {
            if (ftruncate(window_data_way->fd, required_size) == -1)
                return STATE_INTERNAL_ERROR;

            munmap(window_data_way->shm_ptr, window_data_way->pool_size);
            window_data_way->shm_ptr = (uint32_t *) mmap(NULL, required_size, PROT_WRITE, MAP_SHARED, window_data_way->fd, 0);
            if (window_data_way->shm_ptr == MAP_FAILED)
                return STATE_INTERNAL_ERROR;
            wl_shm_pool_resize(window_data_way->shm_pool, required_size);

            window_data_way->pool_size = required_size;
        }
      
        wl_buffer_destroy(window_data->draw_buffer);
        window_data->draw_buffer = wl_shm_pool_create_buffer(window_data_way->shm_pool, 0,
                                        window_data->buffer_width, window_data->buffer_height,
                                        window_data->buffer_stride, window_data_way->shm_format);
    }
  

    // update shm buffer
    memcpy(window_data_way->shm_ptr, buffer, window_data->buffer_stride * window_data->buffer_height);

    wl_surface_attach(window_data_way->surface, (struct wl_buffer *) window_data->draw_buffer, window_data->dst_offset_x, window_data->dst_offset_y);
    wl_surface_damage(window_data_way->surface, window_data->dst_offset_x, window_data->dst_offset_y, window_data->dst_width, window_data->dst_height);
    wl_surface_commit(window_data_way->surface);

    if (wl_display_roundtrip(window_data_way->display) == -1)
        return STATE_INTERNAL_ERROR;

    return STATE_OK;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

mfb_update_state
mfb_update_events(struct mfb_window *window)
{
    if (!window) {
        return STATE_INVALID_WINDOW;
    }

    SWindowData *window_data = (SWindowData *) window;
    if (window_data->close) {
        destroy(window_data);
        return STATE_EXIT;
    }

    SWindowData_Way *window_data_way = window_data->specific;
    if (!window_data_way->display || wl_display_get_error(window_data_way->display) != 0)
        return STATE_INTERNAL_ERROR;

    if (wl_display_dispatch_pending(window_data_way->display) == -1) {
        return STATE_INTERNAL_ERROR;
    }

    return STATE_OK;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extern double g_time_for_frame;

bool
mfb_wait_sync(struct mfb_window *window) {
    if (!window) {
        return false;
    }

    SWindowData *window_data = (SWindowData *) window;
    if (window_data->close) {
        destroy(window_data);
        return false;
    }

    SWindowData_Way *window_data_way = window_data->specific;
    double      current;
    uint32_t    millis = 1;
    while (true) {
        current = mfb_timer_now(window_data_way->timer);
        if (current >= g_time_for_frame * 0.96) {
            mfb_timer_reset(window_data_way->timer);
            return true;
        }
        else if (current >= g_time_for_frame * 0.8) {
            millis = 0;
        }

        usleep(millis * 1000);
        //sched_yield();

        if (millis == 1) {
            if (wl_display_dispatch_pending(window_data_way->display) == -1) {
                return false;
            }

            if (window_data->close) {
                destroy_window_data(window_data);
                return false;
            }
        }
    }

    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void
mfb_get_monitor_scale(struct mfb_window *window, float *scale_x, float *scale_y)
{
    if (!window)
    {
        *scale_x = 1.0;
        *scale_y = 1.0;
        return;
    }

    SWindowData     *window_data     = (SWindowData *) window;
    SWindowData_Way *window_data_way = window_data->specific;

    *scale_x = window_data_way->scale;
    *scale_y = window_data_way->scale;
}
