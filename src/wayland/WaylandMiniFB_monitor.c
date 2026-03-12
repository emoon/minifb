#include <MiniFB.h>
#include <MiniFB_internal.h>
#include <WindowData.h>
#include "WindowData_Way.h"

#include <wayland-client.h>
#include "generated/xdg-output-unstable-v1-client-protocol.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// ---------------------------------------------------------------------------
// Pre-window monitor query via a temporary Wayland connection.
//
// Scale detection strategy:
//   1. zxdg_output_manager_v1 (xdg-output-unstable-v1) — gives the logical
//      size per output as reported by the compositor. Scale is derived as
//      physical_pixels / logical_size, which correctly handles fractional
//      scaling (e.g. 1.25x) that wl_output_scale (integer-only) cannot express.
//   2. wl_output_scale (integer, v2) — fallback if zxdg_output_manager_v1 is
//      not available.
//   3. 1.0 — if neither is available (e.g. WAYLAND_DISPLAY not set).
//
// Limitations:
//   - Wayland compositor controls window placement; monitor_index in
//     mfb_open_on_monitor[_ex] is used only to calculate the requested size.
//   - On WSL, WAYLAND_DISPLAY may not be set; falls back to 1 monitor + defaults.
// ---------------------------------------------------------------------------

#define kMAX_WAY_MONITORS   16
#define kWL_OUTPUT_VERSION   2   // gives: geometry, mode, done, scale
#define kXDG_OUTPUT_VERSION  3   // gives: logical_position, logical_size, done, name, description

typedef struct {
    int      logical_x, logical_y;
    int      xdg_logical_w, xdg_logical_h;  // from zxdg_output_v1 (fractional-accurate)
    unsigned physical_width, physical_height;
    float    int_scale;   // from wl_output_scale (integer fallback)
    bool     done;
    bool     xdg_done;
    char     name[128];
} WayOutputData;

typedef struct {
    WayOutputData                 outputs[kMAX_WAY_MONITORS];
    struct wl_output             *handles[kMAX_WAY_MONITORS];
    struct zxdg_output_v1        *xdg_outputs[kMAX_WAY_MONITORS];
    struct zxdg_output_manager_v1 *xdg_manager;
    int                           count;
} WayMonitorList;

// wl_output callbacks -------------------------------------------------------

static void
way_output_geometry(void *data, struct wl_output *output,
                    int32_t x, int32_t y,
                    int32_t phys_mm_w, int32_t phys_mm_h,
                    int32_t subpixel, const char *make, const char *model,
                    int32_t transform) {
    (void) output; (void) phys_mm_w; (void) phys_mm_h;
    (void) subpixel; (void) transform;
    WayOutputData *d = (WayOutputData *) data;
    d->logical_x = (int) x;
    d->logical_y = (int) y;
    // Fallback name from make+model (overridden by xdg_output name if available)
    if (!d->name[0])
        snprintf(d->name, sizeof(d->name), "%s %s",
                 make ? make : "", model ? model : "");
}

static void
way_output_mode(void *data, struct wl_output *output,
                uint32_t flags, int32_t width, int32_t height, int32_t refresh) {
    (void) output; (void) refresh;
    if (flags & WL_OUTPUT_MODE_CURRENT) {
        WayOutputData *d = (WayOutputData *) data;
        d->physical_width  = (unsigned) width;
        d->physical_height = (unsigned) height;
    }
}

static void
way_output_done(void *data, struct wl_output *output) {
    (void) output;
    ((WayOutputData *) data)->done = true;
}

static void
way_output_scale(void *data, struct wl_output *output, int32_t factor) {
    (void) output;
    ((WayOutputData *) data)->int_scale = (float) factor;
}

static const struct wl_output_listener kOutputListener = {
    way_output_geometry,
    way_output_mode,
    way_output_done,
    way_output_scale,
};

// zxdg_output_v1 callbacks --------------------------------------------------

static void
xdg_output_logical_position(void *data, struct zxdg_output_v1 *xdg,
                             int32_t x, int32_t y) {
    (void) xdg;
    WayOutputData *d = (WayOutputData *) data;
    d->logical_x = (int) x;
    d->logical_y = (int) y;
}

static void
xdg_output_logical_size(void *data, struct zxdg_output_v1 *xdg,
                         int32_t width, int32_t height) {
    (void) xdg;
    WayOutputData *d = (WayOutputData *) data;
    d->xdg_logical_w = (int) width;
    d->xdg_logical_h = (int) height;
}

static void
xdg_output_done(void *data, struct zxdg_output_v1 *xdg) {
    (void) xdg;
    ((WayOutputData *) data)->xdg_done = true;
}

static void
xdg_output_name(void *data, struct zxdg_output_v1 *xdg, const char *name) {
    (void) xdg;
    WayOutputData *d = (WayOutputData *) data;
    if (name && name[0]) {
        strncpy(d->name, name, sizeof(d->name) - 1);
        d->name[sizeof(d->name) - 1] = '\0';
    }
}

static void
xdg_output_description(void *data, struct zxdg_output_v1 *xdg,
                        const char *description) {
    (void) data; (void) xdg; (void) description;
}

static const struct zxdg_output_v1_listener kXdgOutputListener = {
    xdg_output_logical_position,
    xdg_output_logical_size,
    xdg_output_done,
    xdg_output_name,
    xdg_output_description,
};

// wl_registry callbacks -----------------------------------------------------

static void
way_registry_global(void *data, struct wl_registry *registry,
                    uint32_t name, const char *interface, uint32_t version) {
    WayMonitorList *list = (WayMonitorList *) data;

    if (strcmp(interface, wl_output_interface.name) == 0) {
        if (list->count >= kMAX_WAY_MONITORS)
            return;
        uint32_t ver = (version < kWL_OUTPUT_VERSION) ? version : kWL_OUTPUT_VERSION;
        struct wl_output *output = wl_registry_bind(registry, name,
                                                     &wl_output_interface, ver);
        int idx = list->count++;
        memset(&list->outputs[idx], 0, sizeof(list->outputs[idx]));
        list->outputs[idx].int_scale = 1.0f;
        list->handles[idx] = output;
        list->xdg_outputs[idx] = NULL;
        wl_output_add_listener(output, &kOutputListener, &list->outputs[idx]);
    }
    else if (strcmp(interface, zxdg_output_manager_v1_interface.name) == 0) {
        uint32_t ver = (version < kXDG_OUTPUT_VERSION) ? version : kXDG_OUTPUT_VERSION;
        list->xdg_manager = wl_registry_bind(registry, name,
                                              &zxdg_output_manager_v1_interface, ver);
    }
}

static void
way_registry_global_remove(void *data, struct wl_registry *registry, uint32_t name) {
    (void) data; (void) registry; (void) name;
}

static const struct wl_registry_listener kRegistryListener = {
    way_registry_global,
    way_registry_global_remove,
};

// ---------------------------------------------------------------------------
// Enumerate monitors via a temporary connection.
// Returns the number of monitors found, or 0 on failure.
// ---------------------------------------------------------------------------
static int
enumerate_monitors(WayMonitorList *list) {
    list->count      = 0;
    list->xdg_manager = NULL;

    struct wl_display *display = wl_display_connect(NULL);
    if (!display)
        return 0;

    struct wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &kRegistryListener, list);

    // 1st roundtrip: receive the global list (wl_output + zxdg_output_manager_v1)
    wl_display_roundtrip(display);

    // Bind zxdg_output_v1 for each output, now that we know if the manager exists
    if (list->xdg_manager) {
        for (int i = 0; i < list->count; i++) {
            list->xdg_outputs[i] = zxdg_output_manager_v1_get_xdg_output(
                list->xdg_manager, list->handles[i]);
            zxdg_output_v1_add_listener(list->xdg_outputs[i],
                                         &kXdgOutputListener,
                                         &list->outputs[i]);
        }
    }

    // 2nd roundtrip: receive output events (geometry, mode, scale, done) and
    //                xdg_output events (logical_size, name, done)
    wl_display_roundtrip(display);

    // Cleanup
    for (int i = 0; i < list->count; i++) {
        if (list->xdg_outputs[i])
            zxdg_output_v1_destroy(list->xdg_outputs[i]);
        wl_output_destroy(list->handles[i]);
    }
    if (list->xdg_manager)
        zxdg_output_manager_v1_destroy(list->xdg_manager);
    wl_registry_destroy(registry);
    wl_display_disconnect(display);

    return list->count;
}

// ---------------------------------------------------------------------------
// Compute effective scale for an output.
//
// Priority:
//   1. zxdg_output_v1 logical_size  — physical / logical → real fractional scale
//   2. wl_output_scale (integer)    — fallback
//   3. 1.0
// ---------------------------------------------------------------------------
static float
effective_scale(const WayOutputData *d) {
    if (d->xdg_logical_w > 0 && d->physical_width > 0) {
        float sx = (float) d->physical_width  / (float) d->xdg_logical_w;
        float sy = (float) d->physical_height / (float) d->xdg_logical_h;
        // Sanity: both axes should agree within 1%; use average
        float s = (sx + sy) * 0.5f;
        if (s >= 0.5f && s <= 8.0f)
            return s;
    }
    if (d->int_scale > 0.0f)
        return d->int_scale;
    return 1.0f;
}

// ---------------------------------------------------------------------------
// Convert raw WayOutputData → mfb_monitor_info
// ---------------------------------------------------------------------------
static void
fill_monitor_info(const WayOutputData *src, mfb_monitor_info *dst, bool is_primary) {
    memset(dst, 0, sizeof(*dst));

    float scale = effective_scale(src);
    dst->scale_x = scale;
    dst->scale_y = scale;

    dst->physical_width  = src->physical_width;
    dst->physical_height = src->physical_height;

    // Prefer xdg logical size (fractional-accurate); fall back to physical/scale
    if (src->xdg_logical_w > 0) {
        dst->logical_width  = (unsigned) src->xdg_logical_w;
        dst->logical_height = (unsigned) src->xdg_logical_h;
    } else if (scale > 0.0f) {
        dst->logical_width  = (unsigned) (src->physical_width  / scale);
        dst->logical_height = (unsigned) (src->physical_height / scale);
    }

    dst->logical_x  = src->logical_x;
    dst->logical_y  = src->logical_y;
    dst->is_primary = is_primary;

    strncpy(dst->name, src->name, sizeof(dst->name) - 1);
    dst->name[sizeof(dst->name) - 1] = '\0';
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

int
mfb_get_num_monitors(void) {
    WayMonitorList list;
    int n = enumerate_monitors(&list);
    return n > 0 ? n : 1;
}

//-------------------------------------
bool
mfb_get_monitor_info(unsigned index, mfb_monitor_info *out_info) {
    if (out_info == NULL)
        return false;

    WayMonitorList list;
    int n = enumerate_monitors(&list);

    if (n == 0) {
        if (index != 0)
            return false;
        memset(out_info, 0, sizeof(*out_info));
        out_info->scale_x    = 1.0f;
        out_info->scale_y    = 1.0f;
        out_info->is_primary = true;
        return true;
    }

    if ((int) index >= n)
        return false;

    fill_monitor_info(&list.outputs[index], out_info, index == 0);
    return true;
}

//-------------------------------------
static mfb_monitor_info g_window_monitor_cache;

#define WAYLAND_FRACTIONAL_SCALE_DENOMINATOR 120

mfb_monitor_info *
mfb_get_window_monitor(struct mfb_window *window) {
    if (window == NULL)
        return NULL;

    SWindowData     *window_data = (SWindowData *) window;
    SWindowData_Way *way         = (SWindowData_Way *) window_data->specific;
    if (way == NULL)
        return NULL;

    mfb_monitor_info *info = &g_window_monitor_cache;
    memset(info, 0, sizeof(*info));

    // Prefer fractional scale (120ths), fall back to integer output scale
    float scale = 1.0f;
    if (way->preferred_scale_120 > 0)
        scale = (float) way->preferred_scale_120 / WAYLAND_FRACTIONAL_SCALE_DENOMINATOR;
    else if (way->current_output_scale > 0)
        scale = (float) way->current_output_scale;

    if (scale <= 0.0f) scale = 1.0f;
    info->scale_x    = scale;
    info->scale_y    = scale;
    info->is_primary = true;

    info->physical_width  = window_data->window_width;
    info->physical_height = window_data->window_height;
    info->logical_width   = (unsigned) (window_data->window_width  / scale);
    info->logical_height  = (unsigned) (window_data->window_height / scale);

    return info;
}

//-------------------------------------
struct mfb_window *
mfb_open_on_monitor(const char *title, unsigned width, unsigned height,
                    unsigned monitor_index) {
    return mfb_open_on_monitor_ex(title, width, height, 0, monitor_index);
}

//-------------------------------------
struct mfb_window *
mfb_open_on_monitor_ex(const char *title, unsigned width, unsigned height,
                        unsigned flags, unsigned monitor_index) {
    if ((flags & MFB_WF_SIZE_LOGICAL) && (flags & MFB_WF_SIZE_PHYSICAL)) {
        fprintf(stderr, "mfb_open_on_monitor_ex: MFB_WF_SIZE_LOGICAL and MFB_WF_SIZE_PHYSICAL are mutually exclusive\n");
        return NULL;
    }

    mfb_monitor_info target;
    if (!mfb_get_monitor_info(monitor_index, &target))
        mfb_get_monitor_info(0, &target);

    unsigned open_w = width, open_h = height;
    if ((flags & MFB_WF_SIZE_PHYSICAL) && target.scale_x > 0.0f) {
        open_w = (unsigned) (width  / target.scale_x);
        open_h = (unsigned) (height / target.scale_y);
    }

    unsigned open_flags = flags & ~(unsigned)(MFB_WF_SIZE_LOGICAL | MFB_WF_SIZE_PHYSICAL);
    return mfb_open_ex(title, open_w, open_h, open_flags);
}
