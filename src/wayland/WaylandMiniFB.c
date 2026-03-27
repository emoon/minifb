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
#include <xkbcommon/xkbcommon-compose.h>

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

#define WAYLAND_FRACTIONAL_SCALE_DENOMINATOR 120.0f

//-------------------------------------
void init_keycodes();

// Forward declarations for functions used in destroy/configure
// but defined later in the file.
static void slot_destroy(SWaylandBufferSlot *slot);
static bool slot_ensure_buffer(SWaylandBufferSlot *slot, struct wl_shm *shm, uint32_t shm_format, uint32_t surface_w, uint32_t surface_h);

//-------------------------------------
static void
update_mod_keys_from_xkb(SWindowData *window_data, SWindowData_Way *window_data_specific) {
    if (window_data == NULL || window_data_specific == NULL || window_data_specific->xkb_state == NULL) {
        return;
    }

    window_data->mod_keys = 0;
    if (xkb_state_mod_name_is_active(window_data_specific->xkb_state, XKB_MOD_NAME_SHIFT, XKB_STATE_MODS_EFFECTIVE) > 0) {
        window_data->mod_keys |= MFB_KB_MOD_SHIFT;
    }

    if (xkb_state_mod_name_is_active(window_data_specific->xkb_state, XKB_MOD_NAME_CTRL, XKB_STATE_MODS_EFFECTIVE) > 0) {
        window_data->mod_keys |= MFB_KB_MOD_CONTROL;
    }

    if (xkb_state_mod_name_is_active(window_data_specific->xkb_state, XKB_MOD_NAME_ALT, XKB_STATE_MODS_EFFECTIVE) > 0) {
        window_data->mod_keys |= MFB_KB_MOD_ALT;
    }

    if (xkb_state_mod_name_is_active(window_data_specific->xkb_state, XKB_MOD_NAME_LOGO, XKB_STATE_MODS_EFFECTIVE) > 0) {
        window_data->mod_keys |= MFB_KB_MOD_SUPER;
    }
}

//-------------------------------------
static void
destroy_window_data(SWindowData *window_data) {
    if (window_data == NULL) {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: destroy_window_data called with a null window pointer.");
        return;
    }

    release_cpp_stub((struct mfb_window *) window_data);

    SWindowData_Way   *window_data_specific = (SWindowData_Way *) window_data->specific;
    if (window_data_specific != NULL) {
        // Destroy all buffer slots (pool, buffer, mmap, fd per slot).
        for (int i = 0; i < WAYLAND_BUFFER_SLOTS; ++i) {
            slot_destroy(&window_data_specific->slots[i]);
        }

        if (window_data_specific->xkb_compose_state) {
            xkb_compose_state_unref(window_data_specific->xkb_compose_state);
        }

        if (window_data_specific->xkb_compose_table) {
            xkb_compose_table_unref(window_data_specific->xkb_compose_table);
        }

        if (window_data_specific->xkb_state) {
            xkb_state_unref(window_data_specific->xkb_state);
        }

        if (window_data_specific->xkb_keymap) {
            xkb_keymap_unref(window_data_specific->xkb_keymap);
        }

        if (window_data_specific->xkb_context) {
            xkb_context_unref(window_data_specific->xkb_context);
        }

        mfb_timer_destroy(window_data_specific->timer);
        memset(window_data_specific, 0, sizeof(SWindowData_Way));
        free(window_data_specific);
    }
    memset(window_data, 0, sizeof(SWindowData));
    free(window_data);
}

//-------------------------------------
static void
destroy(SWindowData *window_data) {
    if (window_data == NULL) {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: destroy called with a null window pointer.");
        return;
    }

    SWindowData_Way *window_data_specific = (SWindowData_Way *) window_data->specific;
    if (window_data_specific == NULL) {
        destroy_window_data(window_data);
        return;
    }

    // Destroy protocol objects before disconnecting the display.
    // Order: extensions first, then core objects, then connection.

    if (window_data_specific->toplevel_decoration) {
        zxdg_toplevel_decoration_v1_destroy(window_data_specific->toplevel_decoration);
        window_data_specific->toplevel_decoration = NULL;
    }

    if (window_data_specific->toplevel) {
        xdg_toplevel_destroy(window_data_specific->toplevel);
        window_data_specific->toplevel = NULL;
    }

    if (window_data_specific->shell_surface) {
        xdg_surface_destroy(window_data_specific->shell_surface);
        window_data_specific->shell_surface = NULL;
    }

    if (window_data_specific->shell) {
        xdg_wm_base_destroy(window_data_specific->shell);
        window_data_specific->shell = NULL;
    }

    if (window_data_specific->decoration_manager) {
        zxdg_decoration_manager_v1_destroy(window_data_specific->decoration_manager);
        window_data_specific->decoration_manager = NULL;
    }

    if (window_data_specific->fractional_scale) {
        wp_fractional_scale_v1_destroy(window_data_specific->fractional_scale);
        window_data_specific->fractional_scale = NULL;
    }

    if (window_data_specific->fractional_scale_manager) {
        wp_fractional_scale_manager_v1_destroy(window_data_specific->fractional_scale_manager);
        window_data_specific->fractional_scale_manager = NULL;
    }

    if (window_data_specific->surface) {
        wl_surface_destroy(window_data_specific->surface);
        window_data_specific->surface = NULL;
    }

    // Destroy all buffer slots (protocol objects + resources) before display disconnect.
    for (int i = 0; i < WAYLAND_BUFFER_SLOTS; ++i) {
        slot_destroy(&window_data_specific->slots[i]);
    }

    if (window_data_specific->cursor_surface) {
        wl_surface_destroy(window_data_specific->cursor_surface);
        window_data_specific->cursor_surface = NULL;
    }

    if (window_data_specific->cursor_theme) {
        wl_cursor_theme_destroy(window_data_specific->cursor_theme);
        window_data_specific->cursor_theme = NULL;
    }

    if (window_data_specific->shm) {
        wl_shm_destroy(window_data_specific->shm);
        window_data_specific->shm = NULL;
    }

    for (uint32_t i = 0; i < window_data_specific->output_count; ++i) {
        if (window_data_specific->outputs[i]) {
            wl_output_destroy(window_data_specific->outputs[i]);
            window_data_specific->outputs[i] = NULL;
        }
    }

    window_data_specific->output_count = 0;
    window_data_specific->integer_output_scale = 1;

    if (window_data_specific->compositor) {
        wl_compositor_destroy(window_data_specific->compositor);
        window_data_specific->compositor = NULL;
    }

    if (window_data_specific->keyboard) {
        wl_keyboard_destroy(window_data_specific->keyboard);
        window_data_specific->keyboard = NULL;
    }

    if (window_data_specific->pointer) {
        wl_pointer_destroy(window_data_specific->pointer);
        window_data_specific->pointer = NULL;
    }

    if (window_data_specific->seat) {
        wl_seat_destroy(window_data_specific->seat);
        window_data_specific->seat = NULL;
    }

    if (window_data_specific->registry) {
        wl_registry_destroy(window_data_specific->registry);
        window_data_specific->registry = NULL;
    }

    if (window_data_specific->display) {
        wl_display_disconnect(window_data_specific->display);
        window_data_specific->display = NULL;
    }

    destroy_window_data(window_data);
}

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

    *index += 1;
    return false;
}

//-------------------------------------
static const char *
get_compose_locale(void) {
    const char *locale;
    locale = getenv("LC_ALL");
    if (locale && *locale) return locale;
    locale = getenv("LC_CTYPE");
    if (locale && *locale) return locale;
    locale = getenv("LANG");
    if (locale && *locale) return locale;
    return "C";
}

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
    SWindowData_Way *window_data_specific = window_data ? (SWindowData_Way *) window_data->specific : NULL;
    kUnused(keyboard);

    if (window_data_specific == NULL) {
        if (fd >= 0) {
            close(fd);
        }
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: keyboard_keymap received without valid window state.");
        return;
    }

    if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1 || fd < 0 || size == 0) {
        if (fd >= 0) {
            close(fd);
        }
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: unsupported or invalid keymap payload.");
        return;
    }

    char *keymap_data = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (keymap_data == MAP_FAILED) {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: failed to map Wayland keymap (%s).", strerror(errno));
        return;
    }

    if (window_data_specific->xkb_context == NULL) {
        window_data_specific->xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
        if (window_data_specific->xkb_context == NULL) {
            munmap(keymap_data, size);
            MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: xkb_context_new failed.");
            return;
        }
    }

    struct xkb_keymap *keymap = xkb_keymap_new_from_string(window_data_specific->xkb_context,
                                                            keymap_data,
                                                            XKB_KEYMAP_FORMAT_TEXT_V1,
                                                            XKB_KEYMAP_COMPILE_NO_FLAGS);
    munmap(keymap_data, size);
    if (keymap == NULL) {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: xkb_keymap_new_from_string failed.");
        return;
    }

    struct xkb_state *state = xkb_state_new(keymap);
    if (state == NULL) {
        xkb_keymap_unref(keymap);
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: xkb_state_new failed.");
        return;
    }

    if (window_data_specific->xkb_state) {
        xkb_state_unref(window_data_specific->xkb_state);
    }
    if (window_data_specific->xkb_keymap) {
        xkb_keymap_unref(window_data_specific->xkb_keymap);
    }

    window_data_specific->xkb_keymap = keymap;
    window_data_specific->xkb_state = state;

    // (Re)create compose table and state for dead-key support
    if (window_data_specific->xkb_compose_state) {
        xkb_compose_state_unref(window_data_specific->xkb_compose_state);
        window_data_specific->xkb_compose_state = NULL;
    }
    if (window_data_specific->xkb_compose_table) {
        xkb_compose_table_unref(window_data_specific->xkb_compose_table);
        window_data_specific->xkb_compose_table = NULL;
    }

    const char *locale = get_compose_locale();
    struct xkb_compose_table *compose_table = xkb_compose_table_new_from_locale(
        window_data_specific->xkb_context, locale, XKB_COMPOSE_COMPILE_NO_FLAGS);
    if (compose_table != NULL) {
        struct xkb_compose_state *compose_state = xkb_compose_state_new(
            compose_table, XKB_COMPOSE_STATE_NO_FLAGS);
        if (compose_state != NULL) {
            window_data_specific->xkb_compose_table = compose_table;
            window_data_specific->xkb_compose_state = compose_state;
        }
        else {
            xkb_compose_table_unref(compose_table);
            MFB_LOG(MFB_LOG_WARNING, "xkb_compose_state_new failed; dead keys will not work");
        }
    }
    else {
        MFB_LOG(MFB_LOG_DEBUG, "xkb_compose_table_new_from_locale('%s') failed; dead keys will not work", locale);
    }
}

//-------------------------------------
// Clear all keyboard state: key_status, mod_keys, and xkb_state.
// Used on keyboard_leave and when the seat loses keyboard capability.
//-------------------------------------
static void
reset_keyboard_state(SWindowData *window_data, SWindowData_Way *window_data_specific) {
    memset(window_data->key_status, 0, sizeof(window_data->key_status));
    window_data->mod_keys = 0;

    if (window_data_specific != NULL
        && window_data_specific->xkb_keymap != NULL
        && window_data_specific->xkb_state != NULL) {
        struct xkb_state *fresh = xkb_state_new(window_data_specific->xkb_keymap);
        if (fresh != NULL) {
            xkb_state_unref(window_data_specific->xkb_state);
            window_data_specific->xkb_state = fresh;
        }
    }

    if (window_data_specific != NULL) {
        if (window_data_specific->xkb_compose_state != NULL) {
            xkb_compose_state_reset(window_data_specific->xkb_compose_state);
        }
        window_data_specific->compose_sequence_count = 0;
    }
}

//-------------------------------------
// Rebuild keyboard state from the compositor's pressed-key list.
// Assumes reset_keyboard_state was called first.
// Does not emit synthetic keyboard callbacks — only synchronizes state.
//-------------------------------------
static void
rebuild_keyboard_state_from_keys(SWindowData *window_data, SWindowData_Way *window_data_specific, struct wl_array *keys) {
    if (keys == NULL) {
        return;
    }

    uint32_t *key;
    wl_array_for_each(key, keys) {
        if (*key < 512) {
            mfb_key key_code = (mfb_key) g_keycodes[*key];
            if (key_code != MFB_KB_KEY_UNKNOWN && key_code >= 0 && key_code < MFB_MAX_KEYS) {
                window_data->key_status[key_code] = true;
            }
            // Update xkb_state so mod_keys reflects held modifiers.
            if (window_data_specific != NULL && window_data_specific->xkb_state != NULL) {
                xkb_keycode_t xkb_keycode = (xkb_keycode_t) *key + 8;
                xkb_state_update_key(window_data_specific->xkb_state, xkb_keycode, XKB_KEY_DOWN);
            }
        }
    }

    if (window_data_specific != NULL) {
        update_mod_keys_from_xkb(window_data, window_data_specific);
    }
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

    SWindowData *window_data = (SWindowData *) data;
    SWindowData_Way *window_data_specific = window_data ? (SWindowData_Way *) window_data->specific : NULL;

    window_data->is_active = true;
    reset_keyboard_state(window_data, window_data_specific);
    rebuild_keyboard_state_from_keys(window_data, window_data_specific, keys);
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
    SWindowData_Way *window_data_specific = window_data ? (SWindowData_Way *) window_data->specific : NULL;

    window_data->is_active = false;
    reset_keyboard_state(window_data, window_data_specific);
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
    SWindowData_Way *window_data_specific = (SWindowData_Way *) window_data->specific;
    if (key < 512) {
        mfb_key key_code = (mfb_key) g_keycodes[key];
        bool   is_pressed = (bool) (state == WL_KEYBOARD_KEY_STATE_PRESSED);
        if (window_data_specific && window_data_specific->xkb_state) {
            xkb_keycode_t xkb_keycode = (xkb_keycode_t) key + 8;
            xkb_state_update_key(window_data_specific->xkb_state, xkb_keycode, is_pressed ? XKB_KEY_DOWN : XKB_KEY_UP);
            update_mod_keys_from_xkb(window_data, window_data_specific);
            if (is_pressed) {
                if (window_data_specific->xkb_compose_state) {
                    xkb_keysym_t keysym = xkb_state_key_get_one_sym(window_data_specific->xkb_state, xkb_keycode);
                    xkb_compose_state_feed(window_data_specific->xkb_compose_state, keysym);
                    enum xkb_compose_status status = xkb_compose_state_get_status(window_data_specific->xkb_compose_state);
                    if (status == XKB_COMPOSE_COMPOSED) {
                        bool emitted = false;
                        xkb_keysym_t composed_sym = xkb_compose_state_get_one_sym(window_data_specific->xkb_compose_state);
                        if (composed_sym != XKB_KEY_NoSymbol) {
                            uint32_t codepoint = xkb_keysym_to_utf32(composed_sym);
                            if (codepoint != 0) {
                                kCall(char_input_func, codepoint);
                                emitted = true;
                            }
                        }
                        if (emitted == false) {
                            // Fallback: compose result has no keysym, decode UTF-8
                            char buf[64];
                            int len = xkb_compose_state_get_utf8(window_data_specific->xkb_compose_state, buf, sizeof(buf));
                            if (len > 0) {
                                size_t actual = ((size_t) len < sizeof(buf) - 1) ? (size_t) len : sizeof(buf) - 1;
                                size_t idx = 0;
                                uint32_t cp;
                                while (utf8_decode_next((const unsigned char *) buf, actual, &idx, &cp)) {
                                    if (cp != 0) {
                                        kCall(char_input_func, cp);
                                    }
                                }
                            }
                        }
                        window_data_specific->compose_sequence_count = 0;
                        xkb_compose_state_reset(window_data_specific->xkb_compose_state);
                    }
                    else if (status == XKB_COMPOSE_CANCELLED) {
                        // Replay buffered keycodes + cancelling key as individual characters
                        for (uint8_t i = 0; i < window_data_specific->compose_sequence_count; ++i) {
                            uint32_t codepoint = xkb_state_key_get_utf32(window_data_specific->xkb_state, window_data_specific->compose_sequence[i]);
                            if (codepoint != 0) {
                                kCall(char_input_func, codepoint);
                            }
                        }
                        uint32_t codepoint = xkb_state_key_get_utf32(window_data_specific->xkb_state, xkb_keycode);
                        if (codepoint != 0) {
                            kCall(char_input_func, codepoint);
                        }
                        window_data_specific->compose_sequence_count = 0;
                    }
                    else if (status == XKB_COMPOSE_COMPOSING) {
                        // Dead key pending — buffer keycode, don't emit
                        if (window_data_specific->compose_sequence_count < 8) {
                            window_data_specific->compose_sequence[window_data_specific->compose_sequence_count++] = xkb_keycode;
                        }
                    }
                    else if (status == XKB_COMPOSE_NOTHING) {
                        uint32_t codepoint = xkb_state_key_get_utf32(window_data_specific->xkb_state, xkb_keycode);
                        if (codepoint != 0) {
                            kCall(char_input_func, codepoint);
                        }
                    }
                }
                else {
                    uint32_t codepoint = xkb_state_key_get_utf32(window_data_specific->xkb_state, xkb_keycode);
                    if (codepoint != 0) {
                        kCall(char_input_func, codepoint);
                    }
                }
            }
        }

        else {
            switch (key_code) {
                case MFB_KB_KEY_LEFT_SHIFT:
                case MFB_KB_KEY_RIGHT_SHIFT:
                    if (is_pressed)
                        window_data->mod_keys |= MFB_KB_MOD_SHIFT;
                    else
                        window_data->mod_keys &= ~MFB_KB_MOD_SHIFT;
                    break;

                case MFB_KB_KEY_LEFT_CONTROL:
                case MFB_KB_KEY_RIGHT_CONTROL:
                    if (is_pressed)
                        window_data->mod_keys |= MFB_KB_MOD_CONTROL;
                    else
                        window_data->mod_keys &= ~MFB_KB_MOD_CONTROL;
                    break;

                case MFB_KB_KEY_LEFT_ALT:
                case MFB_KB_KEY_RIGHT_ALT:
                    if (is_pressed)
                        window_data->mod_keys |= MFB_KB_MOD_ALT;
                    else
                        window_data->mod_keys &= ~MFB_KB_MOD_ALT;
                    break;

                case MFB_KB_KEY_LEFT_SUPER:
                case MFB_KB_KEY_RIGHT_SUPER:
                    if (is_pressed)
                        window_data->mod_keys |= MFB_KB_MOD_SUPER;
                    else
                        window_data->mod_keys &= ~MFB_KB_MOD_SUPER;
                    break;
            }
        }

        if (key_code != MFB_KB_KEY_UNKNOWN && key_code >= 0 && key_code < MFB_MAX_KEYS) {
            window_data->key_status[key_code] = is_pressed;
            kCall(keyboard_func, key_code, (mfb_key_mod) window_data->mod_keys, is_pressed);
        }
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
    SWindowData_Way *window_data_specific = window_data ? (SWindowData_Way *) window_data->specific : NULL;
    if (window_data_specific && window_data_specific->xkb_state) {
        xkb_state_update_mask(window_data_specific->xkb_state,
                              mods_depressed,
                              mods_latched,
                              mods_locked,
                              0, 0,
                              group);
        update_mod_keys_from_xkb(window_data, window_data_specific);
    }
}

#if defined(WL_KEYBOARD_REPEAT_INFO_SINCE_VERSION)

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

#endif

//-------------------------------------
static const struct
wl_keyboard_listener keyboard_listener = {
    .keymap      = keyboard_keymap,
    .enter       = keyboard_enter,
    .leave       = keyboard_leave,
    .key         = keyboard_key,
    .modifiers   = keyboard_modifiers,
#if defined(WL_KEYBOARD_REPEAT_INFO_SINCE_VERSION)
    .repeat_info = keyboard_repeat_info,
#endif
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
    struct wl_buffer *buffer;
    struct wl_cursor_image *image;

    SWindowData *window_data = (SWindowData *) data;
    if (window_data == NULL)
        return;
    SWindowData_Way *window_data_specific = (SWindowData_Way *) window_data->specific;
    if (window_data_specific == NULL)
        return;
    window_data_specific->pointer_serial = serial;
    window_data_specific->pointer_enter_serial = serial;
    window_data_specific->pointer_serial_valid = 1;

    // Synchronize stored mouse position from the enter event coordinates.
    window_data->mouse_pos_x = wl_fixed_to_int(sx);
    window_data->mouse_pos_y = wl_fixed_to_int(sy);

    if (window_data->is_cursor_visible) {
        if (window_data_specific->default_cursor == NULL ||
            window_data_specific->default_cursor->image_count == 0 ||
            window_data_specific->cursor_surface == NULL) {
            return;
        }
        image  = window_data_specific->default_cursor->images[0];
        buffer = wl_cursor_image_get_buffer(image);
        if (buffer == NULL) {
            return;
        }

        wl_pointer_set_cursor(pointer, serial, window_data_specific->cursor_surface, image->hotspot_x, image->hotspot_y);
        wl_surface_attach(window_data_specific->cursor_surface, buffer, 0, 0);
#if defined(WL_SURFACE_DAMAGE_BUFFER_SINCE_VERSION)
        if (window_data_specific->compositor_version >= WL_SURFACE_DAMAGE_BUFFER_SINCE_VERSION) {
            wl_surface_damage_buffer(window_data_specific->cursor_surface, 0, 0, image->width, image->height);
        }
        else
#endif
        {
            wl_surface_damage(window_data_specific->cursor_surface, 0, 0, image->width, image->height);
        }
        wl_surface_commit(window_data_specific->cursor_surface);
    }
    else {
        wl_pointer_set_cursor(pointer, serial, NULL, 0, 0);
    }

    //MFB_LOG(MFB_LOG_DEBUG, "Pointer entered surface %p at %d %d (serial: %d)", surface, sx, sy, serial);
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
    SWindowData_Way *window_data_specific = window_data ? (SWindowData_Way *) window_data->specific : NULL;
    if (window_data_specific) {
        window_data_specific->pointer_serial_valid = 0;
    }

    //MFB_LOG(MFB_LOG_DEBUG, "Pointer left surface %p (serial: %d)", surface, serial);
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

    //MFB_LOG(MFB_LOG_DEBUG, "Pointer moved at %f %f", sx / 256.0f, sy / 256.0f);
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

    //MFB_LOG(MFB_LOG_DEBUG, "Pointer button '%d'(%d)", button, state);
    SWindowData *window_data = (SWindowData *) data;
    SWindowData_Way *window_data_specific = window_data ? (SWindowData_Way *) window_data->specific : NULL;
    if (window_data_specific) {
        window_data_specific->pointer_serial = serial;
        window_data_specific->pointer_serial_valid = 1;
    }
    uint32_t mapped = button - BTN_MOUSE + 1;
    if (mapped > MFB_MOUSE_BTN_7) {
        MFB_LOG(MFB_LOG_WARNING, "Mouse button %u exceeds MFB_MOUSE_BTN_7; ignoring.", mapped);
    }
    else {
        window_data->mouse_button_status[mapped] = (state == 1);
        kCall(mouse_btn_func, (mfb_mouse_button) mapped, (mfb_key_mod) window_data->mod_keys, state == 1);
    }

    //MFB_LOG(MFB_LOG_DEBUG, "Pointer button %x, state %x (serial: %d)", button, state, serial);
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

    //MFB_LOG(MFB_LOG_DEBUG, "Pointer handle axis: axis: %d (0x%x)", axis, value);
    SWindowData *window_data = (SWindowData *) data;
    if (axis == 0) {
        window_data->mouse_wheel_y = -(value / 256.0f);
        kCall(mouse_wheel_func, (mfb_key_mod) window_data->mod_keys, 0.0f, window_data->mouse_wheel_y);
    }
    else if (axis == 1) {
        window_data->mouse_wheel_x = -(value / 256.0f);
        kCall(mouse_wheel_func, (mfb_key_mod) window_data->mod_keys, window_data->mouse_wheel_x, 0.0f);
    }
}

#if defined(WL_POINTER_FRAME_SINCE_VERSION)

//-------------------------------------
static void
frame(void *data, struct wl_pointer *pointer) {
    kUnused(data);
    kUnused(pointer);
}

#endif

#if defined(WL_POINTER_AXIS_SOURCE_SINCE_VERSION)

//-------------------------------------
static void
axis_source(void *data, struct wl_pointer *pointer, uint32_t axis_source) {
    kUnused(data);
    kUnused(pointer);
    kUnused(axis_source);
}

#endif

#if defined(WL_POINTER_AXIS_STOP_SINCE_VERSION)

//-------------------------------------
static void
axis_stop(void *data, struct wl_pointer *pointer, uint32_t time, uint32_t axis) {
    kUnused(data);
    kUnused(pointer);
    kUnused(time);
    kUnused(axis);
}

#endif

#if defined(WL_POINTER_AXIS_DISCRETE_SINCE_VERSION)

//-------------------------------------
static void
axis_discrete(void *data, struct wl_pointer *pointer, uint32_t axis, int32_t discrete) {
    kUnused(data);
    kUnused(pointer);
    kUnused(axis);
    kUnused(discrete);
}

#endif

//-------------------------------------
static const struct
wl_pointer_listener pointer_listener = {
    .enter         = pointer_enter,
    .leave         = pointer_leave,
    .motion        = pointer_motion,
    .button        = pointer_button,
    .axis          = pointer_axis,
#if defined(WL_POINTER_FRAME_SINCE_VERSION)
    .frame         = frame,
#endif
#if defined(WL_POINTER_AXIS_SOURCE_SINCE_VERSION)
    .axis_source   = axis_source,
#endif
#if defined(WL_POINTER_AXIS_STOP_SINCE_VERSION)
    .axis_stop     = axis_stop,
#endif
#if defined(WL_POINTER_AXIS_DISCRETE_SINCE_VERSION)
    .axis_discrete = axis_discrete,
#endif
};

//-------------------------------------
static void
seat_capabilities(void *data, struct wl_seat *seat, enum wl_seat_capability caps) {
    kUnused(data);

    SWindowData       *window_data     = (SWindowData *) data;
    SWindowData_Way   *window_data_specific = (SWindowData_Way *) window_data->specific;
    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !window_data_specific->keyboard) {
        window_data_specific->keyboard = wl_seat_get_keyboard(seat);
        if (window_data_specific->keyboard) {
            wl_keyboard_add_listener(window_data_specific->keyboard, &keyboard_listener, window_data);
        }
    }

    else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && window_data_specific->keyboard) {
        wl_keyboard_destroy(window_data_specific->keyboard);
        window_data_specific->keyboard = NULL;
        reset_keyboard_state(window_data, window_data_specific);
    }

    if ((caps & WL_SEAT_CAPABILITY_POINTER) && !window_data_specific->pointer) {
        window_data_specific->pointer = wl_seat_get_pointer(seat);
        if (window_data_specific->pointer) {
            wl_pointer_add_listener(window_data_specific->pointer, &pointer_listener, window_data);
        }
    }
    else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && window_data_specific->pointer) {
        wl_pointer_destroy(window_data_specific->pointer);
        window_data_specific->pointer = NULL;
    }
}

#if defined(WL_SEAT_NAME_SINCE_VERSION)

//-------------------------------------
static void
seat_name(void *data, struct wl_seat *seat, const char *name) {
    kUnused(data);
    kUnused(seat);

    MFB_LOG(MFB_LOG_DEBUG, "Seat '%s'", name);
}

#endif

//-------------------------------------
static const struct
wl_seat_listener seat_listener = {
    .capabilities = seat_capabilities,
#if defined(WL_SEAT_NAME_SINCE_VERSION)
    .name         = seat_name,
#endif
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
    SWindowData_Way   *window_data_specific = (SWindowData_Way *) window_data->specific;
    if (window_data_specific->shm_format == -1u) {
        switch (format) {
            // We could do RGBA, but that would not be what is expected from minifb...
            // case WL_SHM_FORMAT_ARGB8888:
            case WL_SHM_FORMAT_XRGB8888:
                window_data_specific->shm_format = format;
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
    SWindowData_Way *window_data_specific = window_data ? (SWindowData_Way *) window_data->specific : NULL;
    if (window_data_specific == NULL) {
        return;
    }
    window_data_specific->preferred_scale_120 = scale;
}

//-------------------------------------
static const struct wp_fractional_scale_v1_listener fractional_scale_listener = {
    .preferred_scale = fractional_scale_preferred_scale,
};

//-------------------------------------
static int
find_output_index(SWindowData_Way *window_data_specific, struct wl_output *output) {
    if (window_data_specific == NULL || output == NULL) {
        return -1;
    }

    for (uint32_t i = 0; i < window_data_specific->output_count; ++i) {
        if (window_data_specific->outputs[i] == output) {
            return (int) i;
        }
    }
    return -1;
}

//-------------------------------------
// Recompute the integer output scale from all outputs the surface currently
// overlaps.  Uses the maximum scale so content is never blurry on any of them.
// Only meaningful when fractional scale is unavailable.
//-------------------------------------
static void
recompute_output_scale(SWindowData_Way *window_data_specific) {
    uint32_t max_scale = 1;
    for (uint32_t i = 0; i < window_data_specific->output_count; ++i) {
        if (window_data_specific->output_entered[i] && window_data_specific->output_scales[i] > max_scale) {
            max_scale = window_data_specific->output_scales[i];
        }
    }
    window_data_specific->integer_output_scale = max_scale;
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

#if defined(WL_OUTPUT_DONE_SINCE_VERSION)

//-------------------------------------
static void
output_done(void *data, struct wl_output *output) {
    kUnused(data);
    kUnused(output);
}

#endif

#if defined(WL_OUTPUT_SCALE_SINCE_VERSION)

//-------------------------------------
static void
output_scale(void *data, struct wl_output *output, int32_t factor) {
    SWindowData *window_data = (SWindowData *) data;
    SWindowData_Way *window_data_specific = window_data ? (SWindowData_Way *) window_data->specific : NULL;
    if (window_data_specific == NULL || output == NULL) {
        return;
    }

    int idx = find_output_index(window_data_specific, output);
    if (idx < 0) {
        return;
    }

    uint32_t scale = (factor > 0) ? (uint32_t) factor : 1;
    window_data_specific->output_scales[idx] = scale;
    if (window_data_specific->output_entered[idx]) {
        recompute_output_scale(window_data_specific);
    }
}

#endif

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
#if defined(WL_OUTPUT_DONE_SINCE_VERSION)
    .done = output_done,
#endif
#if defined(WL_OUTPUT_SCALE_SINCE_VERSION)
    .scale = output_scale,
#endif
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
    SWindowData_Way *window_data_specific = window_data ? (SWindowData_Way *) window_data->specific : NULL;
    if (window_data_specific == NULL || output == NULL) {
        return;
    }

    int idx = find_output_index(window_data_specific, output);
    if (idx >= 0) {
        window_data_specific->output_entered[idx] = 1;
        recompute_output_scale(window_data_specific);
    }
}

//-------------------------------------
static void
surface_leave(void *data, struct wl_surface *surface, struct wl_output *output) {
    kUnused(surface);
    SWindowData *window_data = (SWindowData *) data;
    SWindowData_Way *window_data_specific = window_data ? (SWindowData_Way *) window_data->specific : NULL;
    if (window_data_specific == NULL || output == NULL) {
        return;
    }

    int idx = find_output_index(window_data_specific, output);
    if (idx >= 0) {
        window_data_specific->output_entered[idx] = 0;
        recompute_output_scale(window_data_specific);
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
    SWindowData_Way   *window_data_specific = (SWindowData_Way *) window_data->specific;
    if (strcmp(iface, "wl_compositor") == 0) {
        uint32_t client_version = (uint32_t) wl_compositor_interface.version;
        uint32_t use_version = version < client_version ? version : client_version;
        window_data_specific->compositor = (struct wl_compositor *) wl_registry_bind(registry, id, &wl_compositor_interface, use_version);
        window_data_specific->compositor_version = use_version;
        MFB_LOG(MFB_LOG_TRACE, "wl_compositor: server=%u client=%u using=%u", version, client_version, use_version);
    }

    else if (strcmp(iface, "wl_shm") == 0) {
        uint32_t client_version = (uint32_t) wl_shm_interface.version;
        uint32_t use_version = version < client_version ? version : client_version;
        window_data_specific->shm = (struct wl_shm *) wl_registry_bind(registry, id, &wl_shm_interface, use_version);
        MFB_LOG(MFB_LOG_TRACE, "wl_shm: server=%u client=%u using=%u", version, client_version, use_version);
        if (window_data_specific->shm) {
            wl_shm_add_listener(window_data_specific->shm, &shm_listener, window_data);
            window_data_specific->cursor_theme = wl_cursor_theme_load(NULL, 32, window_data_specific->shm);
            if (window_data_specific->cursor_theme) {
                window_data_specific->default_cursor = wl_cursor_theme_get_cursor(window_data_specific->cursor_theme, "left_ptr");
            }
        }
    }

    else if (strcmp(iface, "xdg_wm_base") == 0) {
        uint32_t client_version = (uint32_t) xdg_wm_base_interface.version;
        uint32_t use_version = version < client_version ? version : client_version;
        window_data_specific->shell = (struct xdg_wm_base *) wl_registry_bind(registry, id, &xdg_wm_base_interface, use_version);
        MFB_LOG(MFB_LOG_TRACE, "xdg_wm_base: server=%u client=%u using=%u", version, client_version, use_version);
        if (window_data_specific->shell) {
            xdg_wm_base_add_listener(window_data_specific->shell, &shell_listener, NULL);
        }
    }

    else if (strcmp(iface, "wl_seat") == 0) {
        uint32_t client_version = (uint32_t) wl_seat_interface.version;
        uint32_t use_version = version < client_version ? version : client_version;
        window_data_specific->seat = (struct wl_seat *) wl_registry_bind(registry, id, &wl_seat_interface, use_version);
        window_data_specific->seat_version = use_version;
        MFB_LOG(MFB_LOG_TRACE, "wl_seat: server=%u client=%u using=%u", version, client_version, use_version);
        if (window_data_specific->seat) {
            wl_seat_add_listener(window_data_specific->seat, &seat_listener, window_data);
        }
    }

    else if (strcmp(iface, "wl_output") == 0) {
        uint32_t client_version = (uint32_t) wl_output_interface.version;
        uint32_t use_version = version < client_version ? version : client_version;
        struct wl_output *output = (struct wl_output *) wl_registry_bind(registry, id, &wl_output_interface, use_version);
        MFB_LOG(MFB_LOG_TRACE, "wl_output: server=%u client=%u using=%u", version, client_version, use_version);
        if (output) {
            wl_output_add_listener(output, &output_listener, window_data);
            if (window_data_specific->output_count < WAYLAND_MAX_OUTPUTS) {
                uint32_t idx = window_data_specific->output_count++;
                window_data_specific->outputs[idx] = output;
                window_data_specific->output_ids[idx] = id;
                window_data_specific->output_scales[idx] = 1;
            }
            else {
                wl_output_destroy(output);
            }
        }
    }

    else if (strcmp(iface, "wp_fractional_scale_manager_v1") == 0) {
        uint32_t client_version = (uint32_t) wp_fractional_scale_manager_v1_interface.version;
        uint32_t use_version = version < client_version ? version : client_version;
        window_data_specific->fractional_scale_manager = (struct wp_fractional_scale_manager_v1 *)
            wl_registry_bind(registry, id, &wp_fractional_scale_manager_v1_interface, use_version);
        MFB_LOG(MFB_LOG_TRACE, "wp_fractional_scale_manager_v1: server=%u client=%u using=%u", version, client_version, use_version);
    }

    else if (strcmp(iface, "zxdg_decoration_manager_v1") == 0) {
        uint32_t client_version = (uint32_t) zxdg_decoration_manager_v1_interface.version;
        uint32_t use_version = version < client_version ? version : client_version;
        window_data_specific->decoration_manager = (struct zxdg_decoration_manager_v1 *)
            wl_registry_bind(registry, id, &zxdg_decoration_manager_v1_interface, use_version);
        MFB_LOG(MFB_LOG_TRACE, "zxdg_decoration_manager_v1: server=%u client=%u using=%u", version, client_version, use_version);
    }
}

//-------------------------------------
static void
registry_global_remove(void *data, struct wl_registry *registry, uint32_t name) {
    kUnused(registry);

    SWindowData     *window_data          = (SWindowData *) data;
    SWindowData_Way *window_data_specific = (SWindowData_Way *) window_data->specific;

    for (uint32_t i = 0; i < window_data_specific->output_count; ++i) {
        if (window_data_specific->output_ids[i] == name) {
            struct wl_output *removed = window_data_specific->outputs[i];

            bool was_entered = window_data_specific->output_entered[i];

            wl_output_destroy(removed);

            // Compact arrays by moving the last element into the gap
            uint32_t last = window_data_specific->output_count - 1;
            if (i < last) {
                window_data_specific->outputs[i]        = window_data_specific->outputs[last];
                window_data_specific->output_ids[i]     = window_data_specific->output_ids[last];
                window_data_specific->output_scales[i]  = window_data_specific->output_scales[last];
                window_data_specific->output_entered[i] = window_data_specific->output_entered[last];
            }
            window_data_specific->outputs[last]        = NULL;
            window_data_specific->output_ids[last]     = 0;
            window_data_specific->output_scales[last]  = 0;
            window_data_specific->output_entered[last] = 0;
            window_data_specific->output_count--;

            if (was_entered) {
                recompute_output_scale(window_data_specific);
            }
            break;
        }
    }
}

//-------------------------------------
static const struct
wl_registry_listener registry_listener = {
    .global        = registry_global,
    .global_remove = registry_global_remove,
};

//-------------------------------------
static void
handle_shell_surface_configure(void *data, struct xdg_surface *shell_surface, uint32_t serial) {
    SWindowData *window_data = (SWindowData *) data;
    SWindowData_Way *window_data_specific = (SWindowData_Way *) window_data->specific;

    xdg_surface_ack_configure(shell_surface, serial);

    // Some compositors apply startup states (fullscreen/maximized) more reliably
    // when requested after the first configure handshake.
    if (!window_data_specific->startup_state_applied) {
        if (window_data_specific->request_fullscreen) {
            xdg_toplevel_set_fullscreen(window_data_specific->toplevel, NULL);
        }
        else if (window_data_specific->request_maximized) {
            xdg_toplevel_set_maximized(window_data_specific->toplevel);
        }
        window_data_specific->startup_state_applied = 1;
    }

    // On first configure, adapt slot 0 to the compositor's chosen size and
    // attach it so the surface becomes visible.
    if (!window_data->is_initialized) {
        uint32_t init_w = window_data->window_width;
        uint32_t init_h = window_data->window_height;

        SWaylandBufferSlot *slot = &window_data_specific->slots[0];
        if (slot_ensure_buffer(slot, window_data_specific->shm,
                               window_data_specific->shm_format, init_w, init_h) == false) {
            MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: failed to create initial buffer in first configure.");
            window_data->close = true;
        }
        else {
            wl_surface_attach(window_data_specific->surface, slot->wl_buf, 0, 0);

#if defined(WL_SURFACE_DAMAGE_BUFFER_SINCE_VERSION)
            if (window_data_specific->compositor_version >= WL_SURFACE_DAMAGE_BUFFER_SINCE_VERSION) {
                wl_surface_damage_buffer(window_data_specific->surface, 0, 0, init_w, init_h);
            }
            else
#endif
            {
                wl_surface_damage(window_data_specific->surface, 0, 0, init_w, init_h);
            }

            slot->busy = 1;
            wl_surface_commit(window_data_specific->surface);
            window_data->is_initialized = true;
        }
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

    MFB_LOG(MFB_LOG_DEBUG, "Toplevel configure: width=%d, height=%d", width, height);
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

    MFB_LOG(MFB_LOG_DEBUG, "Toplevel close");
}

//-------------------------------------
static void
handle_toplevel_configure_bounds(void *data, struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height) {
    kUnused(data);
    kUnused(xdg_toplevel);
    kUnused(width);
    kUnused(height);

    MFB_LOG(MFB_LOG_DEBUG, "Toplevel configure bounds: width=%d, height=%d", width, height);
}

//-------------------------------------
static void
handle_toplevel_wm_capabilities(void *data, struct xdg_toplevel *xdg_toplevel, struct wl_array *capabilities) {
    kUnused(data);
    kUnused(xdg_toplevel);
    kUnused(capabilities);

    MFB_LOG(MFB_LOG_DEBUG, "Toplevel wm capabilities");
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
static void
buffer_release(void *data, struct wl_buffer *wl_buf) {
    kUnused(wl_buf);
    SWaylandBufferSlot *slot = (SWaylandBufferSlot *) data;
    slot->busy = 0;
}

static const struct wl_buffer_listener buffer_listener = {
    .release = buffer_release,
};

//-------------------------------------
// Destroy a slot's resources (pool, buffer, mmap, fd).
//-------------------------------------
static void
slot_destroy(SWaylandBufferSlot *slot) {
    if (slot->wl_buf) {
        wl_buffer_destroy(slot->wl_buf);
        slot->wl_buf = NULL;
    }
    if (slot->pool) {
        wl_shm_pool_destroy(slot->pool);
        slot->pool = NULL;
    }
    if (slot->shm_ptr && slot->shm_ptr != MAP_FAILED && slot->pool_size > 0) {
        munmap(slot->shm_ptr, slot->pool_size);
        slot->shm_ptr = NULL;
    }
    if (slot->fd >= 0) {
        close(slot->fd);
        slot->fd = -1;
    }
    slot->pool_size = 0;
    slot->width     = 0;
    slot->height    = 0;
    slot->busy      = 0;
}

//-------------------------------------
// Rebuild a slot's pool+buffer if its dimensions are stale.
// Each slot owns its own fd, mmap, and wl_shm_pool — no shared offsets.
// Returns true on success (or if no rebuild was needed).
//-------------------------------------
static bool
slot_ensure_buffer(SWaylandBufferSlot *slot, struct wl_shm *shm,
                   uint32_t shm_format, uint32_t surface_w, uint32_t surface_h) {
    if (slot->wl_buf != NULL
        && slot->width == surface_w
        && slot->height == surface_h) {
        return true;
    }

    // Tear down previous resources for this slot.
    slot_destroy(slot);

    uint32_t stride    = 0;
    size_t   pool_size = 0;
    if (calculate_buffer_layout(surface_w, surface_h, &stride, &pool_size) == false) {
        return false;
    }

    if (pool_size > (size_t) INT_MAX) {
        return false;
    }

    // Create anonymous shared memory.
    char const *xdg_rt_dir = getenv("XDG_RUNTIME_DIR");
    if (xdg_rt_dir == NULL) {
        return false;
    }
    char shmfile[PATH_MAX];
    if (snprintf(shmfile, sizeof(shmfile), "%s/mfb-slot-XXXXXX", xdg_rt_dir) >= (int) sizeof(shmfile)) {
        return false;
    }
    slot->fd = mkstemp(shmfile);
    if (slot->fd == -1) {
        return false;
    }
    unlink(shmfile);

    if (ftruncate(slot->fd, (off_t) pool_size) == -1) {
        close(slot->fd);
        slot->fd = -1;
        return false;
    }

    slot->shm_ptr = (uint32_t *) mmap(NULL, pool_size, PROT_WRITE, MAP_SHARED, slot->fd, 0);
    if (slot->shm_ptr == MAP_FAILED) {
        slot->shm_ptr = NULL;
        close(slot->fd);
        slot->fd = -1;
        return false;
    }
    slot->pool_size = pool_size;

    slot->pool = wl_shm_create_pool(shm, slot->fd, (int) pool_size);
    if (slot->pool == NULL) {
        munmap(slot->shm_ptr, pool_size);
        slot->shm_ptr = NULL;
        close(slot->fd);
        slot->fd = -1;
        slot->pool_size = 0;
        return false;
    }

    slot->wl_buf = wl_shm_pool_create_buffer(slot->pool, 0,
                        surface_w, surface_h,
                        stride, shm_format);
    if (slot->wl_buf == NULL) {
        wl_shm_pool_destroy(slot->pool);
        slot->pool = NULL;
        munmap(slot->shm_ptr, pool_size);
        slot->shm_ptr = NULL;
        close(slot->fd);
        slot->fd = -1;
        slot->pool_size = 0;
        return false;
    }

    wl_buffer_add_listener(slot->wl_buf, &buffer_listener, slot);
    slot->width  = surface_w;
    slot->height = surface_h;
    return true;
}

//-------------------------------------
static bool
create_shm_buffer(SWindowData *window_data, SWindowData_Way *window_data_specific,
                  unsigned width, unsigned height, uint32_t buffer_stride, size_t buffer_total_bytes) {
    kUnused(buffer_stride);
    kUnused(buffer_total_bytes);

    window_data->window_width  = width;
    window_data->window_height = height;
    window_data->buffer_width  = width;
    window_data->buffer_height = height;
    window_data->buffer_stride = width * sizeof(uint32_t);
    calc_dst_factor(window_data, width, height);

    window_data->is_cursor_visible = true;

    // Each slot creates its own pool on demand via slot_ensure_buffer.
    for (int i = 0; i < WAYLAND_BUFFER_SLOTS; ++i) {
        SWaylandBufferSlot *slot = &window_data_specific->slots[i];
        memset(slot, 0, sizeof(*slot));
        slot->fd = -1;
        if (!slot_ensure_buffer(slot, window_data_specific->shm,
                                window_data_specific->shm_format, width, height)) {
            MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: initial buffer creation failed for slot %d.", i);
            return false;
        }
    }
    window_data_specific->front_slot = 0;

    // draw_buffer is not used with double-buffering; slots manage wl_buffers.
    window_data->draw_buffer = NULL;

    return true;
}

//-------------------------------------
static bool
create_xdg_toplevel(SWindowData *window_data, SWindowData_Way *window_data_specific,
                    unsigned effective_flags, const char *window_title, unsigned width, unsigned height) {
    window_data_specific->shell_surface = xdg_wm_base_get_xdg_surface(window_data_specific->shell, window_data_specific->surface);
    if (!window_data_specific->shell_surface) {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: failed to create xdg_surface.");
        return false;
    }

    xdg_surface_add_listener(window_data_specific->shell_surface, &shell_surface_listener, window_data);

    window_data_specific->toplevel = xdg_surface_get_toplevel(window_data_specific->shell_surface);
    if (!window_data_specific->toplevel) {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: failed to create xdg_toplevel.");
        return false;
    }

    if (window_data_specific->decoration_manager) {
        window_data_specific->toplevel_decoration =
            zxdg_decoration_manager_v1_get_toplevel_decoration(window_data_specific->decoration_manager, window_data_specific->toplevel);
        if (window_data_specific->toplevel_decoration) {
            zxdg_toplevel_decoration_v1_add_listener(window_data_specific->toplevel_decoration,
                                                     &toplevel_decoration_listener, window_data);
            if (effective_flags & MFB_WF_BORDERLESS) {
                zxdg_toplevel_decoration_v1_set_mode(window_data_specific->toplevel_decoration,
                                                     ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
            }
            else {
                zxdg_toplevel_decoration_v1_set_mode(window_data_specific->toplevel_decoration,
                                                     ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
            }
        }
    }

    window_data_specific->request_fullscreen = (effective_flags & MFB_WF_FULLSCREEN) ? 1 : 0;
    window_data_specific->request_maximized =
        (!window_data_specific->request_fullscreen && (effective_flags & MFB_WF_FULLSCREEN_DESKTOP)) ? 1 : 0;
    window_data_specific->startup_state_applied = 0;

    if (window_data_specific->request_fullscreen || window_data_specific->request_maximized) {
        xdg_toplevel_set_min_size(window_data_specific->toplevel, 0, 0);
        xdg_toplevel_set_max_size(window_data_specific->toplevel, 0, 0);
    }
    else {
        if (effective_flags & MFB_WF_RESIZABLE) {
            xdg_toplevel_set_min_size(window_data_specific->toplevel, 0, 0);
            xdg_toplevel_set_max_size(window_data_specific->toplevel, 0, 0);
        }
        else {
            xdg_toplevel_set_min_size(window_data_specific->toplevel, (int32_t) width, (int32_t) height);
            xdg_toplevel_set_max_size(window_data_specific->toplevel, (int32_t) width, (int32_t) height);
        }
    }

    xdg_toplevel_set_app_id(window_data_specific->toplevel, MFB_STR(MFB_APP_ID));

    xdg_toplevel_set_title(window_data_specific->toplevel, window_title);
    xdg_toplevel_add_listener(window_data_specific->toplevel, &toplevel_listener, window_data);

    // Commit without a buffer to trigger initial configure event
    wl_surface_commit(window_data_specific->surface);

    // Process events until we get the configure event and the surface is mapped
    while (window_data->is_initialized == false && window_data->close == false) {
        if (wl_display_dispatch(window_data_specific->display) == -1) {
            MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: wl_display_dispatch failed while waiting for initial configure event.");
            return false;
        }
    }

    if (window_data->close) {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: initialization failed during configure handshake.");
        return false;
    }

    return true;
}

//-------------------------------------
struct mfb_window *
mfb_open_ex(const char *title, unsigned width, unsigned height, unsigned flags) {
    const unsigned known_flags = MFB_WF_RESIZABLE | MFB_WF_FULLSCREEN | MFB_WF_FULLSCREEN_DESKTOP | MFB_WF_BORDERLESS | MFB_WF_ALWAYS_ON_TOP;
    unsigned effective_flags = flags;
    const char *window_title = (title != NULL && title[0] != '\0') ? title : "minifb";
    uint32_t buffer_stride = 0;
    size_t buffer_total_bytes = 0;
    if (!calculate_buffer_layout(width, height, &buffer_stride, &buffer_total_bytes)) {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: invalid window size %ux%u.", width, height);
        return NULL;
    }

    if ((effective_flags & ~known_flags) != 0u) {
        MFB_LOG(MFB_LOG_WARNING, "WaylandMiniFB: unknown window flags 0x%x will be ignored.", effective_flags & ~known_flags);
    }

    if ((effective_flags & MFB_WF_FULLSCREEN) && (effective_flags & MFB_WF_FULLSCREEN_DESKTOP)) {
        MFB_LOG(MFB_LOG_WARNING, "WaylandMiniFB: MFB_WF_FULLSCREEN and MFB_WF_FULLSCREEN_DESKTOP were both requested; MFB_WF_FULLSCREEN takes precedence.");
        effective_flags &= ~MFB_WF_FULLSCREEN_DESKTOP;
    }

    if (effective_flags & MFB_WF_ALWAYS_ON_TOP) {
        MFB_LOG(MFB_LOG_WARNING, "WaylandMiniFB: MFB_WF_ALWAYS_ON_TOP is not supported by xdg-shell and will be ignored.");
    }

    SWindowData *window_data = (SWindowData *) malloc(sizeof(SWindowData));
    if (window_data == NULL) {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: failed to allocate SWindowData.");
        return NULL;
    }
    memset(window_data, 0, sizeof(SWindowData));

    SWindowData_Way *window_data_specific = (SWindowData_Way *) malloc(sizeof(SWindowData_Way));
    if (window_data_specific == NULL) {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: failed to allocate SWindowData_Way.");
        free(window_data);
        return NULL;
    }
    memset(window_data_specific, 0, sizeof(SWindowData_Way));
    window_data_specific->integer_output_scale = 1;
    for (int i = 0; i < WAYLAND_BUFFER_SLOTS; ++i) {
        window_data_specific->slots[i].fd = -1;
    }
    window_data->specific = window_data_specific;

    window_data_specific->shm_format = -1u;

    window_data_specific->display = wl_display_connect(NULL);
    if (!window_data_specific->display) {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: failed to connect to Wayland display.");
        free(window_data);
        free(window_data_specific);
        return NULL;
    }
    window_data_specific->registry = wl_display_get_registry(window_data_specific->display);
    if (!window_data_specific->registry) {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: wl_display_get_registry returned NULL.");
        goto out;
    }
    wl_registry_add_listener(window_data_specific->registry, &registry_listener, window_data);

    init_keycodes();

    if (wl_display_dispatch(window_data_specific->display) == -1 ||
        wl_display_roundtrip(window_data_specific->display) == -1) {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: failed to initialize Wayland globals (dispatch/roundtrip).");
        goto out;
    }

    if (!window_data_specific->decoration_manager) {
        MFB_LOG(MFB_LOG_WARNING, "WaylandMiniFB: zxdg_decoration_manager_v1 is unavailable; server-side decorations control may be limited.");
    }

    if (!window_data_specific->fractional_scale_manager) {
        MFB_LOG(MFB_LOG_WARNING, "WaylandMiniFB: wp_fractional_scale_manager_v1 is unavailable; falling back to integer wl_output scale.");
    }

    if (!window_data_specific->seat) {
        MFB_LOG(MFB_LOG_WARNING, "WaylandMiniFB: wl_seat is unavailable; keyboard and pointer input will not be available.");
    }

    if (window_data_specific->output_count == 0) {
        MFB_LOG(MFB_LOG_WARNING, "WaylandMiniFB: wl_output is unavailable; monitor scale tracking will use fallback values.");
    }

    // did not get a format we want... meh
    if (window_data_specific->shm_format == -1u) {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: compositor does not expose a supported shared memory format.");
        goto out;
    }

    if (!window_data_specific->compositor) {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: Wayland compositor interface is unavailable.");
        goto out;
    }

    if (!create_shm_buffer(window_data, window_data_specific, width, height, buffer_stride, buffer_total_bytes))
        goto out;

    window_data_specific->surface = wl_compositor_create_surface(window_data_specific->compositor);
    if (!window_data_specific->surface) {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: failed to create Wayland surface.");
        goto out;
    }

    wl_surface_add_listener(window_data_specific->surface, &surface_listener, window_data);
    if (window_data_specific->fractional_scale_manager) {
        window_data_specific->fractional_scale =
            wp_fractional_scale_manager_v1_get_fractional_scale(window_data_specific->fractional_scale_manager,
                                                                window_data_specific->surface);
        if (window_data_specific->fractional_scale) {
            wp_fractional_scale_v1_add_listener(window_data_specific->fractional_scale,
                                                &fractional_scale_listener,
                                                window_data);
        }
    }

    window_data_specific->cursor_surface = wl_compositor_create_surface(window_data_specific->compositor);
    if (!window_data_specific->cursor_surface) {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: failed to create Wayland cursor surface.");
        goto out;
    }

    if (!window_data_specific->shell) {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: xdg_wm_base is unavailable; cannot create a toplevel surface.");
        goto out;
    }

    if (!create_xdg_toplevel(window_data, window_data_specific, effective_flags, window_title, width, height))
        goto out;

    window_data_specific->timer = mfb_timer_create();
    if (window_data_specific->timer == NULL) {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: failed to create frame timer.");
        goto out;
    }

    mfb_set_keyboard_callback((struct mfb_window *) window_data, keyboard_default);

    MFB_LOG(MFB_LOG_DEBUG, "Window created using Wayland API");

    return (struct mfb_window *) window_data;

out:
    MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: mfb_open_ex failed and is cleaning up partially initialized resources.");
    destroy(window_data);

    return NULL;
}

//-------------------------------------
// Shared Wayland event loop helper.
//
// Performs one prepare_read / flush / poll / read_events / dispatch_pending
// cycle.  Handles EAGAIN on flush (adds POLLOUT to poll) and drains any
// already-queued events before attempting a read.
//
// timeout_ms:  -1 = block forever, 0 = non-blocking, >0 = milliseconds.
// Returns:      1 = events dispatched successfully (from fd or pending queue)
//               0 = poll timed out without dispatching any events
//              -1 = fatal error
//-------------------------------------
static int
wayland_poll_dispatch(struct wl_display *display, int timeout_ms) {
    int display_fd = wl_display_get_fd(display);

    // Drain any already-queued events until prepare_read succeeds.
    bool had_pending = false;
    while (wl_display_prepare_read(display) != 0) {
        if (wl_display_dispatch_pending(display) == -1) {
            return -1;
        }
        had_pending = true;
    }

    // Flush outgoing requests; track EAGAIN so we can poll for writability.
    int flush_res = wl_display_flush(display);
    if (flush_res == -1 && errno != EAGAIN) {
        wl_display_cancel_read(display);
        return -1;
    }

    // If pending events were already dispatched, do not block - just check
    // the fd non-blocking so the caller can re-evaluate its loop condition.
    int effective_timeout = had_pending ? 0 : timeout_ms;

    struct pollfd pfd;
    pfd.fd = display_fd;
    pfd.events = POLLIN | ((flush_res == -1) ? POLLOUT : 0);
    pfd.revents = 0;

    int rc;
    do {
        rc = poll(&pfd, 1, effective_timeout);
    } while (rc < 0 && errno == EINTR);

    if (rc > 0) {
        if (pfd.revents & POLLOUT) {
            // Socket writable again after EAGAIN; retry flush.
            wl_display_flush(display);
        }

        if (pfd.revents & POLLIN) {
            if (wl_display_read_events(display) == -1) {
                return -1;
            }
        }
        else {
            wl_display_cancel_read(display);
        }

        if (wl_display_dispatch_pending(display) == -1) {
            return -1;
        }

        return 1;
    }

    wl_display_cancel_read(display);
    if (rc < 0) {
        return -1;
    }

    return had_pending ? 1 : 0;
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

    *(uint32_t *) data = 1;
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
    uint32_t buffer_stride = 0;
    size_t buffer_total_bytes = 0;

    if (window == NULL) {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: mfb_update_ex called with a null window pointer.");
        return MFB_STATE_INVALID_WINDOW;
    }

    SWindowData *window_data = (SWindowData *) window;
    if (window_data->close) {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: mfb_update_ex aborted because the window is marked for close.");
        destroy(window_data);
        return MFB_STATE_EXIT;
    }

    if (buffer == NULL) {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: mfb_update_ex called with a null buffer.");
        return MFB_STATE_INVALID_BUFFER;
    }

    if (!calculate_buffer_layout(width, height, &buffer_stride, &buffer_total_bytes)) {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: mfb_update_ex called with invalid buffer size %ux%u.", width, height);
        return MFB_STATE_INVALID_BUFFER;
    }

    SWindowData_Way   *window_data_specific = (SWindowData_Way *) window_data->specific;
    if (window_data_specific == NULL) {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: missing Wayland-specific window data during mfb_update_ex.");
        return MFB_STATE_INVALID_WINDOW;
    }

    if (!window_data_specific->display || wl_display_get_error(window_data_specific->display) != 0)
    {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: invalid Wayland display state during mfb_update_ex.");
        return MFB_STATE_INTERNAL_ERROR;
    }

    // --- Update source buffer dimensions (user-provided) ---
    if (window_data->buffer_width != width || window_data->buffer_height != height) {
        window_data->buffer_width  = width;
        window_data->buffer_height = height;
        window_data->buffer_stride = buffer_stride;
    }

    // --- Capture surface dimensions for the entire frame ---
    // Configure events can arrive during dispatch loops later in this function,
    // changing window_width/height.  Capture once here so pool, slots, and
    // pixel composition all use consistent dimensions.
    const uint32_t surface_w = window_data->window_width;
    const uint32_t surface_h = window_data->window_height;

    // --- Acquire a free slot, rebuilding its pool+buffer if dimensions changed ---
    // Each slot owns its own wl_shm_pool, so resizing one slot never affects
    // another — no overlap on shrink, no shared offsets.
    SWaylandBufferSlot *active_slot = NULL;
    bool ensure_failed = false;
    {
        int start = window_data_specific->front_slot;
        for (int i = 0; i < WAYLAND_BUFFER_SLOTS; ++i) {
            int idx = (start + i) % WAYLAND_BUFFER_SLOTS;
            SWaylandBufferSlot *slot = &window_data_specific->slots[idx];

            if (slot->busy) {
                continue;
            }
            if (!slot_ensure_buffer(slot, window_data_specific->shm,
                                    window_data_specific->shm_format, surface_w, surface_h)) {
                MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: wl_buffer recreation failed for slot %d.", idx);
                ensure_failed = true;
                continue;
            }

            active_slot = slot;
            window_data_specific->front_slot = (idx + 1) % WAYLAND_BUFFER_SLOTS;
            break;
        }
    }

    // If no slot is free, dispatch events briefly to process pending releases.
    if (active_slot == NULL && !ensure_failed) {
        struct wl_display *display = window_data_specific->display;
        for (int attempt = 0; attempt < 50 && active_slot == NULL; ++attempt) {
            if (wayland_poll_dispatch(display, 2) == -1) {
                MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: event dispatch failed while waiting for buffer release.");
                return MFB_STATE_INTERNAL_ERROR;
            }
            for (int i = 0; i < WAYLAND_BUFFER_SLOTS; ++i) {
                SWaylandBufferSlot *slot = &window_data_specific->slots[i];
                if (slot->busy) {
                    continue;
                }
                if (!slot_ensure_buffer(slot, window_data_specific->shm,
                                        window_data_specific->shm_format, surface_w, surface_h)) {
                    ensure_failed = true;
                    continue;
                }
                active_slot = slot;
                window_data_specific->front_slot = (i + 1) % WAYLAND_BUFFER_SLOTS;
                break;
            }
        }
    }

    // Always reset per-frame input state, even when dropping a frame.
    window_data->mouse_wheel_x = 0.0f;
    window_data->mouse_wheel_y = 0.0f;

    if (active_slot == NULL) {
        if (ensure_failed) {
            MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: wl_buffer recreation failed; cannot present frame.");
            return MFB_STATE_INTERNAL_ERROR;
        }
        MFB_LOG(MFB_LOG_WARNING, "WaylandMiniFB: all buffer slots busy; dropping frame.");
        return MFB_STATE_OK;
    }

    // --- Compose pixels into the presentation buffer ---
    uint32_t *shm_ptr = active_slot->shm_ptr;

    if (window_data->buffer_width == window_data->dst_width
        && window_data->buffer_height == window_data->dst_height
        && window_data->dst_offset_x == 0
        && window_data->dst_offset_y == 0
        && surface_w == window_data->dst_width
        && surface_h == window_data->dst_height) {
        // Fast path: source fills the entire surface, no scaling needed.
        memcpy(shm_ptr, buffer, (size_t) surface_w * surface_h * sizeof(uint32_t));
    }
    else {
        // Clear the surface buffer, then stretch the source into the viewport.
        memset(shm_ptr, 0, (size_t) surface_w * surface_h * sizeof(uint32_t));
        stretch_image(
            (uint32_t *) buffer, 0, 0,
            window_data->buffer_width, window_data->buffer_height, window_data->buffer_width,
            shm_ptr, window_data->dst_offset_x, window_data->dst_offset_y,
            window_data->dst_width, window_data->dst_height, surface_w);
    }

    wl_surface_attach(window_data_specific->surface, active_slot->wl_buf, 0, 0);
#if defined(WL_SURFACE_DAMAGE_BUFFER_SINCE_VERSION)
    if (window_data_specific->compositor_version >= WL_SURFACE_DAMAGE_BUFFER_SINCE_VERSION) {
        wl_surface_damage_buffer(window_data_specific->surface, 0, 0,
                                 surface_w, surface_h);
    }
    else
#endif
    {
        wl_surface_damage(window_data_specific->surface, 0, 0,
                          surface_w, surface_h);
    }
    struct wl_callback *frame_callback = wl_surface_frame(window_data_specific->surface);
    if (!frame_callback) {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: wl_surface_frame returned NULL.");
        return MFB_STATE_INTERNAL_ERROR;
    }
    wl_callback_add_listener(frame_callback, &frame_listener, &done);
    active_slot->busy = 1;
    wl_surface_commit(window_data_specific->surface);

    {
        struct wl_display *display = window_data_specific->display;

        while (!done && window_data->close == false) {
            int poll_res = wayland_poll_dispatch(display, 100);
            if (poll_res == -1) {
                MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: event dispatch failed during frame wait.");
                if (!done) {
                    wl_callback_destroy(frame_callback);
                }
                return MFB_STATE_INTERNAL_ERROR;
            }
            // The helper may have dispatched the frame callback during its
            // pending-event drain and then timed out waiting for further
            // events.  Check done before treating the timeout as a problem.
            if (poll_res == 0 && !done) {
                MFB_LOG(MFB_LOG_WARNING, "WaylandMiniFB: frame callback not received within 100 ms.");
                break;
            }
        }
    }

    if (!done) {
        wl_callback_destroy(frame_callback);
    }

    if (window_data->close) {
        destroy(window_data);
        return MFB_STATE_EXIT;
    }

    return MFB_STATE_OK;
}

//-------------------------------------
mfb_update_state
mfb_update_events(struct mfb_window *window) {
    if (window == NULL) {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: mfb_update_events called with a null window pointer.");
        return MFB_STATE_INVALID_WINDOW;
    }

    SWindowData *window_data = (SWindowData *) window;
    if (window_data->close) {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: mfb_update_events aborted because the window is marked for close.");
        destroy(window_data);
        return MFB_STATE_EXIT;
    }

    SWindowData_Way   *window_data_specific = (SWindowData_Way *) window_data->specific;
    if (window_data_specific == NULL) {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: missing Wayland-specific window data during mfb_update_events.");
        return MFB_STATE_INVALID_WINDOW;
    }
    if (!window_data_specific->display || wl_display_get_error(window_data_specific->display) != 0)
    {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: invalid Wayland display state during mfb_update_events.");
        return MFB_STATE_INTERNAL_ERROR;
    }

    window_data->mouse_wheel_x = 0.0f;
    window_data->mouse_wheel_y = 0.0f;

    // Non-blocking read/dispatch so mfb_update_events keeps X11-like behavior.
    if (wayland_poll_dispatch(window_data_specific->display, 0) == -1) {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: event dispatch failed in mfb_update_events.");
        return MFB_STATE_INTERNAL_ERROR;
    }

    if (window_data->close) {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: mfb_update_events detected close request after event dispatch.");
        destroy(window_data);
        return MFB_STATE_EXIT;
    }

    return MFB_STATE_OK;
}

//-------------------------------------
extern double   g_time_for_frame;

//-------------------------------------
bool
mfb_wait_sync(struct mfb_window *window) {
    if (window == NULL) {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: mfb_wait_sync called with a null window pointer.");
        return false;
    }

    SWindowData *window_data = (SWindowData *) window;
    if (window_data->close) {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: mfb_wait_sync aborted because the window is marked for close.");
        destroy(window_data);
        return false;
    }

    SWindowData_Way *window_data_specific = (SWindowData_Way *) window_data->specific;
    if (window_data_specific == NULL) {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: mfb_wait_sync missing Wayland-specific window data.");
        return false;
    }

    struct wl_display *display = window_data_specific->display;
    if (display == NULL) {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: mfb_wait_sync has a null Wayland display handle.");
        return false;
    }
    if (window_data_specific->timer == NULL) {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: mfb_wait_sync missing frame timer state.");
        return false;
    }
    window_data->mouse_wheel_x = 0.0f;
    window_data->mouse_wheel_y = 0.0f;

    // Flush outgoing requests and dispatch pending events once before pacing
    wl_display_flush(display);
    if (wl_display_dispatch_pending(display) == -1) {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: wl_display_dispatch_pending failed before sync pacing.");
        return false;
    }

    if (window_data->close) {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: mfb_wait_sync aborted after dispatch because the window is marked for close.");
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

            if (wayland_poll_dispatch(display, timeout_ms) == -1) {
                MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: event dispatch failed during sync loop.");
                return false;
            }
        }
        else {
            sched_yield(); // or nanosleep((const struct timespec){0, 0}, NULL);
        }

        if (wl_display_dispatch_pending(display) == -1) {
            MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: wl_display_dispatch_pending failed during sync loop.");
            return false;
        }

        if (window_data->close) {
            MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: mfb_wait_sync aborted during sync loop because the window is marked for close.");
            destroy(window_data);
            return false;
        }
    }

    mfb_timer_compensated_reset(window_data_specific->timer);
    return true;
}

//-------------------------------------
extern short int g_keycodes[MFB_MAX_KEYS];

//-------------------------------------
void
init_keycodes() {
    static bool s_initialized = false;
    if (s_initialized) {
        return;
    }
    s_initialized = true;

    for (size_t i = 0; i < MFB_MAX_KEYS; ++i) {
        g_keycodes[i] = MFB_KB_KEY_UNKNOWN;
    }

    g_keycodes[KEY_GRAVE]      = MFB_KB_KEY_GRAVE_ACCENT;
    g_keycodes[KEY_1]          = MFB_KB_KEY_1;
    g_keycodes[KEY_2]          = MFB_KB_KEY_2;
    g_keycodes[KEY_3]          = MFB_KB_KEY_3;
    g_keycodes[KEY_4]          = MFB_KB_KEY_4;
    g_keycodes[KEY_5]          = MFB_KB_KEY_5;
    g_keycodes[KEY_6]          = MFB_KB_KEY_6;
    g_keycodes[KEY_7]          = MFB_KB_KEY_7;
    g_keycodes[KEY_8]          = MFB_KB_KEY_8;
    g_keycodes[KEY_9]          = MFB_KB_KEY_9;
    g_keycodes[KEY_0]          = MFB_KB_KEY_0;
    g_keycodes[KEY_SPACE]      = MFB_KB_KEY_SPACE;
    g_keycodes[KEY_MINUS]      = MFB_KB_KEY_MINUS;
    g_keycodes[KEY_EQUAL]      = MFB_KB_KEY_EQUAL;
    g_keycodes[KEY_Q]          = MFB_KB_KEY_Q;
    g_keycodes[KEY_W]          = MFB_KB_KEY_W;
    g_keycodes[KEY_E]          = MFB_KB_KEY_E;
    g_keycodes[KEY_R]          = MFB_KB_KEY_R;
    g_keycodes[KEY_T]          = MFB_KB_KEY_T;
    g_keycodes[KEY_Y]          = MFB_KB_KEY_Y;
    g_keycodes[KEY_U]          = MFB_KB_KEY_U;
    g_keycodes[KEY_I]          = MFB_KB_KEY_I;
    g_keycodes[KEY_O]          = MFB_KB_KEY_O;
    g_keycodes[KEY_P]          = MFB_KB_KEY_P;
    g_keycodes[KEY_LEFTBRACE]  = MFB_KB_KEY_LEFT_BRACKET;
    g_keycodes[KEY_RIGHTBRACE] = MFB_KB_KEY_RIGHT_BRACKET;
    g_keycodes[KEY_A]          = MFB_KB_KEY_A;
    g_keycodes[KEY_S]          = MFB_KB_KEY_S;
    g_keycodes[KEY_D]          = MFB_KB_KEY_D;
    g_keycodes[KEY_F]          = MFB_KB_KEY_F;
    g_keycodes[KEY_G]          = MFB_KB_KEY_G;
    g_keycodes[KEY_H]          = MFB_KB_KEY_H;
    g_keycodes[KEY_J]          = MFB_KB_KEY_J;
    g_keycodes[KEY_K]          = MFB_KB_KEY_K;
    g_keycodes[KEY_L]          = MFB_KB_KEY_L;
    g_keycodes[KEY_SEMICOLON]  = MFB_KB_KEY_SEMICOLON;
    g_keycodes[KEY_APOSTROPHE] = MFB_KB_KEY_APOSTROPHE;
    g_keycodes[KEY_Z]          = MFB_KB_KEY_Z;
    g_keycodes[KEY_X]          = MFB_KB_KEY_X;
    g_keycodes[KEY_C]          = MFB_KB_KEY_C;
    g_keycodes[KEY_V]          = MFB_KB_KEY_V;
    g_keycodes[KEY_B]          = MFB_KB_KEY_B;
    g_keycodes[KEY_N]          = MFB_KB_KEY_N;
    g_keycodes[KEY_M]          = MFB_KB_KEY_M;
    g_keycodes[KEY_COMMA]      = MFB_KB_KEY_COMMA;
    g_keycodes[KEY_DOT]        = MFB_KB_KEY_PERIOD;
    g_keycodes[KEY_SLASH]      = MFB_KB_KEY_SLASH;
    g_keycodes[KEY_BACKSLASH]  = MFB_KB_KEY_BACKSLASH;
    g_keycodes[KEY_ESC]        = MFB_KB_KEY_ESCAPE;
    g_keycodes[KEY_TAB]        = MFB_KB_KEY_TAB;
    g_keycodes[KEY_LEFTSHIFT]  = MFB_KB_KEY_LEFT_SHIFT;
    g_keycodes[KEY_RIGHTSHIFT] = MFB_KB_KEY_RIGHT_SHIFT;
    g_keycodes[KEY_LEFTCTRL]   = MFB_KB_KEY_LEFT_CONTROL;
    g_keycodes[KEY_RIGHTCTRL]  = MFB_KB_KEY_RIGHT_CONTROL;
    g_keycodes[KEY_LEFTALT]    = MFB_KB_KEY_LEFT_ALT;
    g_keycodes[KEY_RIGHTALT]   = MFB_KB_KEY_RIGHT_ALT;
    g_keycodes[KEY_LEFTMETA]   = MFB_KB_KEY_LEFT_SUPER;
    g_keycodes[KEY_RIGHTMETA]  = MFB_KB_KEY_RIGHT_SUPER;
    g_keycodes[KEY_MENU]       = MFB_KB_KEY_MENU;
    g_keycodes[KEY_NUMLOCK]    = MFB_KB_KEY_NUM_LOCK;
    g_keycodes[KEY_CAPSLOCK]   = MFB_KB_KEY_CAPS_LOCK;
    g_keycodes[KEY_PRINT]      = MFB_KB_KEY_PRINT_SCREEN;
    g_keycodes[KEY_SCROLLLOCK] = MFB_KB_KEY_SCROLL_LOCK;
    g_keycodes[KEY_PAUSE]      = MFB_KB_KEY_PAUSE;
    g_keycodes[KEY_DELETE]     = MFB_KB_KEY_DELETE;
    g_keycodes[KEY_BACKSPACE]  = MFB_KB_KEY_BACKSPACE;
    g_keycodes[KEY_ENTER]      = MFB_KB_KEY_ENTER;
    g_keycodes[KEY_HOME]       = MFB_KB_KEY_HOME;
    g_keycodes[KEY_END]        = MFB_KB_KEY_END;
    g_keycodes[KEY_PAGEUP]     = MFB_KB_KEY_PAGE_UP;
    g_keycodes[KEY_PAGEDOWN]   = MFB_KB_KEY_PAGE_DOWN;
    g_keycodes[KEY_INSERT]     = MFB_KB_KEY_INSERT;
    g_keycodes[KEY_LEFT]       = MFB_KB_KEY_LEFT;
    g_keycodes[KEY_RIGHT]      = MFB_KB_KEY_RIGHT;
    g_keycodes[KEY_DOWN]       = MFB_KB_KEY_DOWN;
    g_keycodes[KEY_UP]         = MFB_KB_KEY_UP;
    g_keycodes[KEY_F1]         = MFB_KB_KEY_F1;
    g_keycodes[KEY_F2]         = MFB_KB_KEY_F2;
    g_keycodes[KEY_F3]         = MFB_KB_KEY_F3;
    g_keycodes[KEY_F4]         = MFB_KB_KEY_F4;
    g_keycodes[KEY_F5]         = MFB_KB_KEY_F5;
    g_keycodes[KEY_F6]         = MFB_KB_KEY_F6;
    g_keycodes[KEY_F7]         = MFB_KB_KEY_F7;
    g_keycodes[KEY_F8]         = MFB_KB_KEY_F8;
    g_keycodes[KEY_F9]         = MFB_KB_KEY_F9;
    g_keycodes[KEY_F10]        = MFB_KB_KEY_F10;
    g_keycodes[KEY_F11]        = MFB_KB_KEY_F11;
    g_keycodes[KEY_F12]        = MFB_KB_KEY_F12;
    g_keycodes[KEY_F13]        = MFB_KB_KEY_F13;
    g_keycodes[KEY_F14]        = MFB_KB_KEY_F14;
    g_keycodes[KEY_F15]        = MFB_KB_KEY_F15;
    g_keycodes[KEY_F16]        = MFB_KB_KEY_F16;
    g_keycodes[KEY_F17]        = MFB_KB_KEY_F17;
    g_keycodes[KEY_F18]        = MFB_KB_KEY_F18;
    g_keycodes[KEY_F19]        = MFB_KB_KEY_F19;
    g_keycodes[KEY_F20]        = MFB_KB_KEY_F20;
    g_keycodes[KEY_F21]        = MFB_KB_KEY_F21;
    g_keycodes[KEY_F22]        = MFB_KB_KEY_F22;
    g_keycodes[KEY_F23]        = MFB_KB_KEY_F23;
    g_keycodes[KEY_F24]        = MFB_KB_KEY_F24;
    g_keycodes[KEY_KPSLASH]    = MFB_KB_KEY_KP_DIVIDE;
    g_keycodes[KEY_KPASTERISK] = MFB_KB_KEY_KP_MULTIPLY;
    g_keycodes[KEY_KPDOT]      = MFB_KB_KEY_KP_DECIMAL;
    g_keycodes[KEY_KPMINUS]    = MFB_KB_KEY_KP_SUBTRACT;
    g_keycodes[KEY_KPPLUS]     = MFB_KB_KEY_KP_ADD;
    g_keycodes[KEY_KP0]        = MFB_KB_KEY_KP_0;
    g_keycodes[KEY_KP1]        = MFB_KB_KEY_KP_1;
    g_keycodes[KEY_KP2]        = MFB_KB_KEY_KP_2;
    g_keycodes[KEY_KP3]        = MFB_KB_KEY_KP_3;
    g_keycodes[KEY_KP4]        = MFB_KB_KEY_KP_4;
    g_keycodes[KEY_KP5]        = MFB_KB_KEY_KP_5;
    g_keycodes[KEY_KP6]        = MFB_KB_KEY_KP_6;
    g_keycodes[KEY_KP7]        = MFB_KB_KEY_KP_7;
    g_keycodes[KEY_KP8]        = MFB_KB_KEY_KP_8;
    g_keycodes[KEY_KP9]        = MFB_KB_KEY_KP_9;
    g_keycodes[KEY_KPCOMMA]    = MFB_KB_KEY_KP_DECIMAL;
    g_keycodes[KEY_KPEQUAL]    = MFB_KB_KEY_KP_EQUAL;
    g_keycodes[KEY_KPENTER]    = MFB_KB_KEY_KP_ENTER;
}

//-------------------------------------
bool
mfb_set_viewport(struct mfb_window *window, unsigned offset_x, unsigned offset_y, unsigned width, unsigned height) {
    SWindowData *window_data = (SWindowData *) window;
    if (!mfb_validate_viewport(window_data, offset_x, offset_y, width, height, "WaylandMiniFB")) {
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

    if (window != NULL) {
        SWindowData *window_data = (SWindowData *) window;
        SWindowData_Way *window_data_specific = (SWindowData_Way *) window_data->specific;
        if (window_data_specific) {
            if (window_data_specific->preferred_scale_120 > 0) {
                float scale = (float) window_data_specific->preferred_scale_120 / WAYLAND_FRACTIONAL_SCALE_DENOMINATOR;
                if (scale > 0.0f) {
                    x = scale;
                    y = scale;
                }
            }
            else if (window_data_specific->integer_output_scale > 0) {
                x = (float) window_data_specific->integer_output_scale;
                y = (float) window_data_specific->integer_output_scale;
            }
        }
    }

    if (scale_x) {
        *scale_x = x;
        if (*scale_x == 0) {
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
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: mfb_show_cursor called with a null window pointer.");
        return;
    }

    SWindowData_Way *window_data_specific = (SWindowData_Way *) window_data->specific;
    if (window_data_specific == NULL) {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: mfb_show_cursor missing Wayland-specific window data.");
        return;
    }

    // Keep requested visibility state even if we can't apply it immediately.
    window_data->is_cursor_visible = show;

    struct wl_pointer *pointer = window_data_specific->pointer;
    struct wl_surface *cursor_surface = window_data_specific->cursor_surface;
    if (pointer == NULL) {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: mfb_show_cursor cannot update cursor because wl_pointer is null.");
        return;
    }

    if (!window_data_specific->pointer_serial_valid) {
        return;
    }

    uint32_t serial = window_data_specific->pointer_enter_serial;
    if (show) {
        struct wl_cursor *cursor = window_data_specific->default_cursor;
        if (cursor == NULL || cursor->image_count == 0 || cursor_surface == NULL) {
            MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: default cursor data is incomplete (cursor=%p, image_count=%u, cursor_surface=%p).",
                    (void *) cursor, cursor ? cursor->image_count : 0, (void *) cursor_surface);
            return;
        }

        struct wl_cursor_image *cursor_image = cursor->images[0];
        struct wl_buffer *cursor_image_buffer = wl_cursor_image_get_buffer(cursor_image);
        if (cursor_image_buffer == NULL) {
            MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: wl_cursor_image_get_buffer returned NULL.");
            return;
        }

        wl_pointer_set_cursor(pointer, serial, cursor_surface, cursor_image->hotspot_x, cursor_image->hotspot_y);
        wl_surface_attach(cursor_surface, cursor_image_buffer, 0, 0);
#if defined(WL_SURFACE_DAMAGE_BUFFER_SINCE_VERSION)
        if (window_data_specific->compositor_version >= WL_SURFACE_DAMAGE_BUFFER_SINCE_VERSION) {
            wl_surface_damage_buffer(cursor_surface, 0, 0, cursor_image->width, cursor_image->height);
        }
        else
#endif
        {
            wl_surface_damage(cursor_surface, 0, 0, cursor_image->width, cursor_image->height);
        }
        wl_surface_commit(cursor_surface);
    }
    else {
        wl_pointer_set_cursor(pointer, serial, NULL, 0, 0);
    }
}
