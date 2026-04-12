#include "WaylandMiniFB_input_keyboard.h"

#include "MiniFB_internal.h"
#include "MiniFB_utf8.h"

#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-compose.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/mman.h>

//-------------------------------------
// Resolve the locale used to build the xkb compose table.
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
// Synchronize MiniFB modifier flags from the current xkb state.
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
// Clear keyboard focus and emit the inactive callback if focus changed.
//-------------------------------------
void
wayland_clear_keyboard_focus_state(SWindowData *window_data, SWindowData_Way *window_data_specific) {
    bool was_active = window_data->is_active;

    window_data->is_active = false;
    reset_keyboard_state(window_data, window_data_specific);

    if (was_active == true) {
        kCall(active_func, false);
    }
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
// Rebuild keyboard state from the compositor's pressed-key list.
// Assumes reset_keyboard_state was called first.
// Does not emit synthetic keyboard callbacks - only synchronizes state.
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

    wayland_clear_keyboard_focus_state(window_data, window_data_specific);
}

//-------------------------------------
// Emit a Unicode codepoint for the given xkb key if one exists.
//-------------------------------------
static void
emit_char_input_from_xkb_state(SWindowData *window_data, SWindowData_Way *window_data_specific, xkb_keycode_t xkb_keycode) {
    uint32_t codepoint = xkb_state_key_get_utf32(window_data_specific->xkb_state, xkb_keycode);
    if (codepoint != 0) {
        kCall(char_input_func, codepoint);
    }
}

//-------------------------------------
// A key changed logical state. The time argument is a timestamp with
// millisecond granularity, with an undefined base.
// serial: serial number of the key event
// time:   timestamp with millisecond granularity
// key:    key that produced the event
// state:  logical key state reported by the compositor
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
        bool   is_pressed = false;
        bool   should_emit_key_press = false;
        bool   should_update_xkb_state = false;
#if defined(WL_KEYBOARD_KEY_STATE_REPEATED_SINCE_VERSION)
        bool   is_repeated = false;
#endif

        switch (state) {
            case WL_KEYBOARD_KEY_STATE_RELEASED:
                should_update_xkb_state = true;
                break;

            case WL_KEYBOARD_KEY_STATE_PRESSED:
                is_pressed = true;
                should_emit_key_press = true;
                should_update_xkb_state = true;
                break;

#if defined(WL_KEYBOARD_KEY_STATE_REPEATED_SINCE_VERSION)
            case WL_KEYBOARD_KEY_STATE_REPEATED:
                is_pressed = true;
                should_emit_key_press = true;
                is_repeated = true;
                break;
#endif

            default:
                MFB_LOG(MFB_LOG_WARNING, "WaylandMiniFB: ignoring unknown wl_keyboard key state %u.", state);
                return;
        }

        if (window_data_specific && window_data_specific->xkb_state) {
            xkb_keycode_t xkb_keycode = (xkb_keycode_t) key + 8;
            if (should_update_xkb_state) {
                xkb_state_update_key(window_data_specific->xkb_state, xkb_keycode, is_pressed ? XKB_KEY_DOWN : XKB_KEY_UP);
                update_mod_keys_from_xkb(window_data, window_data_specific);
            }
            if (should_emit_key_press) {
#if defined(WL_KEYBOARD_KEY_STATE_REPEATED_SINCE_VERSION)
                if (is_repeated) {
                    emit_char_input_from_xkb_state(window_data, window_data_specific, xkb_keycode);
                }
                else
#endif
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
                        xkb_compose_state_reset(window_data_specific->xkb_compose_state);
                    }
                    else if (status == XKB_COMPOSE_COMPOSING) {
                        // Dead key pending - buffer keycode, don't emit
                        if (window_data_specific->compose_sequence_count < 8) {
                            window_data_specific->compose_sequence[window_data_specific->compose_sequence_count++] = xkb_keycode;
                        }
                    }
                    else if (status == XKB_COMPOSE_NOTHING) {
                        emit_char_input_from_xkb_state(window_data, window_data_specific, xkb_keycode);
                    }
                }
                else {
                    emit_char_input_from_xkb_state(window_data, window_data_specific, xkb_keycode);
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
// Listener table for wl_keyboard protocol events.
//-------------------------------------
const struct
wl_keyboard_listener g_wayland_keyboard_listener = {
    .keymap      = keyboard_keymap,
    .enter       = keyboard_enter,
    .leave       = keyboard_leave,
    .key         = keyboard_key,
    .modifiers   = keyboard_modifiers,
#if defined(WL_KEYBOARD_REPEAT_INFO_SINCE_VERSION)
    .repeat_info = keyboard_repeat_info,
#endif
};
