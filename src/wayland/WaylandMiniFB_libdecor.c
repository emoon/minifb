#ifdef MINIFB_HAS_LIBDECOR

#include "WaylandMiniFB_libdecor.h"
#include "WaylandMiniFB.h"
#include "MiniFB_internal.h"
#include "MiniFB_enums.h"

#include <libdecor.h>
#include <wayland-client.h>

#include <dlfcn.h>
#include <stdbool.h>
#include <stddef.h>

//-------------------------------------
// The versioned soname is deliberate: plain libdecor-0.so is a symlink that
// only the development package installs.
//-------------------------------------
#define WAYLAND_LIBDECOR_SONAME "libdecor-0.so.0"

//-------------------------------------
// libdecor is resolved at run time rather than linked, so a binary built on a
// machine that has it still runs where it is missing. Every libdecor entry
// point the backend uses goes through this table.
//-------------------------------------
typedef struct SWaylandLibdecor {
    struct libdecor *       (*libdecor_new)(struct wl_display *display, struct libdecor_interface *iface);
    void                    (*libdecor_unref)(struct libdecor *context);
    int                     (*libdecor_dispatch)(struct libdecor *context, int timeout);

    struct libdecor_frame * (*libdecor_decorate)(struct libdecor *context, struct wl_surface *surface,
                                                 struct libdecor_frame_interface *iface, void *user_data);
    void                    (*libdecor_frame_unref)(struct libdecor_frame *frame);
    void                    (*libdecor_frame_map)(struct libdecor_frame *frame);
    void                    (*libdecor_frame_commit)(struct libdecor_frame *frame, struct libdecor_state *state,
                                                     struct libdecor_configuration *configuration);

    void                    (*libdecor_frame_set_title)(struct libdecor_frame *frame, const char *title);
    void                    (*libdecor_frame_set_app_id)(struct libdecor_frame *frame, const char *app_id);
    void                    (*libdecor_frame_set_min_content_size)(struct libdecor_frame *frame,
                                                                   int content_width, int content_height);
    void                    (*libdecor_frame_set_max_content_size)(struct libdecor_frame *frame,
                                                                   int content_width, int content_height);
    void                    (*libdecor_frame_set_capabilities)(struct libdecor_frame *frame,
                                                               enum libdecor_capabilities capabilities);
    void                    (*libdecor_frame_unset_capabilities)(struct libdecor_frame *frame,
                                                                 enum libdecor_capabilities capabilities);
    void                    (*libdecor_frame_set_maximized)(struct libdecor_frame *frame);
    void                    (*libdecor_frame_set_fullscreen)(struct libdecor_frame *frame, struct wl_output *output);

    struct libdecor_state * (*libdecor_state_new)(int width, int height);
    void                    (*libdecor_state_free)(struct libdecor_state *state);

    bool                    (*libdecor_configuration_get_content_size)(struct libdecor_configuration *configuration,
                                                                       struct libdecor_frame *frame,
                                                                       int *width, int *height);
    bool                    (*libdecor_configuration_get_window_state)(struct libdecor_configuration *configuration,
                                                                       enum libdecor_window_state *window_state);
} SWaylandLibdecor;

//-------------------------------------
static void *           g_libdecor_library = NULL;
static SWaylandLibdecor g_libdecor_api;
// Zero until the first attempt, then 1 for loaded and -1 for unavailable.
static int              g_libdecor_state = 0;
// Set once the symbols are in place, and shared by every window since dlopen is
// process wide.
static const SWaylandLibdecor *g_libdecor = NULL;

//-------------------------------------
// libdecor_interface.error carries no user pointer, so an error cannot be
// reported per window. Only window creation reads this flag, and it clears the
// flag right before it creates the context.
//-------------------------------------
static bool g_libdecor_failed = false;

//-------------------------------------
static bool
load_symbol(void **target, const char *name) {
    *target = dlsym(g_libdecor_library, name);
    if (*target == NULL) {
        MFB_LOG(MFB_LOG_WARNING,
                "WaylandMiniFB: %s has no symbol %s; libdecor will not be used.",
                WAYLAND_LIBDECOR_SONAME, name);
        return false;
    }
    return true;
}

//-------------------------------------
// Casting through void ** keeps the symbol name and the member name in one
// place, so a typo cannot bind a symbol to the wrong slot.
//-------------------------------------
#define LOAD_SYMBOL(name) load_symbol((void **) &g_libdecor_api.name, #name)

//-------------------------------------
// Loads libdecor on the first call and caches the outcome, failure included, so
// later windows do not retry. Returns NULL when the library or any symbol the
// backend needs is unavailable.
//-------------------------------------
static const SWaylandLibdecor *
libdecor_load(void) {
    if (g_libdecor_state != 0) {
        return (g_libdecor_state > 0) ? &g_libdecor_api : NULL;
    }

    g_libdecor_state = -1;

    g_libdecor_library = dlopen(WAYLAND_LIBDECOR_SONAME, RTLD_LAZY | RTLD_LOCAL);
    if (g_libdecor_library == NULL) {
        const char *reason = dlerror();
        MFB_LOG(MFB_LOG_INFO, "WaylandMiniFB: %s is not available: %s",
                WAYLAND_LIBDECOR_SONAME, (reason != NULL) ? reason : "no reason reported");
        return NULL;
    }

    bool complete = true;

    complete &= LOAD_SYMBOL(libdecor_new);
    complete &= LOAD_SYMBOL(libdecor_unref);
    complete &= LOAD_SYMBOL(libdecor_dispatch);

    complete &= LOAD_SYMBOL(libdecor_decorate);
    complete &= LOAD_SYMBOL(libdecor_frame_unref);
    complete &= LOAD_SYMBOL(libdecor_frame_map);
    complete &= LOAD_SYMBOL(libdecor_frame_commit);

    complete &= LOAD_SYMBOL(libdecor_frame_set_title);
    complete &= LOAD_SYMBOL(libdecor_frame_set_app_id);
    complete &= LOAD_SYMBOL(libdecor_frame_set_min_content_size);
    complete &= LOAD_SYMBOL(libdecor_frame_set_max_content_size);
    complete &= LOAD_SYMBOL(libdecor_frame_set_capabilities);
    complete &= LOAD_SYMBOL(libdecor_frame_unset_capabilities);
    complete &= LOAD_SYMBOL(libdecor_frame_set_maximized);
    complete &= LOAD_SYMBOL(libdecor_frame_set_fullscreen);

    complete &= LOAD_SYMBOL(libdecor_state_new);
    complete &= LOAD_SYMBOL(libdecor_state_free);

    complete &= LOAD_SYMBOL(libdecor_configuration_get_content_size);
    complete &= LOAD_SYMBOL(libdecor_configuration_get_window_state);

    if (complete == false) {
        // Safe to unload here because no symbol has been called yet. Once the
        // backend starts using them the library stays for the whole process:
        // libdecor loads plugins of its own, and unloading it while a window
        // is open would leave their callbacks pointing at freed code.
        dlclose(g_libdecor_library);
        g_libdecor_library = NULL;
        return NULL;
    }

    g_libdecor_state = 1;

    return &g_libdecor_api;
}

//-------------------------------------
static void
libdecor_handle_error(struct libdecor *context, enum libdecor_error error, const char *message) {
    kUnused(context);

    g_libdecor_failed = true;

    MFB_LOG(MFB_LOG_WARNING, "WaylandMiniFB: libdecor error %d: %s", (int) error, message);
}

//-------------------------------------
// Not const: libdecor_new() takes a mutable pointer.
//-------------------------------------
static struct libdecor_interface libdecor_iface = {
    .error = libdecor_handle_error
};

//-------------------------------------
static void
libdecor_handle_configure(struct libdecor_frame *frame,
                          struct libdecor_configuration *configuration,
                          void *user_data) {
    SWindowData *window_data = (SWindowData *) user_data;
    if (window_data == NULL || window_data->specific == NULL) {
        return;
    }

    SWindowData_Way *window_data_specific = (SWindowData_Way *) window_data->specific;
    enum libdecor_window_state window_state = LIBDECOR_WINDOW_STATE_NONE;
    int width = 0;
    int height = 0;

    g_libdecor->libdecor_configuration_get_window_state(configuration, &window_state);

    // The size libdecor reports is the content size, with its own decorations
    // already excluded. No size means the compositor leaves the choice to us.
    if (g_libdecor->libdecor_configuration_get_content_size(configuration, frame, &width, &height) == false
        || width <= 0 || height <= 0) {
        width  = (int) window_data->window_width;
        height = (int) window_data->window_height;
    }

    MFB_LOG(MFB_LOG_DEBUG, "libdecor configure: width=%d, height=%d, state=0x%x",
            width, height, (unsigned) window_state);

    if (window_data->window_width != (uint32_t) width || window_data->window_height != (uint32_t) height) {
        window_data->window_width  = (uint32_t) width;
        window_data->window_height = (uint32_t) height;
        resize_dst(window_data, (uint32_t) width, (uint32_t) height);
        wayland_update_opaque_region(window_data, window_data_specific);
        window_data->must_resize_context = true;
    }

    // This acknowledges the configure, so it must happen even when nothing changed.
    struct libdecor_state *state = g_libdecor->libdecor_state_new(width, height);
    if (state != NULL) {
        g_libdecor->libdecor_frame_commit(frame, state, configuration);
        g_libdecor->libdecor_state_free(state);
    }

    // Same reason as the xdg path: some compositors apply startup states more
    // reliably when they are requested after the first configure handshake.
    if (window_data_specific->startup_state_applied == 0) {
        if (window_data_specific->request_fullscreen != 0) {
            g_libdecor->libdecor_frame_set_fullscreen(frame, NULL);
        }
        else if (window_data_specific->request_maximized != 0) {
            g_libdecor->libdecor_frame_set_maximized(frame);
        }
        window_data_specific->startup_state_applied = 1;
    }

    if (window_data->is_initialized == false) {
        wayland_attach_initial_buffer(window_data, window_data_specific);
    }
    else {
        // The decorations are synchronized subsurfaces, so the state committed
        // above only reaches the screen once the parent surface is committed.
        // Without this they stay one frame behind the window content.
        wl_surface_commit(window_data_specific->surface);
    }
}

//-------------------------------------
static void
libdecor_handle_close(struct libdecor_frame *frame, void *user_data) {
    kUnused(frame);

    SWindowData *window_data = (SWindowData *) user_data;
    if (window_data == NULL) {
        return;
    }

    // Keep parity with X11: ask close callback before closing.
    if (window_data->close_func == NULL || window_data->close_func((struct mfb_window *) window_data)) {
        window_data->close = true;
    }

    MFB_LOG(MFB_LOG_DEBUG, "libdecor close");
}

//-------------------------------------
// Decorations drawn as subsurfaces are synchronized, so they only reach the
// screen when the parent surface is committed.
//-------------------------------------
static void
libdecor_handle_commit(struct libdecor_frame *frame, void *user_data) {
    kUnused(frame);

    SWindowData *window_data = (SWindowData *) user_data;
    if (window_data == NULL || window_data->specific == NULL) {
        return;
    }

    SWindowData_Way *window_data_specific = (SWindowData_Way *) window_data->specific;
    if (window_data_specific->surface != NULL) {
        wl_surface_commit(window_data_specific->surface);
    }
}

//-------------------------------------
static void
libdecor_handle_dismiss_popup(struct libdecor_frame *frame, const char *seat_name, void *user_data) {
    kUnused(frame);
    kUnused(seat_name);
    kUnused(user_data);
}

//-------------------------------------
static struct libdecor_frame_interface libdecor_frame_iface = {
    .configure     = libdecor_handle_configure,
    .close         = libdecor_handle_close,
    .commit        = libdecor_handle_commit,
    .dismiss_popup = libdecor_handle_dismiss_popup
};

//-------------------------------------
void
wayland_libdecor_release(SWindowData_Way *window_data_specific) {
    if (g_libdecor == NULL) {
        return;
    }

    if (window_data_specific->libdecor_frame != NULL) {
        g_libdecor->libdecor_frame_unref(window_data_specific->libdecor_frame);
        window_data_specific->libdecor_frame = NULL;
    }

    if (window_data_specific->libdecor_context != NULL) {
        g_libdecor->libdecor_unref(window_data_specific->libdecor_context);
        window_data_specific->libdecor_context = NULL;
    }
}

//-------------------------------------
int
wayland_libdecor_dispatch_pending(SWindowData_Way *window_data_specific) {
    if (g_libdecor == NULL || window_data_specific->libdecor_context == NULL) {
        return 0;
    }

    // A timeout of zero never blocks. This has to go through libdecor instead
    // of dispatching the default queue directly: a plugin may have event
    // sources of its own, and the GTK one drives a GLib main context here.
    return g_libdecor->libdecor_dispatch(window_data_specific->libdecor_context, 0);
}

//-------------------------------------
bool
wayland_libdecor_set_title(SWindowData_Way *window_data_specific, const char *title) {
    if (g_libdecor == NULL || window_data_specific->libdecor_frame == NULL) {
        return false;
    }

    g_libdecor->libdecor_frame_set_title(window_data_specific->libdecor_frame, title);

    return true;
}

//-------------------------------------
bool
wayland_libdecor_create_toplevel(SWindowData *window_data, SWindowData_Way *window_data_specific,
                                 unsigned effective_flags, const char *window_title,
                                 const char *app_id, unsigned width, unsigned height) {
    g_libdecor = libdecor_load();
    if (g_libdecor == NULL) {
        return false;
    }

    g_libdecor_failed = false;

    window_data_specific->libdecor_context =
        g_libdecor->libdecor_new(window_data_specific->display, &libdecor_iface);
    if (window_data_specific->libdecor_context == NULL) {
        MFB_LOG(MFB_LOG_WARNING, "WaylandMiniFB: libdecor_new failed.");
        return false;
    }

    // libdecor reports an incompatible compositor asynchronously, through a
    // wl_display sync it issues on the default queue.
    if (wl_display_roundtrip(window_data_specific->display) == -1) {
        MFB_LOG(MFB_LOG_WARNING, "WaylandMiniFB: libdecor initialization did not complete.");
        wayland_libdecor_release(window_data_specific);
        return false;
    }

    if (g_libdecor_failed) {
        wayland_libdecor_release(window_data_specific);
        return false;
    }

    window_data_specific->libdecor_frame =
        g_libdecor->libdecor_decorate(window_data_specific->libdecor_context,
                                      window_data_specific->surface,
                                      &libdecor_frame_iface, window_data);
    if (window_data_specific->libdecor_frame == NULL) {
        MFB_LOG(MFB_LOG_WARNING, "WaylandMiniFB: libdecor_decorate failed.");
        wayland_libdecor_release(window_data_specific);
        return false;
    }

    window_data_specific->request_fullscreen = (effective_flags & MFB_WF_FULLSCREEN) ? 1 : 0;
    window_data_specific->request_maximized =
        (!window_data_specific->request_fullscreen && (effective_flags & MFB_WF_FULLSCREEN_DESKTOP)) ? 1 : 0;
    window_data_specific->startup_state_applied = 0;

    g_libdecor->libdecor_frame_set_app_id(window_data_specific->libdecor_frame, app_id);
    g_libdecor->libdecor_frame_set_title(window_data_specific->libdecor_frame, window_title);

    // A libdecor frame is unconstrained and fully capable by default, so only
    // the fixed-size case needs saying. These bounds are content sizes, so the
    // buffer keeps the size the caller asked for whatever the frame adds.
    if (window_data_specific->request_fullscreen == 0 &&
        window_data_specific->request_maximized == 0 &&
        (effective_flags & MFB_WF_RESIZABLE) == 0) {
        g_libdecor->libdecor_frame_set_min_content_size(window_data_specific->libdecor_frame,
                                                        (int) width, (int) height);
        g_libdecor->libdecor_frame_set_max_content_size(window_data_specific->libdecor_frame,
                                                        (int) width, (int) height);
        g_libdecor->libdecor_frame_unset_capabilities(window_data_specific->libdecor_frame,
                                                      LIBDECOR_ACTION_RESIZE);
    }

    g_libdecor->libdecor_frame_map(window_data_specific->libdecor_frame);

    // libdecor keeps its objects on the default queue, so the handshake has to
    // be driven from there instead of from window_queue.
    while (window_data->is_initialized == false && window_data->close == false) {
        if (wl_display_dispatch(window_data_specific->display) == -1) {
            MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: wl_display_dispatch failed while waiting for the libdecor configure event.");
            return false;
        }
    }

    if (window_data->close == true) {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: initialization failed during the libdecor configure handshake.");
        return false;
    }

    if (wl_display_roundtrip(window_data_specific->display) == -1) {
        MFB_LOG(MFB_LOG_ERROR, "WaylandMiniFB: failed to complete the initial surface mapping.");
        return false;
    }

    return true;
}

#endif
