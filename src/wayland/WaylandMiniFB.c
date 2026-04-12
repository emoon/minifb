#ifndef _XOPEN_SOURCE
    #define _XOPEN_SOURCE 700  // for mkstemp, ftruncate, usleep
#endif

#ifndef _DEFAULT_SOURCE
    #define _DEFAULT_SOURCE     // ensure usleep prototype on glibc
#endif

#ifndef _GNU_SOURCE
    #define _GNU_SOURCE         // for ppoll
#endif

#include <MiniFB.h>
#include "generated/xdg-shell-client-protocol.h"
#include "generated/xdg-decoration-client-protocol.h"
#include "generated/fractional-scale-v1-client-protocol.h"
#include "generated/viewporter-client-protocol.h"
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
#include <time.h>
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
#define WAYLAND_DEFAULT_CURSOR_SIZE 32

//-------------------------------------
void init_keycodes();

// Forward declarations for functions used in destroy/configure
// but defined later in the file.
static void slot_destroy(SWaylandBufferSlot *slot);
static bool slot_ensure_buffer(SWaylandBufferSlot *slot, SWindowData_Way *window_data_specific, uint32_t surface_w, uint32_t surface_h);

//-------------------------------------
static inline void
surface_damage(struct wl_surface *surface,
               uint32_t compositor_version,
               int32_t x, int32_t y,
               int32_t w, int32_t h) {
    kUnused(compositor_version);
#if defined(WL_SURFACE_DAMAGE_BUFFER_SINCE_VERSION)
    if (compositor_version >= WL_SURFACE_DAMAGE_BUFFER_SINCE_VERSION) {
        wl_surface_damage_buffer(surface, x, y, w, h);
        return;
    }
#endif
    wl_surface_damage(surface, x, y, w, h);
}

//-------------------------------------
static bool
get_window_data(struct mfb_window *window, const char *func_name, SWindowData **window_data, SWindowData_Way **window_data_specific) {
    const char *function_name = (func_name != NULL) ? func_name : "unknown";

    if (window == NULL) {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: %s called with a null window pointer.", function_name);
        return false;
    }

    *window_data = (SWindowData *) window;
    *window_data_specific = (SWindowData_Way *) (*window_data)->specific;
    if (*window_data_specific == NULL) {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: %s missing Wayland-specific window data.", function_name);
        return false;
    }

    return true;
}

//-------------------------------------
// Dual-queue dispatch infrastructure.
//
// MiniFB uses two Wayland event queues (following Mesa's eglSwapBuffers pattern):
//   window_queue  - shell, input, output, decoration events
//   render_queue  - buffer release, frame callback, sync callback events
//
// This separation prevents input callbacks from firing during blocking waits
// for frame presentation, which would otherwise cause re-entrant updates or
// stalls when the compositor holds buffers.
//-------------------------------------

//-------------------------------------
static void
ts_add(struct timespec *out, const struct timespec *a, const struct timespec *b) {
    out->tv_sec = a->tv_sec + b->tv_sec;
    out->tv_nsec = a->tv_nsec + b->tv_nsec;

    if (out->tv_nsec >= 1000000000L) {
        out->tv_sec += 1;
        out->tv_nsec -= 1000000000L;
    }
}

//-------------------------------------
static void
ts_sub_sat(struct timespec *out, const struct timespec *a, const struct timespec *b) {
    out->tv_sec = a->tv_sec - b->tv_sec;
    out->tv_nsec = a->tv_nsec - b->tv_nsec;

    if (out->tv_nsec < 0) {
        out->tv_sec -= 1;
        out->tv_nsec += 1000000000L;
    }

    if (out->tv_sec < 0) {
        out->tv_sec = 0;
        out->tv_nsec = 0;
    }
}

//-------------------------------------
static struct timespec
ms_to_ts(double ms) {
    struct timespec out = { 0, 0 };

    if (ms <= 0.0) {
        return out;
    }

    out.tv_sec = (time_t) (ms / 1000.0);
    out.tv_nsec = (long) ((ms - ((double) out.tv_sec * 1000.0)) * 1000000.0);

    if (out.tv_nsec >= 1000000000L) {
        out.tv_sec += out.tv_nsec / 1000000000L;
        out.tv_nsec %= 1000000000L;
    }

    return out;
}

//-------------------------------------
static int
poll_display_fd(struct wl_display *display, short events, const struct timespec *timeout) {
    struct pollfd pfd;
    struct timespec now;
    struct timespec deadline = { 0, 0 };
    struct timespec remaining;
    const struct timespec *effective_timeout = timeout;
    int result;

    if (timeout != NULL) {
        clock_gettime(CLOCK_MONOTONIC, &now);
        ts_add(&deadline, &now, timeout);
    }

    pfd.fd = wl_display_get_fd(display);
    pfd.events = events;
    pfd.revents = 0;

    do {
        if (timeout != NULL) {
            clock_gettime(CLOCK_MONOTONIC, &now);
            ts_sub_sat(&remaining, &deadline, &now);
            effective_timeout = &remaining;
        }

        result = ppoll(&pfd, 1, effective_timeout, NULL);
    } while (result == -1 && errno == EINTR);

    return result;
}

//-------------------------------------
static bool
flush_display(struct wl_display *display) {
    int result = wl_display_flush(display);

    if (result == -1 && errno != EAGAIN) {
        return false;
    }

    return true;
}

//-------------------------------------
static int
dispatch_queue_pending_count(SWindowData *window_data, struct wl_event_queue *queue) {
    SWindowData_Way *window_data_specific = (SWindowData_Way *) window_data->specific;

    if (window_data_specific->display == NULL ||
        queue == NULL) {
        return -1;
    }

    return wl_display_dispatch_queue_pending(window_data_specific->display, queue);
}

//-------------------------------------
static bool
dispatch_owned_pending(SWindowData *window_data) {
    SWindowData_Way *window_data_specific = (SWindowData_Way *) window_data->specific;

    if (window_data_specific->display == NULL ||
        window_data_specific->window_queue == NULL ||
        window_data_specific->render_queue == NULL) {
        return false;
    }

    if (dispatch_queue_pending_count(window_data, window_data_specific->render_queue) == -1) {
        return false;
    }

    if (dispatch_queue_pending_count(window_data, window_data_specific->window_queue) == -1) {
        return false;
    }

    return true;
}

//-------------------------------------
static bool
dispatch_render_pending(SWindowData *window_data) {
    SWindowData_Way *window_data_specific = (SWindowData_Way *) window_data->specific;

    if (window_data_specific->display == NULL ||
        window_data_specific->render_queue == NULL) {
        return false;
    }

    if (dispatch_queue_pending_count(window_data, window_data_specific->render_queue) == -1) {
        return false;
    }

    return true;
}

//-------------------------------------
static bool
dispatch_queue_timeout(SWindowData *window_data,
                       struct wl_event_queue *read_queue,
                       const struct timespec *timeout) {
    SWindowData_Way *window_data_specific = (SWindowData_Way *) window_data->specific;
    struct timespec now;
    struct timespec deadline = { 0, 0 };
    struct timespec remaining = { 0, 0 };
    const struct timespec *timeout_ptr = NULL;
    int result;

    if (window_data_specific->display == NULL ||
        window_data_specific->window_queue == NULL ||
        window_data_specific->render_queue == NULL ||
        read_queue == NULL) {
        return false;
    }

    if (timeout != NULL) {
        clock_gettime(CLOCK_MONOTONIC, &now);
        ts_add(&deadline, &now, timeout);
    }

    if (wl_display_prepare_read_queue(window_data_specific->display, read_queue) == -1) {
        result = dispatch_queue_pending_count(window_data, read_queue);
        return result >= 0;
    }

    while (true) {
        result = wl_display_flush(window_data_specific->display);
        if (result != -1 || errno != EAGAIN) {
            break;
        }

        if (timeout != NULL) {
            clock_gettime(CLOCK_MONOTONIC, &now);
            ts_sub_sat(&remaining, &deadline, &now);
            timeout_ptr = &remaining;
        }
        else {
            timeout_ptr = NULL;
        }

        result = poll_display_fd(window_data_specific->display, POLLOUT, timeout_ptr);
        if (result < 0) {
            wl_display_cancel_read(window_data_specific->display);
            return false;
        }

        if (result == 0) {
            wl_display_cancel_read(window_data_specific->display);
            return true;
        }
    }

    if (result < 0) {
        wl_display_cancel_read(window_data_specific->display);
        return false;
    }

    while (true) {
        if (timeout != NULL) {
            clock_gettime(CLOCK_MONOTONIC, &now);
            ts_sub_sat(&remaining, &deadline, &now);
            timeout_ptr = &remaining;
        }
        else {
            timeout_ptr = NULL;
        }

        result = poll_display_fd(window_data_specific->display, POLLIN, timeout_ptr);
        if (result <= 0) {
            wl_display_cancel_read(window_data_specific->display);
            if (result < 0) {
                return false;
            }
            return true;
        }

        if (wl_display_read_events(window_data_specific->display) == -1) {
            return false;
        }

        result = dispatch_queue_pending_count(window_data, read_queue);
        if (result < 0) {
            return false;
        }

        if (result != 0) {
            return true;
        }

        if (wl_display_prepare_read_queue(window_data_specific->display, read_queue) == -1) {
            result = dispatch_queue_pending_count(window_data, read_queue);
            return result >= 0;
        }
    }
}

//-------------------------------------
static bool
dispatch_owned_timeout(SWindowData *window_data, const struct timespec *timeout) {
    SWindowData_Way *window_data_specific = (SWindowData_Way *) window_data->specific;

    return dispatch_queue_timeout(window_data, window_data_specific->window_queue, timeout);
}

//-------------------------------------
static bool
dispatch_owned_non_blocking(SWindowData *window_data) {
    SWindowData_Way *window_data_specific = (SWindowData_Way *) window_data->specific;
    struct wl_event_queue *read_queue;
    struct pollfd pfd;
    int result;

    if (window_data_specific->display == NULL ||
        window_data_specific->window_queue == NULL) {
        return false;
    }

    read_queue = window_data_specific->window_queue;

    if (dispatch_owned_pending(window_data) == false) {
        return false;
    }

    if (wl_display_prepare_read_queue(window_data_specific->display, read_queue) == -1) {
        return dispatch_owned_pending(window_data);
    }

    while (true) {
        result = wl_display_flush(window_data_specific->display);
        if (result != -1 || errno != EAGAIN) {
            break;
        }

        pfd.fd = wl_display_get_fd(window_data_specific->display);
        pfd.events = POLLIN | POLLOUT;
        pfd.revents = 0;

        do {
            result = poll(&pfd, 1, 0);
        } while (result == -1 && errno == EINTR);

        if (result < 0) {
            wl_display_cancel_read(window_data_specific->display);
            return false;
        }

        if (result == 0) {
            wl_display_cancel_read(window_data_specific->display);
            return dispatch_owned_pending(window_data);
        }

        if ((pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
            wl_display_cancel_read(window_data_specific->display);
            return false;
        }

        if ((pfd.revents & POLLIN) != 0) {
            break;
        }
    }

    if (result == -1) {
        wl_display_cancel_read(window_data_specific->display);
        return false;
    }

    pfd.fd = wl_display_get_fd(window_data_specific->display);
    pfd.events = POLLIN;
    pfd.revents = 0;

    do {
        result = poll(&pfd, 1, 0);
    } while (result == -1 && errno == EINTR);

    if (result < 0) {
        wl_display_cancel_read(window_data_specific->display);
        return false;
    }

    if (result == 0) {
        wl_display_cancel_read(window_data_specific->display);
        return dispatch_owned_pending(window_data);
    }

    if (wl_display_read_events(window_data_specific->display) == -1) {
        return false;
    }

    if (dispatch_owned_pending(window_data) == false) {
        return false;
    }

    return flush_display(window_data_specific->display);
}

//-------------------------------------
static bool
dispatch_render_blocking(SWindowData *window_data) {
    SWindowData_Way *window_data_specific = (SWindowData_Way *) window_data->specific;

    if (window_data_specific->display == NULL ||
        window_data_specific->render_queue == NULL) {
        return false;
    }

    return dispatch_queue_timeout(window_data, window_data_specific->render_queue, NULL);
}

//-------------------------------------
// Throttle infrastructure.
//
// surface_throttle() waits for the PREVIOUS frame's callback before starting
// the next frame. This is the key difference from the old inline-wait pattern:
// the CPU can prepare the next frame while the compositor presents the current
// one (pipelined rendering).
//
// g_use_wayland_frame_callback_throttle controls whether wl_surface_frame is
// used (vsync-like) or skipped (mailbox-style, throttled only by wl_display_sync).
//-------------------------------------

//-------------------------------------
extern double g_time_for_frame;
extern bool   g_use_hardware_sync;
bool          g_use_wayland_frame_callback_throttle = true;
//-------------------------------------

//-------------------------------------
static void
throttle_done(void *data, struct wl_callback *callback, uint32_t time) {
    SWindowData_Way *window_data_specific = (SWindowData_Way *) data;

    kUnused(time);

    if (window_data_specific != NULL) {
        window_data_specific->throttle_callback = NULL;
    }

    wl_callback_destroy(callback);
}

//-------------------------------------
static const struct
wl_callback_listener g_throttle_listener = {
    throttle_done,
};

//-------------------------------------
static bool
surface_throttle(SWindowData *window_data) {
    SWindowData_Way *window_data_specific = (SWindowData_Way *) window_data->specific;

    if (window_data_specific->surface_wrapper == NULL) {
        return false;
    }

    while (window_data_specific->throttle_callback != NULL) {
        if (window_data->close == true) {
            return false;
        }

        if (dispatch_render_blocking(window_data) == false) {
            return false;
        }
    }

    if (g_use_wayland_frame_callback_throttle == false) {
        return true;
    }

    window_data_specific->throttle_callback = wl_surface_frame(window_data_specific->surface_wrapper);
    if (window_data_specific->throttle_callback == NULL) {
        return false;
    }

    wl_callback_add_listener(window_data_specific->throttle_callback, &g_throttle_listener, window_data_specific);
    return true;
}

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

    if (window_data_specific->viewport) {
        wp_viewport_destroy(window_data_specific->viewport);
        window_data_specific->viewport = NULL;
    }

    if (window_data_specific->viewporter) {
        wp_viewporter_destroy(window_data_specific->viewporter);
        window_data_specific->viewporter = NULL;
    }

    if (window_data_specific->throttle_callback) {
        wl_callback_destroy(window_data_specific->throttle_callback);
        window_data_specific->throttle_callback = NULL;
    }

    if (window_data_specific->surface_wrapper) {
        wl_proxy_wrapper_destroy(window_data_specific->surface_wrapper);
        window_data_specific->surface_wrapper = NULL;
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

    for (uint32_t i = 0; i < window_data_specific->cursor_theme_cache_count; ++i) {
        if (window_data_specific->cursor_theme_cache[i]) {
            wl_cursor_theme_destroy(window_data_specific->cursor_theme_cache[i]);
            window_data_specific->cursor_theme_cache[i] = NULL;
        }
        window_data_specific->default_cursor_cache[i] = NULL;
        window_data_specific->cursor_theme_cache_scales[i] = 0;
    }
    window_data_specific->cursor_theme_cache_count = 0;
    window_data_specific->cursor_theme = NULL;
    window_data_specific->default_cursor = NULL;
    window_data_specific->cursor_theme_scale = 0;

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

    if (window_data_specific->window_display_wrapper) {
        wl_proxy_wrapper_destroy(window_data_specific->window_display_wrapper);
        window_data_specific->window_display_wrapper = NULL;
    }

    if (window_data_specific->render_display_wrapper) {
        wl_proxy_wrapper_destroy(window_data_specific->render_display_wrapper);
        window_data_specific->render_display_wrapper = NULL;
    }

    if (window_data_specific->render_queue) {
        wl_event_queue_destroy(window_data_specific->render_queue);
        window_data_specific->render_queue = NULL;
    }

    if (window_data_specific->window_queue) {
        wl_event_queue_destroy(window_data_specific->window_queue);
        window_data_specific->window_queue = NULL;
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
static void
clear_keyboard_focus_state(SWindowData *window_data, SWindowData_Way *window_data_specific) {
    bool was_active = window_data->is_active;

    window_data->is_active = false;
    reset_keyboard_state(window_data, window_data_specific);

    if (was_active == true) {
        kCall(active_func, false);
    }
}

//-------------------------------------
static void
invalidate_pointer_serial_state(SWindowData_Way *window_data_specific) {
    window_data_specific->pointer_serial = 0;
    window_data_specific->pointer_enter_serial = 0;
    window_data_specific->pointer_serial_valid = 0;
}

//-------------------------------------
static uint32_t
get_cursor_buffer_scale(const SWindowData_Way *window_data_specific) {
    uint32_t scale = 1;

#if defined(WL_SURFACE_SET_BUFFER_SCALE_SINCE_VERSION)
    if (window_data_specific->compositor_version >= WL_SURFACE_SET_BUFFER_SCALE_SINCE_VERSION) {
        if (window_data_specific->preferred_scale_120 > 0) {
            scale = (window_data_specific->preferred_scale_120 + 119u) / 120u;
        }
        else if (window_data_specific->integer_output_scale > 0) {
            scale = window_data_specific->integer_output_scale;
        }
    }
#endif

    if (scale == 0) {
        scale = 1;
    }

    return scale;
}

//-------------------------------------
static int
find_cursor_theme_cache_index(const SWindowData_Way *window_data_specific, uint32_t scale) {
    for (uint32_t i = 0; i < window_data_specific->cursor_theme_cache_count; ++i) {
        if (window_data_specific->cursor_theme_cache_scales[i] == scale) {
            return (int) i;
        }
    }

    return -1;
}

//-------------------------------------
static bool
ensure_cursor_theme_for_scale(SWindowData_Way *window_data_specific, uint32_t scale) {
    if (scale == 0) {
        scale = 1;
    }

    if (window_data_specific->cursor_theme != NULL &&
        window_data_specific->default_cursor != NULL &&
        window_data_specific->cursor_theme_scale == scale) {
        return true;
    }

    int cache_index = find_cursor_theme_cache_index(window_data_specific, scale);
    if (cache_index >= 0) {
        window_data_specific->cursor_theme = window_data_specific->cursor_theme_cache[cache_index];
        window_data_specific->default_cursor = window_data_specific->default_cursor_cache[cache_index];
        window_data_specific->cursor_theme_scale = scale;
        return true;
    }

    if (window_data_specific->shm == NULL) {
        return false;
    }

    if (window_data_specific->cursor_theme_cache_count >= WAYLAND_CURSOR_THEME_CACHE_SIZE) {
        MFB_LOG(MFB_LOG_WARNING, "WaylandMiniFB: cursor theme cache is full; keeping the current cursor theme.");
        return window_data_specific->cursor_theme != NULL && window_data_specific->default_cursor != NULL;
    }

    if (scale > (uint32_t) (INT_MAX / WAYLAND_DEFAULT_CURSOR_SIZE)) {
        return false;
    }

    int theme_size = (int) (WAYLAND_DEFAULT_CURSOR_SIZE * scale);
    struct wl_cursor_theme *theme = wl_cursor_theme_load(NULL, theme_size, window_data_specific->shm);
    if (theme == NULL) {
        return false;
    }

    struct wl_cursor *cursor = wl_cursor_theme_get_cursor(theme, "left_ptr");
    if (cursor == NULL || cursor->image_count == 0) {
        wl_cursor_theme_destroy(theme);
        return false;
    }

    uint32_t cache_slot = window_data_specific->cursor_theme_cache_count++;
    window_data_specific->cursor_theme_cache[cache_slot] = theme;
    window_data_specific->default_cursor_cache[cache_slot] = cursor;
    window_data_specific->cursor_theme_cache_scales[cache_slot] = scale;

    window_data_specific->cursor_theme = theme;
    window_data_specific->default_cursor = cursor;
    window_data_specific->cursor_theme_scale = scale;

    return true;
}

//-------------------------------------
static void
refresh_cursor_surface(SWindowData *window_data) {
    SWindowData_Way *window_data_specific = window_data ? (SWindowData_Way *) window_data->specific : NULL;
    if (window_data_specific == NULL ||
        window_data_specific->pointer == NULL ||
        window_data_specific->cursor_surface == NULL ||
        window_data_specific->pointer_serial_valid == 0) {
        return;
    }

    uint32_t serial = window_data_specific->pointer_enter_serial;
    if (window_data->is_cursor_visible == false) {
        wl_pointer_set_cursor(window_data_specific->pointer, serial, NULL, 0, 0);
        return;
    }

    uint32_t cursor_scale = get_cursor_buffer_scale(window_data_specific);
    if (ensure_cursor_theme_for_scale(window_data_specific, cursor_scale) == false) {
        MFB_LOG(MFB_LOG_WARNING, "WaylandMiniFB: failed to load cursor theme for scale %u.", cursor_scale);
        return;
    }

    uint32_t applied_scale = 1;
#if defined(WL_SURFACE_SET_BUFFER_SCALE_SINCE_VERSION)
    if (window_data_specific->compositor_version >= WL_SURFACE_SET_BUFFER_SCALE_SINCE_VERSION) {
        applied_scale = window_data_specific->cursor_theme_scale > 0 ? window_data_specific->cursor_theme_scale : 1u;
    }
#endif

    struct wl_cursor *cursor = window_data_specific->default_cursor;
    if (cursor == NULL || cursor->image_count == 0) {
        return;
    }

    struct wl_cursor_image *image = cursor->images[0];
    if (applied_scale > 1 &&
        (image->width % applied_scale != 0 || image->height % applied_scale != 0)) {
        MFB_LOG(MFB_LOG_WARNING, "WaylandMiniFB: cursor image %ux%u is incompatible with buffer scale %u; retrying with scale 1.",
                image->width, image->height, applied_scale);

        if (ensure_cursor_theme_for_scale(window_data_specific, 1) == false) {
            return;
        }

        cursor = window_data_specific->default_cursor;
        if (cursor == NULL || cursor->image_count == 0) {
            return;
        }

        image = cursor->images[0];
        applied_scale = 1;
    }

    struct wl_buffer *buffer = wl_cursor_image_get_buffer(image);
    if (buffer == NULL) {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: wl_cursor_image_get_buffer returned NULL.");
        return;
    }

#if defined(WL_SURFACE_SET_BUFFER_SCALE_SINCE_VERSION)
    if (window_data_specific->compositor_version >= WL_SURFACE_SET_BUFFER_SCALE_SINCE_VERSION) {
        wl_surface_set_buffer_scale(window_data_specific->cursor_surface, (int32_t) applied_scale);
    }
#endif

    int32_t hotspot_x = (int32_t) image->hotspot_x;
    int32_t hotspot_y = (int32_t) image->hotspot_y;
    if (applied_scale > 1) {
        hotspot_x = (hotspot_x + (int32_t) (applied_scale / 2)) / (int32_t) applied_scale;
        hotspot_y = (hotspot_y + (int32_t) (applied_scale / 2)) / (int32_t) applied_scale;
    }

    wl_pointer_set_cursor(window_data_specific->pointer, serial,
                          window_data_specific->cursor_surface,
                          hotspot_x, hotspot_y);
    wl_surface_attach(window_data_specific->cursor_surface, buffer, 0, 0);
    surface_damage(window_data_specific->cursor_surface,
                   window_data_specific->compositor_version,
                   0, 0,
                   image->width, image->height);
    wl_surface_commit(window_data_specific->cursor_surface);
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

    clear_keyboard_focus_state(window_data, window_data_specific);
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
    kUnused(pointer);
    kUnused(surface);

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

    refresh_cursor_surface(window_data);

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
        invalidate_pointer_serial_state(window_data_specific);
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

#if defined(WL_POINTER_AXIS_VALUE120_SINCE_VERSION)

//-------------------------------------
// High-resolution wheel scroll information.
// Currently unused; MiniFB keeps handling scroll through axis/axis_discrete.
//-------------------------------------
static void
axis_value120(void *data, struct wl_pointer *pointer, uint32_t axis, int32_t value120) {
    kUnused(data);
    kUnused(pointer);
    kUnused(axis);
    kUnused(value120);
}

#endif

#if defined(WL_POINTER_AXIS_RELATIVE_DIRECTION_SINCE_VERSION)

//-------------------------------------
// Physical direction of the entity causing the axis motion.
// Currently unused; MiniFB consumes logical scroll direction only.
//-------------------------------------
static void
axis_relative_direction(void *data, struct wl_pointer *pointer, uint32_t axis, uint32_t direction) {
    kUnused(data);
    kUnused(pointer);
    kUnused(axis);
    kUnused(direction);
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
#if defined(WL_POINTER_AXIS_VALUE120_SINCE_VERSION)
    .axis_value120 = axis_value120,
#endif
#if defined(WL_POINTER_AXIS_RELATIVE_DIRECTION_SINCE_VERSION)
    .axis_relative_direction = axis_relative_direction,
#endif
};

//-------------------------------------
static void
seat_capabilities(void *data, struct wl_seat *seat, enum wl_seat_capability caps) {
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
        clear_keyboard_focus_state(window_data, window_data_specific);
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
        invalidate_pointer_serial_state(window_data_specific);
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
    refresh_cursor_surface(window_data);
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
        refresh_cursor_surface(window_data);
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
        refresh_cursor_surface(window_data);
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
        refresh_cursor_surface(window_data);
    }
}

#if defined(WL_SURFACE_PREFERRED_BUFFER_SCALE_SINCE_VERSION)

//-------------------------------------
// Compositor-preferred integer buffer scale for this surface.
// Currently unused; the backend derives scale from fractional-scale/output data.
//-------------------------------------
static void
surface_preferred_buffer_scale(void *data, struct wl_surface *surface, int32_t factor) {
    kUnused(data);
    kUnused(surface);
    kUnused(factor);
}

#endif

#if defined(WL_SURFACE_PREFERRED_BUFFER_TRANSFORM_SINCE_VERSION)

//-------------------------------------
// Compositor-preferred buffer transform for this surface.
// Currently unused; the backend does not render with transformed buffers.
//-------------------------------------
static void
surface_preferred_buffer_transform(void *data, struct wl_surface *surface, uint32_t transform) {
    kUnused(data);
    kUnused(surface);
    kUnused(transform);
}

#endif

//-------------------------------------
static const struct wl_surface_listener surface_listener = {
    .enter = surface_enter,
    .leave = surface_leave,
#if defined(WL_SURFACE_PREFERRED_BUFFER_SCALE_SINCE_VERSION)
    .preferred_buffer_scale = surface_preferred_buffer_scale,
#endif
#if defined(WL_SURFACE_PREFERRED_BUFFER_TRANSFORM_SINCE_VERSION)
    .preferred_buffer_transform = surface_preferred_buffer_transform,
#endif
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

    else if (strcmp(iface, "wp_viewporter") == 0) {
        uint32_t client_version = (uint32_t) wp_viewporter_interface.version;
        uint32_t use_version = version < client_version ? version : client_version;
        window_data_specific->viewporter = (struct wp_viewporter *)
            wl_registry_bind(registry, id, &wp_viewporter_interface, use_version);
        MFB_LOG(MFB_LOG_TRACE, "wp_viewporter: server=%u client=%u using=%u", version, client_version, use_version);
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
                refresh_cursor_surface(window_data);
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
        if (slot_ensure_buffer(slot, window_data_specific, init_w, init_h) == false) {
            MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: failed to create initial buffer in first configure.");
            window_data->close = true;
        }
        else {
            wl_surface_attach(window_data_specific->surface, slot->wl_buf, 0, 0);

            surface_damage(window_data_specific->surface, window_data_specific->compositor_version,
                           0, 0, init_w, init_h);

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
            window_data->must_resize_context = true;
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
// Each slot owns its own fd, mmap, and wl_shm_pool - no shared offsets.
// Returns true on success (or if no rebuild was needed).
//-------------------------------------
static bool
slot_ensure_buffer(SWaylandBufferSlot *slot,
                   SWindowData_Way *window_data_specific,
                   uint32_t surface_w, uint32_t surface_h) {
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

    slot->pool = wl_shm_create_pool(window_data_specific->shm, slot->fd, (int) pool_size);
    if (slot->pool == NULL) {
        munmap(slot->shm_ptr, pool_size);
        slot->shm_ptr = NULL;
        close(slot->fd);
        slot->fd = -1;
        slot->pool_size = 0;
        return false;
    }

    // Route pool to render_queue so buffer release events go there.
    if (window_data_specific->render_queue != NULL) {
        wl_proxy_set_queue((struct wl_proxy *) slot->pool, window_data_specific->render_queue);
    }

    slot->wl_buf = wl_shm_pool_create_buffer(slot->pool, 0,
                        surface_w, surface_h,
                        stride, window_data_specific->shm_format);
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
        if (!slot_ensure_buffer(slot, window_data_specific, width, height)) {
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

    // Process events until we get the configure event and the surface is mapped.
    // Configure events arrive on window_queue (shell objects are on that queue).
    while (window_data->is_initialized == false && window_data->close == false) {
        if (wl_display_dispatch_queue(window_data_specific->display,
                                      window_data_specific->window_queue) == -1) {
            MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: wl_display_dispatch_queue failed while waiting for initial configure event.");
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

    // Dual-queue setup: window_queue for shell/input/output events,
    // render_queue for buffer release/frame callback/sync events.
    window_data_specific->window_queue = wl_display_create_queue(window_data_specific->display);
    window_data_specific->render_queue = wl_display_create_queue(window_data_specific->display);
    window_data_specific->window_display_wrapper = wl_proxy_create_wrapper(window_data_specific->display);
    window_data_specific->render_display_wrapper = wl_proxy_create_wrapper(window_data_specific->display);

    if (window_data_specific->window_queue == NULL ||
        window_data_specific->render_queue == NULL ||
        window_data_specific->window_display_wrapper == NULL ||
        window_data_specific->render_display_wrapper == NULL) {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: failed to create event queues or display wrappers.");
        goto out;
    }

    wl_proxy_set_queue((struct wl_proxy *) window_data_specific->window_display_wrapper,
                       window_data_specific->window_queue);
    wl_proxy_set_queue((struct wl_proxy *) window_data_specific->render_display_wrapper,
                       window_data_specific->render_queue);

    // Registry via window_display_wrapper so all globals route to window_queue.
    window_data_specific->registry = wl_display_get_registry(window_data_specific->window_display_wrapper);
    if (!window_data_specific->registry) {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: wl_display_get_registry returned NULL.");
        goto out;
    }
    wl_registry_add_listener(window_data_specific->registry, &registry_listener, window_data);

    init_keycodes();

    // Two roundtrips on window_queue: first gets globals, second ensures
    // all format events (from wl_shm) have been received.
    if (wl_display_roundtrip_queue(window_data_specific->display,
                                   window_data_specific->window_queue) == -1) {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: failed to initialize Wayland globals (first roundtrip).");
        goto out;
    }
    if (window_data_specific->shm_format == -1u &&
        wl_display_roundtrip_queue(window_data_specific->display,
                                   window_data_specific->window_queue) == -1) {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: failed to initialize Wayland globals (second roundtrip).");
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

    // Create surface wrapper routed to render_queue for frame callbacks and buffer ops.
    window_data_specific->surface_wrapper = wl_proxy_create_wrapper(window_data_specific->surface);
    if (window_data_specific->surface_wrapper == NULL) {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: failed to create surface proxy wrapper.");
        goto out;
    }
    wl_proxy_set_queue((struct wl_proxy *) window_data_specific->surface_wrapper,
                       window_data_specific->render_queue);

    if (window_data_specific->viewporter) {
        window_data_specific->viewport =
            wp_viewporter_get_viewport(window_data_specific->viewporter, window_data_specific->surface);
    }
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

    // Avoid retroactive callback to resize
    window_data->must_resize_context = false;

    MFB_LOG(MFB_LOG_DEBUG, "Window created using Wayland API");

    return (struct mfb_window *) window_data;

out:
    MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: mfb_open_ex failed and is cleaning up partially initialized resources.");
    destroy(window_data);

    return NULL;
}

//-------------------------------------
typedef struct {
    uint32_t logical_surface_width;
    uint32_t logical_surface_height;
    uint32_t logical_dst_offset_x;
    uint32_t logical_dst_offset_y;
    uint32_t logical_dst_width;
    uint32_t logical_dst_height;
    uint32_t physical_surface_width;
    uint32_t physical_surface_height;
    uint32_t physical_dst_offset_x;
    uint32_t physical_dst_offset_y;
    uint32_t physical_dst_width;
    uint32_t physical_dst_height;
    uint32_t integer_scale;
    bool     use_buffer_scale;
    bool     use_fractional;
} SWaylandPresentationMetrics;

//-------------------------------------
typedef enum {
    WAYLAND_SLOT_ACQUIRE_OK = 0,
    WAYLAND_SLOT_ACQUIRE_BUSY,
    WAYLAND_SLOT_ACQUIRE_ERROR,
} EWaylandSlotAcquireStatus;

//-------------------------------------
static void
compute_presentation_metrics(const SWindowData *window_data,
                             const SWindowData_Way *window_data_specific,
                             SWaylandPresentationMetrics *metrics) {
    metrics->logical_surface_width  = window_data->window_width;
    metrics->logical_surface_height = window_data->window_height;
    metrics->logical_dst_offset_x   = window_data->dst_offset_x;
    metrics->logical_dst_offset_y   = window_data->dst_offset_y;
    metrics->logical_dst_width      = window_data->dst_width;
    metrics->logical_dst_height     = window_data->dst_height;
    metrics->integer_scale          = window_data_specific->integer_output_scale;
    metrics->use_buffer_scale       = false;
    metrics->use_fractional         = false;

    if (metrics->integer_scale == 0) {
        metrics->integer_scale = 1;
    }

    // 4B: fractional HiDPI via wp_viewporter - takes priority over 4A.
    if (window_data_specific->viewport != NULL
        && window_data_specific->preferred_scale_120 > 0) {
        float fscale = (float) window_data_specific->preferred_scale_120 / 120.0f;
        metrics->use_fractional          = true;
        metrics->physical_surface_width  = (uint32_t) ((float) metrics->logical_surface_width  * fscale + 0.5f);
        metrics->physical_surface_height = (uint32_t) ((float) metrics->logical_surface_height * fscale + 0.5f);
        metrics->physical_dst_offset_x   = (uint32_t) ((float) metrics->logical_dst_offset_x   * fscale + 0.5f);
        metrics->physical_dst_offset_y   = (uint32_t) ((float) metrics->logical_dst_offset_y   * fscale + 0.5f);
        metrics->physical_dst_width      = (uint32_t) ((float) metrics->logical_dst_width      * fscale + 0.5f);
        metrics->physical_dst_height     = (uint32_t) ((float) metrics->logical_dst_height     * fscale + 0.5f);
        if (metrics->physical_surface_width  == 0) { metrics->physical_surface_width  = 1; }
        if (metrics->physical_surface_height == 0) { metrics->physical_surface_height = 1; }
        // Clamp dst rect to physical surface bounds to prevent stretch_image
        // writing outside the slot buffer when independent rounding of offset
        // and size pushes offset + size beyond the physical surface edge.
        if (metrics->physical_dst_offset_x > metrics->physical_surface_width) {
            metrics->physical_dst_offset_x = metrics->physical_surface_width;
        }
        if (metrics->physical_dst_offset_y > metrics->physical_surface_height) {
            metrics->physical_dst_offset_y = metrics->physical_surface_height;
        }
        if (metrics->physical_dst_offset_x + metrics->physical_dst_width > metrics->physical_surface_width) {
            metrics->physical_dst_width = metrics->physical_surface_width - metrics->physical_dst_offset_x;
        }
        if (metrics->physical_dst_offset_y + metrics->physical_dst_height > metrics->physical_surface_height) {
            metrics->physical_dst_height = metrics->physical_surface_height - metrics->physical_dst_offset_y;
        }
        return;
    }

    // 4A: integer HiDPI via wl_surface_set_buffer_scale.
#if defined(WL_SURFACE_SET_BUFFER_SCALE_SINCE_VERSION)
    if (window_data_specific->compositor_version >= WL_SURFACE_SET_BUFFER_SCALE_SINCE_VERSION
        && metrics->integer_scale > 1) {
        metrics->use_buffer_scale = true;
    }
#endif

    if (metrics->use_buffer_scale == true
        && (metrics->logical_surface_width  > UINT32_MAX / metrics->integer_scale
        ||  metrics->logical_surface_height > UINT32_MAX / metrics->integer_scale
        ||  metrics->logical_dst_offset_x   > UINT32_MAX / metrics->integer_scale
        ||  metrics->logical_dst_offset_y   > UINT32_MAX / metrics->integer_scale
        ||  metrics->logical_dst_width      > UINT32_MAX / metrics->integer_scale
        ||  metrics->logical_dst_height     > UINT32_MAX / metrics->integer_scale)) {
        metrics->use_buffer_scale = false;
    }

    uint32_t applied_scale = metrics->use_buffer_scale ? metrics->integer_scale : 1u;

    metrics->physical_surface_width  = metrics->logical_surface_width  * applied_scale;
    metrics->physical_surface_height = metrics->logical_surface_height * applied_scale;
    metrics->physical_dst_offset_x   = metrics->logical_dst_offset_x   * applied_scale;
    metrics->physical_dst_offset_y   = metrics->logical_dst_offset_y   * applied_scale;
    metrics->physical_dst_width      = metrics->logical_dst_width      * applied_scale;
    metrics->physical_dst_height     = metrics->logical_dst_height     * applied_scale;
}

//-------------------------------------
static EWaylandSlotAcquireStatus
acquire_presentation_slot(SWindowData *window_data,
                          SWindowData_Way *window_data_specific,
                          const SWaylandPresentationMetrics *metrics,
                          SWaylandBufferSlot **out_slot) {
    *out_slot = NULL;

    // Dispatch render pending to process any buffer release events.
    dispatch_render_pending(window_data);

    while (true) {
        int start = window_data_specific->front_slot;
        for (int i = 0; i < WAYLAND_BUFFER_SLOTS; ++i) {
            int idx = (start + i) % WAYLAND_BUFFER_SLOTS;
            SWaylandBufferSlot *slot = &window_data_specific->slots[idx];

            if (slot->busy) {
                continue;
            }

            if (slot_ensure_buffer(slot, window_data_specific,
                                   metrics->physical_surface_width,
                                   metrics->physical_surface_height) == false) {
                MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: wl_buffer recreation failed for slot %d.", idx);
                return WAYLAND_SLOT_ACQUIRE_ERROR;
            }

            *out_slot = slot;
            window_data_specific->front_slot = (idx + 1) % WAYLAND_BUFFER_SLOTS;
            return WAYLAND_SLOT_ACQUIRE_OK;
        }

        // All slots busy - block until a release event arrives.
        if (window_data->close == true) {
            return WAYLAND_SLOT_ACQUIRE_ERROR;
        }

        if (dispatch_render_blocking(window_data) == false) {
            MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: event dispatch failed while waiting for buffer release.");
            return WAYLAND_SLOT_ACQUIRE_ERROR;
        }
    }
}

//-------------------------------------
static void
compose_presentation_buffer(const SWindowData *window_data,
                            const SWaylandPresentationMetrics *metrics,
                            const void *buffer,
                            uint32_t *shm_ptr) {
    if (metrics->use_buffer_scale == false
        && metrics->use_fractional == false
        && window_data->buffer_width == metrics->logical_dst_width
        && window_data->buffer_height == metrics->logical_dst_height
        && metrics->logical_dst_offset_x == 0
        && metrics->logical_dst_offset_y == 0
        && metrics->logical_surface_width == metrics->logical_dst_width
        && metrics->logical_surface_height == metrics->logical_dst_height) {
        // Fast path: no scaling and source fills the entire surface exactly.
        memcpy(shm_ptr, buffer,
               (size_t) metrics->physical_surface_width * metrics->physical_surface_height * sizeof(uint32_t));
    }
    else {
        // Clear the physical buffer, then stretch the source into the viewport.
        // When use_buffer_scale is true, physical_* coords are in physical pixels.
        memset(shm_ptr, 0,
               (size_t) metrics->physical_surface_width * metrics->physical_surface_height * sizeof(uint32_t));
        stretch_image(
            (uint32_t *) buffer, 0, 0,
            window_data->buffer_width, window_data->buffer_height, window_data->buffer_width,
            shm_ptr, metrics->physical_dst_offset_x, metrics->physical_dst_offset_y,
            metrics->physical_dst_width, metrics->physical_dst_height, metrics->physical_surface_width);
    }
}

//-------------------------------------
static mfb_update_state
present_presentation_buffer(SWindowData *window_data,
                            SWindowData_Way *window_data_specific,
                            SWaylandBufferSlot *active_slot,
                            const SWaylandPresentationMetrics *metrics) {
    kUnused(window_data);

    // Attach buffer via surface_wrapper (render-path proxy).
    wl_surface_attach(window_data_specific->surface_wrapper, active_slot->wl_buf, 0, 0);

    // HiDPI: set buffer scale and viewport on the real surface (window config).
#if defined(WL_SURFACE_SET_BUFFER_SCALE_SINCE_VERSION)
    if (window_data_specific->compositor_version >= WL_SURFACE_SET_BUFFER_SCALE_SINCE_VERSION) {
        wl_surface_set_buffer_scale(window_data_specific->surface,
                                    (int32_t) (metrics->use_buffer_scale ? metrics->integer_scale : 1u));
    }
#endif
    if (window_data_specific->viewport != NULL) {
        if (metrics->use_fractional) {
            wp_viewport_set_destination(window_data_specific->viewport,
                                        (int32_t) metrics->logical_surface_width,
                                        (int32_t) metrics->logical_surface_height);
        }
        else {
            wp_viewport_set_destination(window_data_specific->viewport, -1, -1);
        }
    }

    surface_damage(window_data_specific->surface_wrapper,
                   window_data_specific->compositor_version,
                   0, 0,
                   (int32_t) metrics->physical_surface_width,
                   (int32_t) metrics->physical_surface_height);

    wl_surface_commit(window_data_specific->surface_wrapper);
    active_slot->busy = 1;

    // Sync fallback: when no frame callback is pending (throttle off or first
    // frame), use wl_display_sync as a minimal throttle to avoid runaway submits.
    if (window_data_specific->throttle_callback == NULL) {
        window_data_specific->throttle_callback =
            wl_display_sync(window_data_specific->render_display_wrapper);
        if (window_data_specific->throttle_callback == NULL) {
            MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: wl_display_sync returned NULL.");
            return MFB_STATE_INTERNAL_ERROR;
        }
        wl_callback_add_listener(window_data_specific->throttle_callback,
                                 &g_throttle_listener, window_data_specific);
    }

    if (flush_display(window_data_specific->display) == false) {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: flush failed after commit.");
        return MFB_STATE_INTERNAL_ERROR;
    }

    return MFB_STATE_OK;
}

//-------------------------------------
mfb_update_state
mfb_update_ex(struct mfb_window *window, void *buffer, unsigned width, unsigned height) {
    uint32_t buffer_stride = 0;
    size_t buffer_total_bytes = 0;
    SWindowData *window_data;
    SWindowData_Way *window_data_specific;

    if (get_window_data(window, __func__, &window_data, &window_data_specific) == false) {
        return MFB_STATE_INVALID_WINDOW;
    }

    if (window_data->close == true) {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: mfb_update_ex aborted because the window is marked for close.");
        destroy(window_data);
        return MFB_STATE_EXIT;
    }

    if (buffer == NULL) {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: mfb_update_ex called with a null buffer.");
        return MFB_STATE_INVALID_BUFFER;
    }

    if (calculate_buffer_layout(width, height, &buffer_stride, &buffer_total_bytes) == false) {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: mfb_update_ex called with invalid buffer size %ux%u.", width, height);
        return MFB_STATE_INVALID_BUFFER;
    }

    if (window_data_specific->display == NULL
        || wl_display_get_error(window_data_specific->display) != 0) {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: invalid Wayland display state during mfb_update_ex.");
        return MFB_STATE_INTERNAL_ERROR;
    }

    // 1. Dispatch window events (input, resize, close) non-blocking.
    if (dispatch_owned_non_blocking(window_data) == false) {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: event dispatch failed in mfb_update_ex.");
        return MFB_STATE_INTERNAL_ERROR;
    }

    if (window_data->close == true) {
        destroy(window_data);
        return MFB_STATE_EXIT;
    }

    // Update buffer dimensions.
    if (window_data->buffer_width != width || window_data->buffer_height != height) {
        window_data->buffer_width  = width;
        window_data->buffer_height = height;
        window_data->buffer_stride = buffer_stride;
    }

    // 2. Wait for PREVIOUS frame's callback (pipelined throttle).
    if (surface_throttle(window_data) == false) {
        if (window_data->close == true) {
            destroy(window_data);
            return MFB_STATE_EXIT;
        }
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: frame throttle failed.");
        return MFB_STATE_INTERNAL_ERROR;
    }

    if (window_data->close == true) {
        destroy(window_data);
        return MFB_STATE_EXIT;
    }

    // 3. Compute metrics AFTER dispatch (captures latest size from resize events).
    SWaylandPresentationMetrics metrics;
    compute_presentation_metrics(window_data, window_data_specific, &metrics);

    // 4. Acquire a free buffer slot (blocks on render_queue if all busy).
    SWaylandBufferSlot *active_slot = NULL;
    EWaylandSlotAcquireStatus slot_status =
        acquire_presentation_slot(window_data, window_data_specific, &metrics, &active_slot);

    // Always reset per-frame input state.
    window_data->mouse_wheel_x = 0.0f;
    window_data->mouse_wheel_y = 0.0f;

    if (slot_status != WAYLAND_SLOT_ACQUIRE_OK || active_slot == NULL) {
        if (window_data->close == true) {
            destroy(window_data);
            return MFB_STATE_EXIT;
        }
        return MFB_STATE_INTERNAL_ERROR;
    }

    // 5. Compose and present.
    compose_presentation_buffer(window_data, &metrics, buffer, active_slot->shm_ptr);
    mfb_update_state state = present_presentation_buffer(window_data, window_data_specific, active_slot, &metrics);

    if (window_data->must_resize_context && state == MFB_STATE_OK) {
        window_data->must_resize_context = false;
        kCall(resize_func, window_data->window_width, window_data->window_height);
    }

    return state;
}

//-------------------------------------
mfb_update_state
mfb_update_events(struct mfb_window *window) {
    SWindowData *window_data;
    SWindowData_Way *window_data_specific;

    if (get_window_data(window, __func__, &window_data, &window_data_specific) == false) {
        return MFB_STATE_INVALID_WINDOW;
    }

    if (window_data->close == true) {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: mfb_update_events aborted because the window is marked for close.");
        destroy(window_data);
        return MFB_STATE_EXIT;
    }

    if (window_data_specific->display == NULL
        || wl_display_get_error(window_data_specific->display) != 0) {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: invalid Wayland display state during mfb_update_events.");
        return MFB_STATE_INTERNAL_ERROR;
    }

    window_data->mouse_wheel_x = 0.0f;
    window_data->mouse_wheel_y = 0.0f;

    // Non-blocking dispatch of both queues for X11-like event polling behavior.
    if (dispatch_owned_non_blocking(window_data) == false) {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: event dispatch failed in mfb_update_events.");
        return MFB_STATE_INTERNAL_ERROR;
    }

    if (window_data->close == true) {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: mfb_update_events detected close request after event dispatch.");
        destroy(window_data);
        return MFB_STATE_EXIT;
    }

    if (window_data->must_resize_context) {
        window_data->must_resize_context = false;
        kCall(resize_func, window_data->window_width, window_data->window_height);
    }

    return MFB_STATE_OK;
}

//-------------------------------------
bool
mfb_wait_sync(struct mfb_window *window) {
    SWindowData *window_data;
    SWindowData_Way *window_data_specific;

    if (get_window_data(window, __func__, &window_data, &window_data_specific) == false) {
        return false;
    }

    if (window_data->close == true) {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: mfb_wait_sync aborted because the window is marked for close.");
        destroy(window_data);
        return false;
    }

    if (window_data_specific->display == NULL) {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: mfb_wait_sync has a null Wayland display handle.");
        return false;
    }
    if (window_data_specific->timer == NULL) {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: mfb_wait_sync missing frame timer state.");
        return false;
    }

    window_data->mouse_wheel_x = 0.0f;
    window_data->mouse_wheel_y = 0.0f;

    // Dispatch events once before pacing.
    if (dispatch_owned_non_blocking(window_data) == false) {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: event dispatch failed in mfb_wait_sync.");
        return false;
    }

    if (window_data->close == true) {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: mfb_wait_sync aborted after dispatch because the window is marked for close.");
        destroy(window_data);
        return false;
    }

    if (window_data->must_resize_context) {
        window_data->must_resize_context = false;
        kCall(resize_func, window_data->window_width, window_data->window_height);
    }

    // When hardware sync is active (frame callback provides timing) or
    // no target FPS is set, skip software pacing entirely.
    if (g_use_hardware_sync == true || g_time_for_frame == 0.0) {
        mfb_timer_compensated_reset(window_data_specific->timer);
        return true;
    }

    // Software pacing: sleep for the remaining time, dispatching events meanwhile.
    while (true) {
        double elapsed_time = mfb_timer_now(window_data_specific->timer);
        if (elapsed_time >= g_time_for_frame) {
            break;
        }

        double remaining_ms = (g_time_for_frame - elapsed_time) * 1000.0;

        if (remaining_ms > 1.0) {
            struct timespec timeout = ms_to_ts(remaining_ms);
            if (dispatch_owned_timeout(window_data, &timeout) == false) {
                MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: event dispatch failed during sync loop.");
                return false;
            }
        }
        else {
            sched_yield();
            if (dispatch_owned_non_blocking(window_data) == false) {
                MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: event dispatch failed during sync loop (non-blocking).");
                return false;
            }
        }

        if (window_data->close == true) {
            MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: mfb_wait_sync aborted during sync loop because the window is marked for close.");
            destroy(window_data);
            return false;
        }

        if (window_data->must_resize_context) {
            window_data->must_resize_context = false;
            kCall(resize_func, window_data->window_width, window_data->window_height);
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
mfb_set_title(struct mfb_window *window, const char *title) {
    SWindowData *window_data;
    SWindowData_Way *window_data_specific;

    if (title == NULL) {
        return;
    }

    if (get_window_data(window, __func__, &window_data, &window_data_specific) == false) {
        return;
    }

    kUnused(window_data);
    xdg_toplevel_set_title(window_data_specific->toplevel, title);
    wl_surface_commit(window_data_specific->surface);
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
    SWindowData *window_data;
    SWindowData_Way *window_data_specific;

    if (get_window_data(window, __func__, &window_data, &window_data_specific) == false) {
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

    kUnused(cursor_surface);
    refresh_cursor_surface(window_data);
}
