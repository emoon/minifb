// Single-display fallback implementation used by backends that never expose
// more than one monitor: Web (Emscripten), iOS, Android, and DOS.
// Multi-display backends (Windows, macOS, X11, Wayland) provide their own
// implementation in their respective *_monitor.* files.

#include <MiniFB.h>
#include <string.h>
#include <stdio.h>

//-------------------------------------
int
mfb_get_num_monitors(void) {
    return 1;
}

//-------------------------------------
bool
mfb_get_monitor_info(unsigned index, mfb_monitor_info *out_info) {
    if (out_info == NULL || index != 0)
        return false;

    memset(out_info, 0, sizeof(mfb_monitor_info));
    out_info->scale_x    = 1.0f;
    out_info->scale_y    = 1.0f;
    out_info->is_primary = true;
    // logical/physical width and height are 0 — they are not knowable before
    // a window is created on single-display backends.
    return true;
}

//-------------------------------------
static mfb_monitor_info g_window_monitor_cache;

mfb_monitor_info *
mfb_get_window_monitor(struct mfb_window *window) {
    if (window == NULL)
        return NULL;

    if (!mfb_get_monitor_info(0, &g_window_monitor_cache))
        return NULL;

    // Fill in scale from the window now that one exists.
    mfb_get_monitor_scale(window, &g_window_monitor_cache.scale_x, &g_window_monitor_cache.scale_y);
    return &g_window_monitor_cache;
}

//-------------------------------------
struct mfb_window *
mfb_open_on_monitor(const char *title, unsigned width, unsigned height,
                    unsigned monitor_index) {
    (void) monitor_index;
    return mfb_open(title, width, height);
}

//-------------------------------------
struct mfb_window *
mfb_open_on_monitor_ex(const char *title, unsigned width, unsigned height,
                        unsigned flags, unsigned monitor_index) {
    (void) monitor_index;

    if ((flags & MFB_WF_SIZE_LOGICAL) && (flags & MFB_WF_SIZE_PHYSICAL)) {
        fprintf(stderr, "mfb_open_on_monitor_ex: MFB_WF_SIZE_LOGICAL and MFB_WF_SIZE_PHYSICAL are mutually exclusive\n");
        return NULL;
    }

    // On single-display backends there is no DPI scale to apply before window
    // creation, so both SIZE_LOGICAL and SIZE_PHYSICAL are no-ops here.
    unsigned open_flags = flags & ~(unsigned)(MFB_WF_SIZE_LOGICAL | MFB_WF_SIZE_PHYSICAL);
    return mfb_open_ex(title, width, height, open_flags);
}
