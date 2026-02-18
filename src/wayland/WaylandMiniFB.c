#ifndef _XOPEN_SOURCE
    #define _XOPEN_SOURCE 700  // for mkstemp, ftruncate, usleep
#endif

#ifndef _DEFAULT_SOURCE
    #define _DEFAULT_SOURCE     // ensure usleep prototype on glibc
#endif

#include <MiniFB.h>
#include "generated/xdg-shell-client-protocol.h"
#include "generated/xdg-decoration-client-protocol.h"
#include "generated/fractional-scale-v1-client-protocol.h"
#include "MiniFB_internal.h"
#include "MiniFB_enums.h"
#include "WindowData.h"
#include "WindowData_Way.h"

#include <wayland-client.h>
#include <wayland-cursor.h>
#include <xkbcommon/xkbcommon.h>

#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <sched.h>
#include <errno.h>

#include <linux/limits.h>
#include <linux/input.h>
#include <linux/input-event-codes.h>

#include <sys/mman.h>

//-------------------------------------
#ifndef MFB_APP_ID
    #define MFB_APP_ID minifb
#endif

#define MFB_STR_IMPL(x) #x
#define MFB_STR(x) MFB_STR_IMPL(x)

#define WAYLAND_MAX_OUTPUTS 16
#define WAYLAND_FRACTIONAL_SCALE_DENOMINATOR 120.0f

//-------------------------------------
void init_keycodes();

//-------------------------------------
static void
update_mod_keys_from_xkb(SWindowData *window_data, SWindowData_Way *window_data_way) {
    if (window_data == NULL || window_data_way == NULL || window_data_way->xkb_state == NULL) {
        return;
    }

    window_data->mod_keys = 0;
    if (xkb_state_mod_name_is_active(window_data_way->xkb_state, XKB_MOD_NAME_SHIFT, XKB_STATE_MODS_EFFECTIVE) > 0) {
        window_data->mod_keys |= KB_MOD_SHIFT;
    }

    if (xkb_state_mod_name_is_active(window_data_way->xkb_state, XKB_MOD_NAME_CTRL, XKB_STATE_MODS_EFFECTIVE) > 0) {
        window_data->mod_keys |= KB_MOD_CONTROL;
    }

    if (xkb_state_mod_name_is_active(window_data_way->xkb_state, XKB_MOD_NAME_ALT, XKB_STATE_MODS_EFFECTIVE) > 0) {
        window_data->mod_keys |= KB_MOD_ALT;
    }

    if (xkb_state_mod_name_is_active(window_data_way->xkb_state, XKB_MOD_NAME_LOGO, XKB_STATE_MODS_EFFECTIVE) > 0) {
        window_data->mod_keys |= KB_MOD_SUPER;
    }
}

//-------------------------------------
static void
destroy_window_data(SWindowData *window_data) {
    if(window_data == NULL) {
        mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: destroy_window_data called with a null window pointer.");
        return;
    }

    if(window_data->draw_buffer) {
        wl_buffer_destroy(window_data->draw_buffer);
        window_data->draw_buffer = NULL;
    }

    SWindowData_Way   *window_data_way = (SWindowData_Way *) window_data->specific;
    if(window_data_way != NULL) {
        if (window_data_way->shm_ptr && window_data_way->shm_ptr != MAP_FAILED && window_data_way->shm_length > 0) {
            munmap(window_data_way->shm_ptr, window_data_way->shm_length);
            window_data_way->shm_ptr = NULL;
            window_data_way->shm_length = 0;
        }

        if (window_data_way->xkb_state) {
            xkb_state_unref(window_data_way->xkb_state);
        }

        if (window_data_way->xkb_keymap) {
            xkb_keymap_unref(window_data_way->xkb_keymap);
        }

        if (window_data_way->xkb_context) {
            xkb_context_unref(window_data_way->xkb_context);
        }

        mfb_timer_destroy(window_data_way->timer);
        memset(window_data_way, 0, sizeof(SWindowData_Way));
        free(window_data_way);
    }
    memset(window_data, 0, sizeof(SWindowData));
    free(window_data);
}

//-------------------------------------
static void
destroy(SWindowData *window_data) {
    if(window_data == NULL) {
        mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: destroy called with a null window pointer.");
        return;
    }

    SWindowData_Way *window_data_way = (SWindowData_Way *) window_data->specific;
    if (window_data_way == NULL || window_data_way->display == NULL) {
        mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: missing Wayland display state during destroy, forcing local cleanup.");
        if (window_data_way) {
            if (window_data_way->toplevel_decoration) {
                zxdg_toplevel_decoration_v1_destroy(window_data_way->toplevel_decoration);
                window_data_way->toplevel_decoration = NULL;
            }

            if (window_data_way->toplevel) {
                xdg_toplevel_destroy(window_data_way->toplevel);
                window_data_way->toplevel = NULL;
            }

            if (window_data_way->shell_surface) {
                xdg_surface_destroy(window_data_way->shell_surface);
                window_data_way->shell_surface = NULL;
            }

            if (window_data_way->shell) {
                xdg_wm_base_destroy(window_data_way->shell);
                window_data_way->shell = NULL;
            }

            if (window_data_way->decoration_manager) {
                zxdg_decoration_manager_v1_destroy(window_data_way->decoration_manager);
                window_data_way->decoration_manager = NULL;
            }

            if (window_data_way->fractional_scale) {
                wp_fractional_scale_v1_destroy(window_data_way->fractional_scale);
                window_data_way->fractional_scale = NULL;
            }

            if (window_data_way->fractional_scale_manager) {
                wp_fractional_scale_manager_v1_destroy(window_data_way->fractional_scale_manager);
                window_data_way->fractional_scale_manager = NULL;
            }

            if (window_data_way->surface) {
                wl_surface_destroy(window_data_way->surface);
                window_data_way->surface = NULL;
            }

            if (window_data_way->cursor_surface) {
                wl_surface_destroy(window_data_way->cursor_surface);
                window_data_way->cursor_surface = NULL;
            }

            if (window_data_way->cursor_theme) {
                wl_cursor_theme_destroy(window_data_way->cursor_theme);
                window_data_way->cursor_theme = NULL;
            }

            if (window_data_way->shm_pool) {
                wl_shm_pool_destroy(window_data_way->shm_pool);
                window_data_way->shm_pool = NULL;
            }

            if (window_data_way->shm) {
                wl_shm_destroy(window_data_way->shm);
                window_data_way->shm = NULL;
            }

            for (uint32_t i = 0; i < window_data_way->output_count; ++i) {
                if (window_data_way->outputs[i]) {
                    wl_output_destroy(window_data_way->outputs[i]);
                    window_data_way->outputs[i] = NULL;
                }
            }

            window_data_way->output_count = 0;
            window_data_way->current_output = NULL;
            window_data_way->current_output_scale = 1;

            if (window_data_way->compositor) {
                wl_compositor_destroy(window_data_way->compositor);
                window_data_way->compositor = NULL;
            }

            if (window_data_way->keyboard) {
                wl_keyboard_destroy(window_data_way->keyboard);
                window_data_way->keyboard = NULL;
            }

            if (window_data_way->pointer) {
                wl_pointer_destroy(window_data_way->pointer);
                window_data_way->pointer = NULL;
            }

            if (window_data_way->seat) {
                wl_seat_destroy(window_data_way->seat);
                window_data_way->seat = NULL;
            }

            if (window_data_way->registry) {
                wl_registry_destroy(window_data_way->registry);
                window_data_way->registry = NULL;
            }

            if (window_data_way->fd >= 0) {
                close(window_data_way->fd);
                window_data_way->fd = -1;
            }
        }

        destroy_window_data(window_data);
        return;
    }

    // Destroy XDG objects with correct functions
    if (window_data_way->toplevel_decoration) {
        zxdg_toplevel_decoration_v1_destroy(window_data_way->toplevel_decoration);
        window_data_way->toplevel_decoration = NULL;
    }

    if (window_data_way->toplevel) {
        xdg_toplevel_destroy(window_data_way->toplevel);
        window_data_way->toplevel = NULL;
    }

    if (window_data_way->shell_surface) {
        xdg_surface_destroy(window_data_way->shell_surface);
        window_data_way->shell_surface = NULL;
    }

    if (window_data_way->shell) {
        xdg_wm_base_destroy(window_data_way->shell);
        window_data_way->shell = NULL;
    }

    if (window_data_way->decoration_manager) {
        zxdg_decoration_manager_v1_destroy(window_data_way->decoration_manager);
        window_data_way->decoration_manager = NULL;
    }

    if (window_data_way->fractional_scale) {
        wp_fractional_scale_v1_destroy(window_data_way->fractional_scale);
        window_data_way->fractional_scale = NULL;
    }

    if (window_data_way->fractional_scale_manager) {
        wp_fractional_scale_manager_v1_destroy(window_data_way->fractional_scale_manager);
        window_data_way->fractional_scale_manager = NULL;
    }

    if (window_data_way->surface) {
        wl_surface_destroy(window_data_way->surface);
        window_data_way->surface = NULL;
    }

    // Restore KILL macro for remaining Wayland objects
#define KILL(NAME)                                      \
    do                                                  \
    {                                                   \
        if (window_data_way->NAME)                      \
            wl_##NAME##_destroy(window_data_way->NAME); \
        window_data_way->NAME = NULL;                   \
    } while (0)

    //KILL(buffer);
    if(window_data->draw_buffer) {
        wl_buffer_destroy(window_data->draw_buffer);
        window_data->draw_buffer = NULL;
    }

    // Clean up cursor objects
    if (window_data_way->cursor_surface) {
        wl_surface_destroy(window_data_way->cursor_surface);
        window_data_way->cursor_surface = NULL;
    }

    if (window_data_way->cursor_theme) {
        wl_cursor_theme_destroy(window_data_way->cursor_theme);
        window_data_way->cursor_theme = NULL;
    }

    KILL(shm_pool);
    KILL(shm);
    for (uint32_t i = 0; i < window_data_way->output_count; ++i) {
        if (window_data_way->outputs[i]) {
            wl_output_destroy(window_data_way->outputs[i]);
            window_data_way->outputs[i] = NULL;
        }
    }

    window_data_way->output_count = 0;
    window_data_way->current_output = NULL;
    window_data_way->current_output_scale = 1;
    KILL(compositor);
    KILL(keyboard);
    if (window_data_way->pointer) {
        wl_pointer_destroy(window_data_way->pointer);
        window_data_way->pointer = NULL;
    }
    KILL(seat);
    KILL(registry);
    wl_display_disconnect(window_data_way->display);

    if (window_data_way->fd >= 0) {
        close(window_data_way->fd);
        window_data_way->fd = -1;
    }

    destroy_window_data(window_data);
}

#undef KILL

//-------------------------------------
static void
handle_ping(void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial) {
    kUnused(data);
    xdg_wm_base_pong(xdg_wm_base, serial);
}

//-------------------------------------
static struct xdg_wm_base_listener shell_listener = {
    handle_ping
};

//-------------------------------------
// This event provides a file descriptor to the client which can be memory-mapped
// to provide a keyboard mapping description.
// format: keymap format
// fd:     keymap file descriptor
// size:   keymap size, in bytes
//-------------------------------------
static void
keyboard_keymap(void *data, struct wl_keyboard *keyboard, uint32_t format, int fd, uint32_t size) {
    SWindowData *window_data = (SWindowData *) data;
    SWindowData_Way *window_data_way = window_data ? (SWindowData_Way *) window_data->specific : NULL;
    kUnused(keyboard);

    if (window_data_way == NULL) {
        if (fd >= 0) {
            close(fd);
        }
        mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: keyboard_keymap received without valid window state.");
        return;
    }

    if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1 || fd < 0 || size == 0) {
        if (fd >= 0) {
            close(fd);
        }
        mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: unsupported or invalid keymap payload.");
        return;
    }

    char *keymap_data = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (keymap_data == MAP_FAILED) {
        mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: failed to map Wayland keymap (%s).", strerror(errno));
        return;
    }

    if (window_data_way->xkb_context == NULL) {
        window_data_way->xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
        if (window_data_way->xkb_context == NULL) {
            munmap(keymap_data, size);
            mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: xkb_context_new failed.");
            return;
        }
    }

    struct xkb_keymap *keymap = xkb_keymap_new_from_string(window_data_way->xkb_context,
                                                            keymap_data,
                                                            XKB_KEYMAP_FORMAT_TEXT_V1,
                                                            XKB_KEYMAP_COMPILE_NO_FLAGS);
    munmap(keymap_data, size);
    if (keymap == NULL) {
        mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: xkb_keymap_new_from_string failed.");
        return;
    }

    struct xkb_state *state = xkb_state_new(keymap);
    if (state == NULL) {
        xkb_keymap_unref(keymap);
        mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: xkb_state_new failed.");
        return;
    }

    if (window_data_way->xkb_state) {
        xkb_state_unref(window_data_way->xkb_state);
    }
    if (window_data_way->xkb_keymap) {
        xkb_keymap_unref(window_data_way->xkb_keymap);
    }

    window_data_way->xkb_keymap = keymap;
    window_data_way->xkb_state = state;
}

//-------------------------------------
// Notification that this seat's keyboard focus is on a certain surface.
// serial:  serial number of the enter event
// surface: surface gaining keyboard focus
// keys:    the currently pressed keys
//-------------------------------------
static void
keyboard_enter(void *data, struct wl_keyboard *keyboard, uint32_t serial, struct wl_surface *surface, struct wl_array *keys) {
    kUnused(keyboard);
    kUnused(serial);
    kUnused(surface);
    kUnused(keys);

    SWindowData *window_data = (SWindowData *) data;
    window_data->is_active = true;
    kCall(active_func, true);
}

//-------------------------------------
// The leave notification is sent before the enter notification for the new focus.
// serial:  serial number of the leave event
// surface: surface that lost keyboard focus
//-------------------------------------
static void
keyboard_leave(void *data, struct wl_keyboard *keyboard, uint32_t serial, struct wl_surface *surface) {
    kUnused(keyboard);
    kUnused(serial);
    kUnused(surface);

    SWindowData *window_data = (SWindowData *) data;
    window_data->is_active = false;
    kCall(active_func, false);
}

//-------------------------------------
// A key was pressed or released. The time argument is a timestamp with
// millisecond granularity, with an undefined base.
// serial: serial number of the key event
// time:   timestamp with millisecond granularity
// key:    key that produced the event
// state:  physical state of the key
//-------------------------------------
static void
keyboard_key(void *data, struct wl_keyboard *keyboard, uint32_t serial, uint32_t time, uint32_t key, uint32_t state) {
    kUnused(keyboard);
    kUnused(serial);
    kUnused(time);

    SWindowData *window_data = (SWindowData *) data;
    SWindowData_Way *window_data_way = (SWindowData_Way *) window_data->specific;
    if(key < 512) {
        mfb_key key_code = (mfb_key) g_keycodes[key];
        bool   is_pressed = (bool) (state == WL_KEYBOARD_KEY_STATE_PRESSED);
        if (window_data_way && window_data_way->xkb_state) {
            xkb_keycode_t xkb_keycode = (xkb_keycode_t) key + 8;
            xkb_state_update_key(window_data_way->xkb_state, xkb_keycode, is_pressed ? XKB_KEY_DOWN : XKB_KEY_UP);
            update_mod_keys_from_xkb(window_data, window_data_way);
            if (is_pressed) {
                uint32_t codepoint = xkb_state_key_get_utf32(window_data_way->xkb_state, xkb_keycode);
                if (codepoint != 0) {
                    kCall(char_input_func, codepoint);
                }
            }
        }

        else {
            switch (key_code) {
                case KB_KEY_LEFT_SHIFT:
                case KB_KEY_RIGHT_SHIFT:
                    if(is_pressed)
                        window_data->mod_keys |= KB_MOD_SHIFT;
                    else
                        window_data->mod_keys &= ~KB_MOD_SHIFT;
                    break;

                case KB_KEY_LEFT_CONTROL:
                case KB_KEY_RIGHT_CONTROL:
                    if(is_pressed)
                        window_data->mod_keys |= KB_MOD_CONTROL;
                    else
                        window_data->mod_keys &= ~KB_MOD_CONTROL;
                    break;

                case KB_KEY_LEFT_ALT:
                case KB_KEY_RIGHT_ALT:
                    if(is_pressed)
                        window_data->mod_keys |= KB_MOD_ALT;
                    else
                        window_data->mod_keys &= ~KB_MOD_ALT;
                    break;

                case KB_KEY_LEFT_SUPER:
                case KB_KEY_RIGHT_SUPER:
                    if(is_pressed)
                        window_data->mod_keys |= KB_MOD_SUPER;
                    else
                        window_data->mod_keys &= ~KB_MOD_SUPER;
                    break;
            }
        }

        window_data->key_status[key_code] = is_pressed;
        kCall(keyboard_func, key_code, (mfb_key_mod) window_data->mod_keys, is_pressed);
    }
}

//-------------------------------------
// Notifies clients that the modifier and/or group state has changed,
// and it should update its local state.
// serial:         serial number of the modifiers event
// mods_depressed: depressed modifiers
// mods_latched:   latched modifiers
// mods_locked:    locked modifiers
// group:          keyboard layout
//-------------------------------------
static void
keyboard_modifiers(void *data, struct wl_keyboard *keyboard, uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group) {
    kUnused(keyboard);
    kUnused(serial);

    SWindowData *window_data = (SWindowData *) data;
    SWindowData_Way *window_data_way = window_data ? (SWindowData_Way *) window_data->specific : NULL;
    if (window_data_way && window_data_way->xkb_state) {
        xkb_state_update_mask(window_data_way->xkb_state,
                              mods_depressed,
                              mods_latched,
                              mods_locked,
                              0, 0,
                              group);
        update_mod_keys_from_xkb(window_data, window_data_way);
    }
}

//-------------------------------------
// Informs the client about the keyboard's repeat rate and delay.
// rate:  the rate of repeating keys in characters per second
// delay: delay in milliseconds since key down until repeating starts
//-------------------------------------
static void
keyboard_repeat_info(void *data, struct wl_keyboard *keyboard, int32_t rate, int32_t delay) {
    kUnused(data);
    kUnused(keyboard);
    kUnused(rate);
    kUnused(delay);
}

//-------------------------------------
static const struct
wl_keyboard_listener keyboard_listener = {
    .keymap      = keyboard_keymap,
    .enter       = keyboard_enter,
    .leave       = keyboard_leave,
    .key         = keyboard_key,
    .modifiers   = keyboard_modifiers,
    .repeat_info = NULL,
};

//-------------------------------------
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
//-------------------------------------
static void
pointer_enter(void *data, struct wl_pointer *pointer, uint32_t serial, struct wl_surface *surface, wl_fixed_t sx, wl_fixed_t sy) {
    //kUnused(pointer);
    //kUnused(serial);
    kUnused(surface);
    kUnused(sx);
    kUnused(sy);
    struct wl_buffer *buffer;
    struct wl_cursor_image *image;

    SWindowData *window_data = (SWindowData *) data;
    SWindowData_Way *window_data_way = (SWindowData_Way *) window_data->specific;
    window_data_way->pointer_serial = serial;
    window_data_way->pointer_serial_valid = 1;

    if (window_data->is_cursor_visible) {
        if (window_data_way->default_cursor == NULL ||
            window_data_way->default_cursor->image_count == 0 ||
            window_data_way->cursor_surface == NULL) {
            return;
        }
        image  = window_data_way->default_cursor->images[0];
        buffer = wl_cursor_image_get_buffer(image);
        if (buffer == NULL) {
            return;
        }

        wl_pointer_set_cursor(pointer, serial, window_data_way->cursor_surface, image->hotspot_x, image->hotspot_y);
        wl_surface_attach(window_data_way->cursor_surface, buffer, 0, 0);
        wl_surface_damage(window_data_way->cursor_surface, 0, 0, image->width, image->height);
        wl_surface_commit(window_data_way->cursor_surface);
    }
    else {
        wl_pointer_set_cursor(pointer, 0, NULL, 0, 0);
    }

    //mfb_log(MFB_LOG_DEBUG, "Pointer entered surface %p at %d %d (serial: %d)", surface, sx, sy, serial);
}

//-------------------------------------
// Notification that this seat's pointer is no longer focused on a certain surface.
//
// The leave notification is sent before the enter notification for the new focus.
//
// serial:  serial number of the leave event
// surface: surface left by the pointer
//-------------------------------------
static void
pointer_leave(void *data, struct wl_pointer *pointer, uint32_t serial, struct wl_surface *surface) {
    kUnused(pointer);
    kUnused(serial);
    kUnused(surface);

    SWindowData *window_data = (SWindowData *) data;
    SWindowData_Way *window_data_way = window_data ? (SWindowData_Way *) window_data->specific : NULL;
    if (window_data_way) {
        window_data_way->pointer_serial_valid = 0;
    }

    //mfb_log(MFB_LOG_DEBUG, "Pointer left surface %p (serial: %d)", surface, serial);
}

//-------------------------------------
// Notification of pointer location change.
//
// The arguments sx and sy are the location relative to the focused surface.
//
// time:  timestamp with millisecond granularity
// sx:    surface-local x coordinate
// sy:    surface-local y coordinate
//-------------------------------------
static void
pointer_motion(void *data, struct wl_pointer *pointer, uint32_t time, wl_fixed_t sx, wl_fixed_t sy) {
    kUnused(pointer);
    kUnused(time);

    SWindowData *window_data = (SWindowData *) data;

    window_data->mouse_pos_x = wl_fixed_to_int(sx);
    window_data->mouse_pos_y = wl_fixed_to_int(sy);
    kCall(mouse_move_func, window_data->mouse_pos_x, window_data->mouse_pos_y);

    //mfb_log(MFB_LOG_DEBUG, "Pointer moved at %f %f", sx / 256.0f, sy / 256.0f);
}

//-------------------------------------
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
//-------------------------------------
static void
pointer_button(void *data, struct wl_pointer *pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t state) {
    kUnused(pointer);
    kUnused(time);

    //mfb_log(MFB_LOG_DEBUG, "Pointer button '%d'(%d)", button, state);
    SWindowData *window_data = (SWindowData *) data;
    SWindowData_Way *window_data_way = window_data ? (SWindowData_Way *) window_data->specific : NULL;
    if (window_data_way) {
        window_data_way->pointer_serial = serial;
        window_data_way->pointer_serial_valid = 1;
    }
    window_data->mouse_button_status[(button - BTN_MOUSE + 1) & 0x07] = (state == 1);
    kCall(mouse_btn_func, (mfb_mouse_button) (button - BTN_MOUSE + 1), (mfb_key_mod) window_data->mod_keys, state == 1);

    //mfb_log(MFB_LOG_DEBUG, "Pointer button %x, state %x (serial: %d)", button, state, serial);
}

//-------------------------------------
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
//-------------------------------------
static void
pointer_axis(void *data, struct wl_pointer *pointer, uint32_t time, uint32_t axis, wl_fixed_t value) {
    kUnused(pointer);
    kUnused(time);
    kUnused(axis);

    //mfb_log(MFB_LOG_DEBUG, "Pointer handle axis: axis: %d (0x%x)", axis, value);
    SWindowData *window_data = (SWindowData *) data;
    if(axis == 0) {
        window_data->mouse_wheel_y = -(value / 256.0f);
        kCall(mouse_wheel_func, (mfb_key_mod) window_data->mod_keys, 0.0f, window_data->mouse_wheel_y);
    }
    else if(axis == 1) {
        window_data->mouse_wheel_x = -(value / 256.0f);
        kCall(mouse_wheel_func, (mfb_key_mod) window_data->mod_keys, window_data->mouse_wheel_x, 0.0f);
    }
}

//-------------------------------------
static void
frame(void *data, struct wl_pointer *pointer) {
    kUnused(data);
    kUnused(pointer);
}

//-------------------------------------
static void
axis_source(void *data, struct wl_pointer *pointer, uint32_t axis_source) {
    kUnused(data);
    kUnused(pointer);
    kUnused(axis_source);
}

//-------------------------------------
static void
axis_stop(void *data, struct wl_pointer *pointer, uint32_t time, uint32_t axis) {
    kUnused(data);
    kUnused(pointer);
    kUnused(time);
    kUnused(axis);
}

//-------------------------------------
static void
axis_discrete(void *data, struct wl_pointer *pointer, uint32_t axis, int32_t discrete) {
    kUnused(data);
    kUnused(pointer);
    kUnused(axis);
    kUnused(discrete);
}

//-------------------------------------
static const struct
wl_pointer_listener pointer_listener = {
    .enter         = pointer_enter,
    .leave         = pointer_leave,
    .motion        = pointer_motion,
    .button        = pointer_button,
    .axis          = pointer_axis,
    .frame         = NULL,
    .axis_source   = NULL,
    .axis_stop     = NULL,
    .axis_discrete = NULL,
};

//-------------------------------------
static void
seat_capabilities(void *data, struct wl_seat *seat, enum wl_seat_capability caps) {
    kUnused(data);

    SWindowData       *window_data     = (SWindowData *) data;
    SWindowData_Way   *window_data_way = (SWindowData_Way *) window_data->specific;
    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !window_data_way->keyboard) {
        window_data_way->keyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_add_listener(window_data_way->keyboard, &keyboard_listener, window_data);
    }

    else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && window_data_way->keyboard) {
        wl_keyboard_destroy(window_data_way->keyboard);
        window_data_way->keyboard = NULL;
    }

    if ((caps & WL_SEAT_CAPABILITY_POINTER) && !window_data_way->pointer) {
        window_data_way->pointer = wl_seat_get_pointer(seat);
        wl_pointer_add_listener(window_data_way->pointer, &pointer_listener, window_data);
    }
    else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && window_data_way->pointer) {
        wl_pointer_destroy(window_data_way->pointer);
        window_data_way->pointer = NULL;
    }
}

//-------------------------------------
static void
seat_name(void *data, struct wl_seat *seat, const char *name) {
    kUnused(data);
    kUnused(seat);

    mfb_log(MFB_LOG_DEBUG, "Seat '%s'", name);
}

//-------------------------------------
static const struct
wl_seat_listener seat_listener = {
    .capabilities = seat_capabilities,
    .name         = NULL,
};

//-------------------------------------
// pixel format description
//
// Informs the client about a valid pixel format that can be used
// for buffers. Known formats include argb8888 and xrgb8888.
//
// format: buffer pixel format
//-------------------------------------
static void
shm_format(void *data, struct wl_shm *shm, uint32_t format) {
    kUnused(shm);

    SWindowData         *window_data     = (SWindowData *) data;
    SWindowData_Way   *window_data_way = (SWindowData_Way *) window_data->specific;
    if (window_data_way->shm_format == -1u) {
        switch (format) {
            // We could do RGBA, but that would not be what is expected from minifb...
            // case WL_SHM_FORMAT_ARGB8888:
            case WL_SHM_FORMAT_XRGB8888:
                window_data_way->shm_format = format;
            break;

            default:
            break;
        }
    }
}

//-------------------------------------
static const struct
wl_shm_listener shm_listener = {
    .format = shm_format
};

//-------------------------------------
static void
fractional_scale_preferred_scale(void *data, struct wp_fractional_scale_v1 *fractional_scale, uint32_t scale) {
    kUnused(fractional_scale);
    SWindowData *window_data = (SWindowData *) data;
    SWindowData_Way *window_data_way = window_data ? (SWindowData_Way *) window_data->specific : NULL;
    if (window_data_way == NULL) {
        return;
    }
    window_data_way->preferred_scale_120 = scale;
}

//-------------------------------------
static const struct wp_fractional_scale_v1_listener fractional_scale_listener = {
    .preferred_scale = fractional_scale_preferred_scale,
};

//-------------------------------------
static int
find_output_index(SWindowData_Way *window_data_way, struct wl_output *output) {
    if (window_data_way == NULL || output == NULL) {
        return -1;
    }

    for (uint32_t i = 0; i < window_data_way->output_count; ++i) {
        if (window_data_way->outputs[i] == output) {
            return (int) i;
        }
    }
    return -1;
}

//-------------------------------------
static void
output_geometry(void *data, struct wl_output *output, int32_t x, int32_t y, int32_t phys_width, int32_t phys_height,
                int32_t subpixel, const char *make, const char *model, int32_t transform) {
    kUnused(data);
    kUnused(output);
    kUnused(x);
    kUnused(y);
    kUnused(phys_width);
    kUnused(phys_height);
    kUnused(subpixel);
    kUnused(make);
    kUnused(model);
    kUnused(transform);
}

//-------------------------------------
static void
output_mode(void *data, struct wl_output *output, uint32_t flags, int32_t width, int32_t height, int32_t refresh) {
    kUnused(data);
    kUnused(output);
    kUnused(flags);
    kUnused(width);
    kUnused(height);
    kUnused(refresh);
}

//-------------------------------------
static void
output_done(void *data, struct wl_output *output) {
    kUnused(data);
    kUnused(output);
}

//-------------------------------------
static void
output_scale(void *data, struct wl_output *output, int32_t factor) {
    SWindowData *window_data = (SWindowData *) data;
    SWindowData_Way *window_data_way = window_data ? (SWindowData_Way *) window_data->specific : NULL;
    if (window_data_way == NULL || output == NULL) {
        return;
    }

    int idx = find_output_index(window_data_way, output);
    if (idx < 0) {
        return;
    }

    uint32_t scale = (factor > 0) ? (uint32_t) factor : 1;
    window_data_way->output_scales[idx] = scale;
    if (window_data_way->current_output == output) {
        window_data_way->current_output_scale = scale;
    }
}

#if defined(WL_OUTPUT_NAME_SINCE_VERSION)

//-------------------------------------
static void
output_name(void *data, struct wl_output *output, const char *name) {
    kUnused(data);
    kUnused(output);
    kUnused(name);
}

#endif

#if defined(WL_OUTPUT_DESCRIPTION_SINCE_VERSION)

//-------------------------------------
static void
output_description(void *data, struct wl_output *output, const char *description) {
    kUnused(data);
    kUnused(output);
    kUnused(description);
}

#endif

//-------------------------------------
static const struct wl_output_listener output_listener = {
    .geometry = output_geometry,
    .mode = output_mode,
    .done = output_done,
    .scale = output_scale,
#if defined(WL_OUTPUT_NAME_SINCE_VERSION)
    .name = output_name,
#endif
#if defined(WL_OUTPUT_DESCRIPTION_SINCE_VERSION)
    .description = output_description,
#endif
};

//-------------------------------------
static void
surface_enter(void *data, struct wl_surface *surface, struct wl_output *output) {
    kUnused(surface);
    SWindowData *window_data = (SWindowData *) data;
    SWindowData_Way *window_data_way = window_data ? (SWindowData_Way *) window_data->specific : NULL;
    if (window_data_way == NULL || output == NULL) {
        return;
    }

    window_data_way->current_output = output;
    int idx = find_output_index(window_data_way, output);
    if (idx >= 0 && window_data_way->output_scales[idx] > 0) {
        window_data_way->current_output_scale = window_data_way->output_scales[idx];
    }
    else {
        window_data_way->current_output_scale = 1;
    }
}

//-------------------------------------
static void
surface_leave(void *data, struct wl_surface *surface, struct wl_output *output) {
    kUnused(surface);
    SWindowData *window_data = (SWindowData *) data;
    SWindowData_Way *window_data_way = window_data ? (SWindowData_Way *) window_data->specific : NULL;
    if (window_data_way == NULL || output == NULL) {
        return;
    }

    if (window_data_way->current_output == output) {
        window_data_way->current_output = NULL;
        window_data_way->current_output_scale = 1;
    }
}

//-------------------------------------
static const struct wl_surface_listener surface_listener = {
    .enter = surface_enter,
    .leave = surface_leave,
};

//-------------------------------------
static void
toplevel_decoration_configure(void *data, struct zxdg_toplevel_decoration_v1 *decoration, uint32_t mode) {
    kUnused(data);
    kUnused(decoration);
    kUnused(mode);
}

//-------------------------------------
static const struct zxdg_toplevel_decoration_v1_listener toplevel_decoration_listener = {
    .configure = toplevel_decoration_configure
};

//-------------------------------------
static void
registry_global(void *data, struct wl_registry *registry, uint32_t id, char const *iface, uint32_t version) {
    SWindowData         *window_data     = (SWindowData *) data;
    SWindowData_Way   *window_data_way = (SWindowData_Way *) window_data->specific;
    if (strcmp(iface, "wl_compositor") == 0) {
        // Use version 1 for compositor (stable)
        window_data_way->compositor = (struct wl_compositor *) wl_registry_bind(registry, id, &wl_compositor_interface, 1);
    }

    else if (strcmp(iface, "wl_shm") == 0) {
        // Use version 1 for shm (stable)
        window_data_way->shm = (struct wl_shm *) wl_registry_bind(registry, id, &wl_shm_interface, 1);
        if (window_data_way->shm) {
            wl_shm_add_listener(window_data_way->shm, &shm_listener, window_data);
            window_data_way->cursor_theme = wl_cursor_theme_load(NULL, 32, window_data_way->shm);
            if (window_data_way->cursor_theme) {
                window_data_way->default_cursor = wl_cursor_theme_get_cursor(window_data_way->cursor_theme, "left_ptr");
            }
        }
    }

    else if (strcmp(iface, "xdg_wm_base") == 0) {
        // Bind to the maximum version supported by BOTH server and client stubs.
        // This keeps compatibility with old compositors and avoids receiving
        // events newer than the generated protocol code can decode.
        uint32_t client_version = (uint32_t) xdg_wm_base_interface.version;
        uint32_t use_version = version < client_version ? version : client_version;
        window_data_way->shell = (struct xdg_wm_base *) wl_registry_bind(registry, id, &xdg_wm_base_interface, use_version);
        if (window_data_way->shell) {
            xdg_wm_base_add_listener(window_data_way->shell, &shell_listener, NULL);
        }
    }

    else if (strcmp(iface, "wl_seat") == 0) {
        // Use version 1 for seat (stable)
        window_data_way->seat = (struct wl_seat *) wl_registry_bind(registry, id, &wl_seat_interface, 1);
        if (window_data_way->seat) {
            wl_seat_add_listener(window_data_way->seat, &seat_listener, window_data);
        }
    }

    else if (strcmp(iface, "wl_output") == 0) {
        uint32_t client_version = (uint32_t) wl_output_interface.version;
        uint32_t use_version = version < client_version ? version : client_version;
        struct wl_output *output = (struct wl_output *) wl_registry_bind(registry, id, &wl_output_interface, use_version);
        if (output) {
            wl_output_add_listener(output, &output_listener, window_data);
            if (window_data_way->output_count < WAYLAND_MAX_OUTPUTS) {
                uint32_t idx = window_data_way->output_count++;
                window_data_way->outputs[idx] = output;
                window_data_way->output_scales[idx] = 1;
            }
            else {
                wl_output_destroy(output);
            }
        }
    }

    else if (strcmp(iface, "wp_fractional_scale_manager_v1") == 0) {
        uint32_t client_version = (uint32_t) wp_fractional_scale_manager_v1_interface.version;
        uint32_t use_version = version < client_version ? version : client_version;
        window_data_way->fractional_scale_manager = (struct wp_fractional_scale_manager_v1 *)
            wl_registry_bind(registry, id, &wp_fractional_scale_manager_v1_interface, use_version);
    }

    else if (strcmp(iface, "zxdg_decoration_manager_v1") == 0) {
        uint32_t client_version = (uint32_t) zxdg_decoration_manager_v1_interface.version;
        uint32_t use_version = version < client_version ? version : client_version;
        window_data_way->decoration_manager = (struct zxdg_decoration_manager_v1 *)
            wl_registry_bind(registry, id, &zxdg_decoration_manager_v1_interface, use_version);
    }
}

//-------------------------------------
static const struct
wl_registry_listener registry_listener = {
    .global        = registry_global,
    .global_remove = NULL,
};

//-------------------------------------
static void
handle_shell_surface_configure(void *data, struct xdg_surface *shell_surface, uint32_t serial) {
    SWindowData *window_data = (SWindowData *) data;
    SWindowData_Way *window_data_way = (SWindowData_Way *) window_data->specific;

    xdg_surface_ack_configure(shell_surface, serial);

    // Some compositors apply startup states (fullscreen/maximized) more reliably
    // when requested after the first configure handshake.
    if (!window_data_way->startup_state_applied) {
        if (window_data_way->request_fullscreen) {
            xdg_toplevel_set_fullscreen(window_data_way->toplevel, NULL);
        }
        else if (window_data_way->request_maximized) {
            xdg_toplevel_set_maximized(window_data_way->toplevel);
        }
        window_data_way->startup_state_applied = 1;
    }

    // On first configure, attach buffer and commit
    if (!window_data->is_initialized) {
        wl_surface_attach(window_data_way->surface, (struct wl_buffer *) window_data->draw_buffer,
                         window_data->dst_offset_x, window_data->dst_offset_y);

        wl_surface_damage(window_data_way->surface, window_data->dst_offset_x, window_data->dst_offset_y,
                         window_data->dst_width, window_data->dst_height);

        wl_surface_commit(window_data_way->surface);
        window_data->is_initialized = true;
    }
}

//-------------------------------------
static const struct xdg_surface_listener shell_surface_listener = {
    handle_shell_surface_configure
};

//-------------------------------------
static void
handle_toplevel_configure(void *data, struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height, struct wl_array *states) {
    kUnused(xdg_toplevel);
    kUnused(states);

    SWindowData *window_data = (SWindowData *) data;
    if (window_data && width > 0 && height > 0) {
        if (window_data->window_width != (unsigned) width || window_data->window_height != (unsigned) height) {
            window_data->window_width  = (unsigned) width;
            window_data->window_height = (unsigned) height;
            resize_dst(window_data, (unsigned) width, (unsigned) height);
            kCall(resize_func, window_data->window_width, window_data->window_height);
        }
    }

    mfb_log(MFB_LOG_DEBUG, "Toplevel configure: width=%d, height=%d", width, height);
}

//-------------------------------------
static void
handle_toplevel_close(void *data, struct xdg_toplevel *xdg_toplevel) {
    kUnused(xdg_toplevel);

    SWindowData *window_data = (SWindowData *) data;
    if (window_data) {
        bool destroy = false;

        // Keep parity with X11: ask close callback before closing.
        if (!window_data->close_func || window_data->close_func((struct mfb_window *) window_data)) {
            destroy = true;
        }

        if (destroy) {
            window_data->close = true;
        }
    }

    mfb_log(MFB_LOG_DEBUG, "Toplevel close");
}

//-------------------------------------
static void
handle_toplevel_configure_bounds(void *data, struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height) {
    kUnused(data);
    kUnused(xdg_toplevel);
    kUnused(width);
    kUnused(height);

    mfb_log(MFB_LOG_DEBUG, "Toplevel configure bounds: width=%d, height=%d", width, height);
}

//-------------------------------------
static void
handle_toplevel_wm_capabilities(void *data, struct xdg_toplevel *xdg_toplevel, struct wl_array *capabilities) {
    kUnused(data);
    kUnused(xdg_toplevel);
    kUnused(capabilities);

    mfb_log(MFB_LOG_DEBUG, "Toplevel wm capabilities");
}

//-------------------------------------
static const struct xdg_toplevel_listener toplevel_listener = {
    .configure        = handle_toplevel_configure,
    .close            = handle_toplevel_close,
    // In recent versions, these fields have disappeared
#if defined(XDG_TOPLEVEL_CONFIGURE_BOUNDS_SINCE_VERSION)
    .configure_bounds = handle_toplevel_configure_bounds,
#endif
#if defined(XDG_TOPLEVEL_WM_CAPABILITIES_SINCE_VERSION)
    .wm_capabilities  = handle_toplevel_wm_capabilities
#endif
};

//-------------------------------------
struct mfb_window *
mfb_open_ex(const char *title, unsigned width, unsigned height, unsigned flags) {
    if (width == 0 || height == 0) {
        mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: invalid window size %ux%u.", width, height);
        return NULL;
    }

    SWindowData *window_data = (SWindowData *) malloc(sizeof(SWindowData));
    if(window_data == NULL) {
        mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: failed to allocate SWindowData.");
        return NULL;
    }
    memset(window_data, 0, sizeof(SWindowData));

    SWindowData_Way *window_data_way = (SWindowData_Way *) malloc(sizeof(SWindowData_Way));
    if(window_data_way == NULL) {
        mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: failed to allocate SWindowData_Way.");
        free(window_data);
        return NULL;
    }
    memset(window_data_way, 0, sizeof(SWindowData_Way));
    window_data_way->fd = -1;
    window_data_way->current_output_scale = 1;
    window_data->specific = window_data_way;

    window_data_way->shm_format = -1u;

    window_data_way->display = wl_display_connect(NULL);
    if (!window_data_way->display) {
        mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: failed to connect to Wayland display.");
        free(window_data);
        free(window_data_way);
        return NULL;
    }
    window_data_way->registry = wl_display_get_registry(window_data_way->display);
    if (!window_data_way->registry) {
        mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: wl_display_get_registry returned NULL.");
        goto out;
    }
    wl_registry_add_listener(window_data_way->registry, &registry_listener, window_data);

    init_keycodes();

    if (wl_display_dispatch(window_data_way->display) == -1 ||
        wl_display_roundtrip(window_data_way->display) == -1) {
        mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: failed to initialize Wayland globals (dispatch/roundtrip).");
        goto out;
    }

    if (!window_data_way->decoration_manager) {
        mfb_log(MFB_LOG_WARNING, "WaylandMiniFB: zxdg_decoration_manager_v1 is unavailable; server-side decorations control may be limited.");
    }

    if (!window_data_way->fractional_scale_manager) {
        mfb_log(MFB_LOG_WARNING, "WaylandMiniFB: wp_fractional_scale_manager_v1 is unavailable; falling back to integer wl_output scale.");
    }

    if (!window_data_way->seat) {
        mfb_log(MFB_LOG_WARNING, "WaylandMiniFB: wl_seat is unavailable; keyboard and pointer input will not be available.");
    }

    if (window_data_way->output_count == 0) {
        mfb_log(MFB_LOG_WARNING, "WaylandMiniFB: wl_output is unavailable; monitor scale tracking will use fallback values.");
    }

    // did not get a format we want... meh
    if (window_data_way->shm_format == -1u) {
        mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: compositor does not expose a supported shared memory format.");
        goto out;
    }

    if (!window_data_way->compositor) {
        mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: Wayland compositor interface is unavailable.");
        goto out;
    }

    char const *xdg_rt_dir = getenv("XDG_RUNTIME_DIR");
    if (xdg_rt_dir == NULL) {
        mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: XDG_RUNTIME_DIR is not set.");
        goto out;
    }

    char shmfile[PATH_MAX];
    uint32_t ret = snprintf(shmfile, sizeof(shmfile), "%s/WaylandMiniFB-SHM-XXXXXX", xdg_rt_dir);
    if (ret >= sizeof(shmfile)) {
        mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: shared memory path exceeds PATH_MAX.");
        goto out;
    }

    window_data_way->fd = mkstemp(shmfile);
    if (window_data_way->fd == -1) {
        mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: mkstemp failed for shared memory file (%s).", strerror(errno));
        goto out;
    }
    unlink(shmfile);

    if ((size_t) width > SIZE_MAX / (size_t) height ||
        (size_t) width * (size_t) height > SIZE_MAX / sizeof(uint32_t)) {
        mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: requested window buffer size overflows size_t.");
        goto out;
    }

    size_t length_sz = sizeof(uint32_t) * (size_t) width * (size_t) height;
    if (length_sz > (size_t) INT_MAX) {
        mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: requested window buffer size exceeds Wayland pool limits.");
        goto out;
    }

    int length = (int) length_sz;

    if (ftruncate(window_data_way->fd, (off_t) length_sz) == -1) {
        mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: ftruncate failed for shared memory buffer (%s).", strerror(errno));
        goto out;
    }

    window_data_way->shm_ptr = (uint32_t *) mmap(NULL, length_sz, PROT_WRITE, MAP_SHARED, window_data_way->fd, 0);
    if (window_data_way->shm_ptr == MAP_FAILED) {
        mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: mmap failed for shared memory buffer (%s).", strerror(errno));
        goto out;
    }

    window_data_way->shm_length = length_sz;
    window_data_way->shm_pool_size = length_sz;

    window_data->window_width  = width;
    window_data->window_height = height;
    window_data->buffer_width  = width;
    window_data->buffer_height = height;
    window_data->buffer_stride = width * sizeof(uint32_t);
    calc_dst_factor(window_data, width, height);

    window_data->is_cursor_visible = true;

    window_data_way->shm_pool  = wl_shm_create_pool(window_data_way->shm, window_data_way->fd, length);
    if (window_data_way->shm_pool == NULL) {
        mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: wl_shm_create_pool failed.");
        goto out;
    }
    window_data->draw_buffer   = wl_shm_pool_create_buffer(window_data_way->shm_pool, 0,
                                    window_data->buffer_width, window_data->buffer_height,
                                    window_data->buffer_stride, window_data_way->shm_format);
    if (window_data->draw_buffer == NULL) {
        mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: wl_shm_pool_create_buffer failed.");
        goto out;
    }

    window_data_way->surface = wl_compositor_create_surface(window_data_way->compositor);
    if (!window_data_way->surface) {
        mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: failed to create Wayland surface.");
        goto out;
    }

    wl_surface_add_listener(window_data_way->surface, &surface_listener, window_data);
    if (window_data_way->fractional_scale_manager) {
        window_data_way->fractional_scale =
            wp_fractional_scale_manager_v1_get_fractional_scale(window_data_way->fractional_scale_manager,
                                                                window_data_way->surface);
        if (window_data_way->fractional_scale) {
            wp_fractional_scale_v1_add_listener(window_data_way->fractional_scale,
                                                &fractional_scale_listener,
                                                window_data);
        }
    }

    window_data_way->cursor_surface = wl_compositor_create_surface(window_data_way->compositor);
    if (!window_data_way->cursor_surface) {
        mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: failed to create Wayland cursor surface.");
        goto out;
    }

    // There should always be a shell, right?
    if (window_data_way->shell) {
        window_data_way->shell_surface = xdg_wm_base_get_xdg_surface(window_data_way->shell, window_data_way->surface);
        if (!window_data_way->shell_surface) {
            mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: failed to create xdg_surface.");
            goto out;
        }

        xdg_surface_add_listener(window_data_way->shell_surface, &shell_surface_listener, window_data);

        window_data_way->toplevel = xdg_surface_get_toplevel(window_data_way->shell_surface);
        if (!window_data_way->toplevel) {
            mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: failed to create xdg_toplevel.");
            goto out;
        }

        if (window_data_way->decoration_manager) {
            window_data_way->toplevel_decoration =
                zxdg_decoration_manager_v1_get_toplevel_decoration(window_data_way->decoration_manager, window_data_way->toplevel);
            if (window_data_way->toplevel_decoration) {
                zxdg_toplevel_decoration_v1_add_listener(window_data_way->toplevel_decoration,
                                                         &toplevel_decoration_listener, window_data);
                if (flags & WF_BORDERLESS) {
                    zxdg_toplevel_decoration_v1_set_mode(window_data_way->toplevel_decoration,
                                                         ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
                }
                else {
                    zxdg_toplevel_decoration_v1_set_mode(window_data_way->toplevel_decoration,
                                                         ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
                }
            }
        }

        window_data_way->request_fullscreen = (flags & WF_FULLSCREEN) ? 1 : 0;
        window_data_way->request_maximized =
            (!window_data_way->request_fullscreen && (flags & WF_FULLSCREEN_DESKTOP)) ? 1 : 0;
        window_data_way->startup_state_applied = 0;

        if (window_data_way->request_fullscreen || window_data_way->request_maximized) {
            xdg_toplevel_set_min_size(window_data_way->toplevel, 0, 0);
            xdg_toplevel_set_max_size(window_data_way->toplevel, 0, 0);
        }
        else {
            if (flags & WF_RESIZABLE) {
                xdg_toplevel_set_min_size(window_data_way->toplevel, 0, 0);
                xdg_toplevel_set_max_size(window_data_way->toplevel, 0, 0);
            }
            else {
                xdg_toplevel_set_min_size(window_data_way->toplevel, (int32_t) width, (int32_t) height);
                xdg_toplevel_set_max_size(window_data_way->toplevel, (int32_t) width, (int32_t) height);
            }
        }

        if (flags & WF_ALWAYS_ON_TOP) {
            mfb_log(MFB_LOG_WARNING, "WaylandMiniFB: WF_ALWAYS_ON_TOP is not supported by xdg-shell and will be ignored.");
        }

        xdg_toplevel_set_app_id(window_data_way->toplevel, MFB_STR(MFB_APP_ID));

        xdg_toplevel_set_title(window_data_way->toplevel, title ? title : "minifb");
        xdg_toplevel_add_listener(window_data_way->toplevel, &toplevel_listener, window_data);

        // Commit without a buffer to trigger initial configure event
        wl_surface_commit(window_data_way->surface);

        // Process events until we get the configure event and the surface is mapped
        while (!window_data->is_initialized) {
            if (wl_display_dispatch(window_data_way->display) == -1) {
                mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: wl_display_dispatch failed while waiting for initial configure event.");
                goto out;
            }
        }
    }
    else {
        mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: xdg_wm_base is unavailable; cannot create a toplevel surface.");
        goto out;
    }

    window_data_way->timer = mfb_timer_create();
    if (window_data_way->timer == NULL) {
        mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: failed to create frame timer.");
        goto out;
    }

    mfb_set_keyboard_callback((struct mfb_window *) window_data, keyboard_default);

    mfb_log(MFB_LOG_DEBUG, "Window created using Wayland API");

    return (struct mfb_window *) window_data;

out:
    mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: mfb_open_ex failed and is cleaning up partially initialized resources.");
    destroy(window_data);

    return NULL;
}

//-------------------------------------
// done event
//
// Notify the client when the related request is done.
//
// callback_data: request-specific data for the callback
//-------------------------------------
static void
frame_done(void *data, struct wl_callback *callback, uint32_t cookie) {
    kUnused(cookie);
    wl_callback_destroy(callback);

    *(uint32_t *)data = 1;
}

//-------------------------------------
static const struct
wl_callback_listener frame_listener = {
    .done = frame_done,
};

//-------------------------------------
mfb_update_state
mfb_update_ex(struct mfb_window *window, void *buffer, unsigned width, unsigned height) {
    uint32_t done = 0;

    if(window == NULL) {
        mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: mfb_update_ex called with a null window pointer.");
        return STATE_INVALID_WINDOW;
    }

    SWindowData *window_data = (SWindowData *) window;
    if(window_data->close) {
        mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: mfb_update_ex aborted because the window is marked for close.");
        destroy(window_data);
        return STATE_EXIT;
    }

    if(buffer == NULL) {
        mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: mfb_update_ex called with a null buffer.");
        return STATE_INVALID_BUFFER;
    }

    if (width == 0 || height == 0) {
        mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: mfb_update_ex called with invalid buffer size %ux%u.", width, height);
        return STATE_INVALID_BUFFER;
    }

    SWindowData_Way   *window_data_way = (SWindowData_Way *) window_data->specific;
    if (window_data_way == NULL) {
        mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: missing Wayland-specific window data during mfb_update_ex.");
        return STATE_INVALID_WINDOW;
    }

    if (!window_data_way->display || wl_display_get_error(window_data_way->display) != 0)
    {
        mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: invalid Wayland display state during mfb_update_ex.");
        return STATE_INTERNAL_ERROR;
    }

    if(window_data->buffer_width != width || window_data->buffer_height != height) {
        unsigned old_buffer_width = window_data->buffer_width;
        unsigned old_buffer_height = window_data->buffer_height;
        unsigned old_buffer_stride = window_data->buffer_stride;
        if ((size_t) width > SIZE_MAX / (size_t) height ||
            (size_t) width * (size_t) height > SIZE_MAX / sizeof(uint32_t)) {
            mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: resize buffer size overflows size_t.");
            return STATE_INTERNAL_ERROR;
        }
        size_t length = sizeof(uint32_t) * (size_t) width * (size_t) height;
        if (length > (size_t) INT_MAX) {
            mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: resize buffer size exceeds Wayland pool limits.");
            return STATE_INTERNAL_ERROR;
        }
        int length_i = (int) length;

        // Wayland pools are grow-only: never call wl_shm_pool_resize with a smaller size.
        if (length > window_data_way->shm_pool_size) {
            if (ftruncate(window_data_way->fd, (off_t) length) == -1)
            {
                mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: ftruncate failed while resizing shared memory buffer (%s).", strerror(errno));
                return STATE_INTERNAL_ERROR;
            }

            uint32_t *old_shm_ptr = window_data_way->shm_ptr;
            size_t old_shm_length = window_data_way->shm_length;
            uint32_t *new_shm_ptr = (uint32_t *) mmap(NULL, length, PROT_WRITE, MAP_SHARED, window_data_way->fd, 0);
            if (new_shm_ptr == MAP_FAILED)
            {
                mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: mmap failed while resizing shared memory buffer (%s).", strerror(errno));
                return STATE_INTERNAL_ERROR;
            }
            if (old_shm_ptr && old_shm_ptr != MAP_FAILED && old_shm_length > 0) {
                munmap(old_shm_ptr, old_shm_length);
            }
            window_data_way->shm_ptr = new_shm_ptr;
            window_data_way->shm_length = length;

            wl_shm_pool_resize(window_data_way->shm_pool, length_i);
            window_data_way->shm_pool_size = length;
        }

        unsigned new_buffer_stride = width * sizeof(uint32_t);
        struct wl_buffer *new_draw_buffer = wl_shm_pool_create_buffer(window_data_way->shm_pool, 0,
                                        width, height,
                                        new_buffer_stride, window_data_way->shm_format);
        if (new_draw_buffer == NULL) {
            mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: wl_shm_pool_create_buffer failed while resizing buffer.");
            window_data->buffer_width  = old_buffer_width;
            window_data->buffer_height = old_buffer_height;
            window_data->buffer_stride = old_buffer_stride;
            return STATE_INTERNAL_ERROR;
        }
        if (window_data->draw_buffer) {
            wl_buffer_destroy(window_data->draw_buffer);
        }
        window_data->draw_buffer = new_draw_buffer;
        window_data->buffer_width  = width;
        window_data->buffer_height = height;
        window_data->buffer_stride = new_buffer_stride;

        // Keep destination rectangle in window space. Changing input buffer size
        // should not recompute dst from buffer dimensions.
    }

    // update shm buffer
    if (window_data_way->shm_ptr == NULL || window_data_way->shm_ptr == MAP_FAILED) {
        mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: shared memory buffer is not mapped.");
        return STATE_INTERNAL_ERROR;
    }
    memcpy(window_data_way->shm_ptr, buffer, window_data->buffer_stride * window_data->buffer_height);

    wl_surface_attach(window_data_way->surface, (struct wl_buffer *) window_data->draw_buffer, window_data->dst_offset_x, window_data->dst_offset_y);
    wl_surface_damage(window_data_way->surface, window_data->dst_offset_x, window_data->dst_offset_y, window_data->dst_width, window_data->dst_height);
    struct wl_callback *frame_callback = wl_surface_frame(window_data_way->surface);
    if (!frame_callback) {
        mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: wl_surface_frame returned NULL.");
        return STATE_INTERNAL_ERROR;
    }
    wl_callback_add_listener(frame_callback, &frame_listener, &done);
    wl_surface_commit(window_data_way->surface);

    while (!done && window_data->close == false) {
        if (wl_display_dispatch(window_data_way->display) == -1 || wl_display_roundtrip(window_data_way->display) == -1) {
            mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: display dispatch/roundtrip failed during frame wait.");
            if (!done) {
                wl_callback_destroy(frame_callback);
            }
            return STATE_INTERNAL_ERROR;
        }
    }

    if (window_data->close) {
        if (!done) {
            wl_callback_destroy(frame_callback);
        }
        destroy(window_data);
        return STATE_EXIT;
    }

    return STATE_OK;
}

//-------------------------------------
mfb_update_state
mfb_update_events(struct mfb_window *window) {
    if(window == NULL) {
        mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: mfb_update_events called with a null window pointer.");
        return STATE_INVALID_WINDOW;
    }

    SWindowData *window_data = (SWindowData *) window;
    if(window_data->close) {
        mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: mfb_update_events aborted because the window is marked for close.");
        destroy(window_data);
        return STATE_EXIT;
    }

    SWindowData_Way   *window_data_way = (SWindowData_Way *) window_data->specific;
    if (window_data_way == NULL) {
        mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: missing Wayland-specific window data during mfb_update_events.");
        return STATE_INVALID_WINDOW;
    }
    if (!window_data_way->display || wl_display_get_error(window_data_way->display) != 0)
    {
        mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: invalid Wayland display state during mfb_update_events.");
        return STATE_INTERNAL_ERROR;
    }

    // Process already queued events first.
    if (wl_display_dispatch_pending(window_data_way->display) == -1) {
        mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: wl_display_dispatch_pending failed in mfb_update_events.");
        return STATE_INTERNAL_ERROR;
    }

    // Non-blocking read/dispatch so mfb_update_events keeps X11-like behavior.
    while (wl_display_prepare_read(window_data_way->display) != 0) {
        if (wl_display_dispatch_pending(window_data_way->display) == -1) {
            mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: wl_display_dispatch_pending failed while preparing read in mfb_update_events.");
            return STATE_INTERNAL_ERROR;
        }
    }

    if (wl_display_flush(window_data_way->display) == -1 && errno != EAGAIN) {
        wl_display_cancel_read(window_data_way->display);
        mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: wl_display_flush failed in mfb_update_events.");
        return STATE_INTERNAL_ERROR;
    }

    struct pollfd pfd;
    pfd.fd = wl_display_get_fd(window_data_way->display);
    pfd.events = POLLIN;
    pfd.revents = 0;

    int rc;
    do {
        rc = poll(&pfd, 1, 0);
    } while (rc < 0 && errno == EINTR);

    if (rc > 0) {
        if (wl_display_read_events(window_data_way->display) == -1) {
            mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: wl_display_read_events failed in mfb_update_events.");
            return STATE_INTERNAL_ERROR;
        }
        if (wl_display_dispatch_pending(window_data_way->display) == -1) {
            mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: wl_display_dispatch_pending failed after read in mfb_update_events.");
            return STATE_INTERNAL_ERROR;
        }
    }
    else {
        wl_display_cancel_read(window_data_way->display);
        if (rc < 0) {
            mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: poll failed in mfb_update_events (%s).", strerror(errno));
            return STATE_INTERNAL_ERROR;
        }
    }

    if (window_data->close) {
        mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: mfb_update_events detected close request after event dispatch.");
        destroy(window_data);
        return STATE_EXIT;
    }

    return STATE_OK;
}

//-------------------------------------
extern double   g_time_for_frame;

//-------------------------------------
bool
mfb_wait_sync(struct mfb_window *window) {
    if (window == NULL) {
        mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: mfb_wait_sync called with a null window pointer.");
        return false;
    }

    SWindowData *window_data = (SWindowData *) window;
    if (window_data->close) {
        mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: mfb_wait_sync aborted because the window is marked for close.");
        destroy(window_data);
        return false;
    }

    SWindowData_Way *window_data_specific = (SWindowData_Way *) window_data->specific;
    if (window_data_specific == NULL) {
        mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: mfb_wait_sync missing Wayland-specific window data.");
        return false;
    }

    struct wl_display *display = window_data_specific->display;
    if (display == NULL) {
        mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: mfb_wait_sync has a null Wayland display handle.");
        return false;
    }
    if (window_data_specific->timer == NULL) {
        mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: mfb_wait_sync missing frame timer state.");
        return false;
    }
    const int fd = wl_display_get_fd(display);

    // Flush outgoing requests and dispatch pending events once before pacing
    wl_display_flush(display);
    if (wl_display_dispatch_pending(display) == -1) {
        mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: wl_display_dispatch_pending failed before sync pacing.");
        return false;
    }

    if (window_data->close) {
        mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: mfb_wait_sync aborted after dispatch because the window is marked for close.");
        destroy(window_data);
        return false;
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

            // Wayland read/dispatch pattern with poll
            int prepared = wl_display_prepare_read(display);
            if (prepared == 0) {
                wl_display_flush(display);

                struct pollfd pfd;
                pfd.fd = fd;
                pfd.events = POLLIN;
                pfd.revents = 0;

                int rc;
                do {
                    rc = poll(&pfd, 1, timeout_ms);
                } while (rc < 0 && errno == EINTR);

                if (rc > 0) {
                    if (wl_display_read_events(display) == -1) {
                        mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: wl_display_read_events failed while waiting for frame sync.");
                        return false;
                    }
                }
                else {
                    wl_display_cancel_read(display);
                }
            }
            else {
                // Could not prepare read because there are pending events
                if (wl_display_dispatch_pending(display) == -1) {
                    mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: wl_display_dispatch_pending failed after prepare_read.");
                    return false;
                }
            }
        }
        else {
            sched_yield(); // or nanosleep((const struct timespec){0,0}, NULL);
        }

        if (wl_display_dispatch_pending(display) == -1) {
            mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: wl_display_dispatch_pending failed during sync loop.");
            return false;
        }

        if (window_data->close) {
            mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: mfb_wait_sync aborted during sync loop because the window is marked for close.");
            destroy(window_data);
            return false;
        }
    }

    mfb_timer_compensated_reset(window_data_specific->timer);
    return true;
}

//-------------------------------------
bool
mfb_wait_sync2(struct mfb_window *window) {
    if(window == NULL) {
        mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: mfb_wait_sync2 called with a null window pointer.");
        return false;
    }

    SWindowData *window_data = (SWindowData *) window;
    if(window_data->close) {
        mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: mfb_wait_sync2 aborted because the window is marked for close.");
        destroy(window_data);
        return false;
    }

    SWindowData_Way   *window_data_way = (SWindowData_Way *) window_data->specific;
    if (window_data_way == NULL || window_data_way->display == NULL || window_data_way->timer == NULL) {
        mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: mfb_wait_sync2 missing Wayland display or timer state.");
        return false;
    }

    double      current;
    uint32_t    millis = 1;
    while(1) {
        current = mfb_timer_now(window_data_way->timer);
        if (current >= g_time_for_frame * 0.96) {
            mfb_timer_reset(window_data_way->timer);
            return true;
        }
        else if(current >= g_time_for_frame * 0.8) {
            millis = 0;
        }

        usleep(millis * 1000);
        //sched_yield();

        if(millis == 1) {
            if (wl_display_dispatch_pending(window_data_way->display) == -1) {
                mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: wl_display_dispatch_pending failed in mfb_wait_sync2.");
                return false;
            }

            if(window_data->close) {
                mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: mfb_wait_sync2 aborted during sync loop because the window is marked for close.");
                destroy(window_data);
                return false;
            }
        }
    }

    return true;
}

//-------------------------------------
extern short int g_keycodes[512];

//-------------------------------------
void
init_keycodes(void) {
    // Clear keys
    for (size_t i = 0; i < sizeof(g_keycodes) / sizeof(g_keycodes[0]); ++i)
        g_keycodes[i] = 0;

    g_keycodes[KEY_GRAVE]      = KB_KEY_GRAVE_ACCENT;
    g_keycodes[KEY_1]          = KB_KEY_1;
    g_keycodes[KEY_2]          = KB_KEY_2;
    g_keycodes[KEY_3]          = KB_KEY_3;
    g_keycodes[KEY_4]          = KB_KEY_4;
    g_keycodes[KEY_5]          = KB_KEY_5;
    g_keycodes[KEY_6]          = KB_KEY_6;
    g_keycodes[KEY_7]          = KB_KEY_7;
    g_keycodes[KEY_8]          = KB_KEY_8;
    g_keycodes[KEY_9]          = KB_KEY_9;
    g_keycodes[KEY_0]          = KB_KEY_0;
    g_keycodes[KEY_SPACE]      = KB_KEY_SPACE;
    g_keycodes[KEY_MINUS]      = KB_KEY_MINUS;
    g_keycodes[KEY_EQUAL]      = KB_KEY_EQUAL;
    g_keycodes[KEY_Q]          = KB_KEY_Q;
    g_keycodes[KEY_W]          = KB_KEY_W;
    g_keycodes[KEY_E]          = KB_KEY_E;
    g_keycodes[KEY_R]          = KB_KEY_R;
    g_keycodes[KEY_T]          = KB_KEY_T;
    g_keycodes[KEY_Y]          = KB_KEY_Y;
    g_keycodes[KEY_U]          = KB_KEY_U;
    g_keycodes[KEY_I]          = KB_KEY_I;
    g_keycodes[KEY_O]          = KB_KEY_O;
    g_keycodes[KEY_P]          = KB_KEY_P;
    g_keycodes[KEY_LEFTBRACE]  = KB_KEY_LEFT_BRACKET;
    g_keycodes[KEY_RIGHTBRACE] = KB_KEY_RIGHT_BRACKET;
    g_keycodes[KEY_A]          = KB_KEY_A;
    g_keycodes[KEY_S]          = KB_KEY_S;
    g_keycodes[KEY_D]          = KB_KEY_D;
    g_keycodes[KEY_F]          = KB_KEY_F;
    g_keycodes[KEY_G]          = KB_KEY_G;
    g_keycodes[KEY_H]          = KB_KEY_H;
    g_keycodes[KEY_J]          = KB_KEY_J;
    g_keycodes[KEY_K]          = KB_KEY_K;
    g_keycodes[KEY_L]          = KB_KEY_L;
    g_keycodes[KEY_SEMICOLON]  = KB_KEY_SEMICOLON;
    g_keycodes[KEY_APOSTROPHE] = KB_KEY_APOSTROPHE;
    g_keycodes[KEY_Z]          = KB_KEY_Z;
    g_keycodes[KEY_X]          = KB_KEY_X;
    g_keycodes[KEY_C]          = KB_KEY_C;
    g_keycodes[KEY_V]          = KB_KEY_V;
    g_keycodes[KEY_B]          = KB_KEY_B;
    g_keycodes[KEY_N]          = KB_KEY_N;
    g_keycodes[KEY_M]          = KB_KEY_M;
    g_keycodes[KEY_COMMA]      = KB_KEY_COMMA;
    g_keycodes[KEY_DOT]        = KB_KEY_PERIOD;
    g_keycodes[KEY_SLASH]      = KB_KEY_SLASH;
    g_keycodes[KEY_BACKSLASH]  = KB_KEY_BACKSLASH;
    g_keycodes[KEY_ESC]        = KB_KEY_ESCAPE;
    g_keycodes[KEY_TAB]        = KB_KEY_TAB;
    g_keycodes[KEY_LEFTSHIFT]  = KB_KEY_LEFT_SHIFT;
    g_keycodes[KEY_RIGHTSHIFT] = KB_KEY_RIGHT_SHIFT;
    g_keycodes[KEY_LEFTCTRL]   = KB_KEY_LEFT_CONTROL;
    g_keycodes[KEY_RIGHTCTRL]  = KB_KEY_RIGHT_CONTROL;
    g_keycodes[KEY_LEFTALT]    = KB_KEY_LEFT_ALT;
    g_keycodes[KEY_RIGHTALT]   = KB_KEY_RIGHT_ALT;
    g_keycodes[KEY_LEFTMETA]   = KB_KEY_LEFT_SUPER;
    g_keycodes[KEY_RIGHTMETA]  = KB_KEY_RIGHT_SUPER;
    g_keycodes[KEY_MENU]       = KB_KEY_MENU;
    g_keycodes[KEY_NUMLOCK]    = KB_KEY_NUM_LOCK;
    g_keycodes[KEY_CAPSLOCK]   = KB_KEY_CAPS_LOCK;
    g_keycodes[KEY_PRINT]      = KB_KEY_PRINT_SCREEN;
    g_keycodes[KEY_SCROLLLOCK] = KB_KEY_SCROLL_LOCK;
    g_keycodes[KEY_PAUSE]      = KB_KEY_PAUSE;
    g_keycodes[KEY_DELETE]     = KB_KEY_DELETE;
    g_keycodes[KEY_BACKSPACE]  = KB_KEY_BACKSPACE;
    g_keycodes[KEY_ENTER]      = KB_KEY_ENTER;
    g_keycodes[KEY_HOME]       = KB_KEY_HOME;
    g_keycodes[KEY_END]        = KB_KEY_END;
    g_keycodes[KEY_PAGEUP]     = KB_KEY_PAGE_UP;
    g_keycodes[KEY_PAGEDOWN]   = KB_KEY_PAGE_DOWN;
    g_keycodes[KEY_INSERT]     = KB_KEY_INSERT;
    g_keycodes[KEY_LEFT]       = KB_KEY_LEFT;
    g_keycodes[KEY_RIGHT]      = KB_KEY_RIGHT;
    g_keycodes[KEY_DOWN]       = KB_KEY_DOWN;
    g_keycodes[KEY_UP]         = KB_KEY_UP;
    g_keycodes[KEY_F1]         = KB_KEY_F1;
    g_keycodes[KEY_F2]         = KB_KEY_F2;
    g_keycodes[KEY_F3]         = KB_KEY_F3;
    g_keycodes[KEY_F4]         = KB_KEY_F4;
    g_keycodes[KEY_F5]         = KB_KEY_F5;
    g_keycodes[KEY_F6]         = KB_KEY_F6;
    g_keycodes[KEY_F7]         = KB_KEY_F7;
    g_keycodes[KEY_F8]         = KB_KEY_F8;
    g_keycodes[KEY_F9]         = KB_KEY_F9;
    g_keycodes[KEY_F10]        = KB_KEY_F10;
    g_keycodes[KEY_F11]        = KB_KEY_F11;
    g_keycodes[KEY_F12]        = KB_KEY_F12;
    g_keycodes[KEY_F13]        = KB_KEY_F13;
    g_keycodes[KEY_F14]        = KB_KEY_F14;
    g_keycodes[KEY_F15]        = KB_KEY_F15;
    g_keycodes[KEY_F16]        = KB_KEY_F16;
    g_keycodes[KEY_F17]        = KB_KEY_F17;
    g_keycodes[KEY_F18]        = KB_KEY_F18;
    g_keycodes[KEY_F19]        = KB_KEY_F19;
    g_keycodes[KEY_F20]        = KB_KEY_F20;
    g_keycodes[KEY_F21]        = KB_KEY_F21;
    g_keycodes[KEY_F22]        = KB_KEY_F22;
    g_keycodes[KEY_F23]        = KB_KEY_F23;
    g_keycodes[KEY_F24]        = KB_KEY_F24;
    g_keycodes[KEY_KPSLASH]    = KB_KEY_KP_DIVIDE;
    g_keycodes[KEY_KPDOT]      = KB_KEY_KP_MULTIPLY;
    g_keycodes[KEY_KPMINUS]    = KB_KEY_KP_SUBTRACT;
    g_keycodes[KEY_KPPLUS]     = KB_KEY_KP_ADD;
    g_keycodes[KEY_KP0]        = KB_KEY_KP_0;
    g_keycodes[KEY_KP1]        = KB_KEY_KP_1;
    g_keycodes[KEY_KP2]        = KB_KEY_KP_2;
    g_keycodes[KEY_KP3]        = KB_KEY_KP_3;
    g_keycodes[KEY_KP4]        = KB_KEY_KP_4;
    g_keycodes[KEY_KP5]        = KB_KEY_KP_5;
    g_keycodes[KEY_KP6]        = KB_KEY_KP_6;
    g_keycodes[KEY_KP7]        = KB_KEY_KP_7;
    g_keycodes[KEY_KP8]        = KB_KEY_KP_8;
    g_keycodes[KEY_KP9]        = KB_KEY_KP_9;
    g_keycodes[KEY_KPCOMMA]    = KB_KEY_KP_DECIMAL;
    g_keycodes[KEY_KPEQUAL]    = KB_KEY_KP_EQUAL;
    g_keycodes[KEY_KPENTER]    = KB_KEY_KP_ENTER;
}

//-------------------------------------
bool
mfb_set_viewport(struct mfb_window *window, unsigned offset_x, unsigned offset_y, unsigned width, unsigned height) {
    SWindowData *window_data = (SWindowData *) window;
    if (window_data == NULL) {
        mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: mfb_set_viewport called with a null window pointer.");
        return false;
    }

    if (offset_x > window_data->window_width || width > (window_data->window_width - offset_x)) {
        mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: viewport exceeds window width (offset_x=%u, width=%u, window_width=%u).",
                offset_x, width, window_data->window_width);
        return false;
    }

    if (offset_y > window_data->window_height || height > (window_data->window_height - offset_y)) {
        mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: viewport exceeds window height (offset_y=%u, height=%u, window_height=%u).",
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
    float x = 1.0f, y = 1.0f;

    if(window != NULL) {
        SWindowData *window_data = (SWindowData *) window;
        SWindowData_Way *window_data_way = (SWindowData_Way *) window_data->specific;
        if (window_data_way) {
            if (window_data_way->preferred_scale_120 > 0) {
                float scale = (float) window_data_way->preferred_scale_120 / WAYLAND_FRACTIONAL_SCALE_DENOMINATOR;
                if (scale > 0.0f) {
                    x = scale;
                    y = scale;
                }
            }
            else if (window_data_way->current_output_scale > 0) {
                x = (float) window_data_way->current_output_scale;
                y = (float) window_data_way->current_output_scale;
            }
        }
    }

    if (scale_x) {
        *scale_x = x;
        if(*scale_x == 0) {
            *scale_x = 1.0f;
        }
    }

    if (scale_y) {
        *scale_y = y;
        if (*scale_y == 0) {
            *scale_y = 1.0f;
        }
    }
}

//-------------------------------------
void
mfb_show_cursor(struct mfb_window *window, bool show) {
    SWindowData *window_data = (SWindowData *) window;
    if (window_data == NULL) {
        mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: mfb_show_cursor called with a null window pointer.");
        return;
    }

    SWindowData_Way *window_data_way = (SWindowData_Way *) window_data->specific;
    if (window_data_way == NULL) {
        mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: mfb_show_cursor missing Wayland-specific window data.");
        return;
    }

    // Keep requested visibility state even if we can't apply it immediately.
    window_data->is_cursor_visible = show;

    struct wl_pointer *pointer = window_data_way->pointer;
    struct wl_surface *cursor_surface = window_data_way->cursor_surface;
    if (pointer == NULL) {
        mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: mfb_show_cursor cannot update cursor because wl_pointer is null.");
        return;
    }

    if (!window_data_way->pointer_serial_valid) {
        return;
    }

    uint32_t serial = window_data_way->pointer_serial;
    if (show) {
        struct wl_cursor *cursor = window_data_way->default_cursor;
        if (cursor == NULL || cursor->image_count == 0 || cursor_surface == NULL) {
            mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: default cursor data is incomplete (cursor=%p, image_count=%u, cursor_surface=%p).",
                    (void *) cursor, cursor ? cursor->image_count : 0, (void *) cursor_surface);
            return;
        }

        struct wl_cursor_image *cursor_image = cursor->images[0];
        struct wl_buffer *cursor_image_buffer = wl_cursor_image_get_buffer(cursor_image);
        if (cursor_image_buffer == NULL) {
            mfb_log(MFB_LOG_ERROR, "WaylandMiniFB: wl_cursor_image_get_buffer returned NULL.");
            return;
        }

        wl_pointer_set_cursor(pointer, serial, cursor_surface, cursor_image->hotspot_x, cursor_image->hotspot_y);
        wl_surface_attach(cursor_surface, cursor_image_buffer, 0, 0);
        wl_surface_damage(cursor_surface, 0, 0, cursor_image->width, cursor_image->height);
        wl_surface_commit(cursor_surface);
    }
    else {
        wl_pointer_set_cursor(pointer, serial, NULL, 0, 0);
    }
}

