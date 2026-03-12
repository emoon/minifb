#include <MiniFB.h>
#include <MiniFB_internal.h>
#include <WindowData.h>
#include "WindowData_X11.h"

#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#if defined(HAVE_XRANDR)
    #include <X11/extensions/Xrandr.h>
#endif

// ---------------------------------------------------------------------------
// DPI / scale detection
//
// Priority order:
//   1. XRandR EDID physical size per monitor  — accurate on real hardware,
//      most reliable for multi-monitor setups with different DPIs.
//   2. Xft.dpi X resource                     — global setting, commonly set
//      by GNOME/KDE; reliable on single-monitor or homogeneous setups.
//   3. 1.0 fallback                            — VMs, WSL, missing EDID.
//
// A DPI sanity range of [48, 600] is enforced to reject bogus EDID values
// that appear in virtual machines and some unusual hardware.
// ---------------------------------------------------------------------------

#define kDPI_MIN  48.0f
#define kDPI_MAX  600.0f
#define kDPI_BASE 96.0f

#if defined(HAVE_XRANDR)
// Returns the DPI for a given XRRMonitorInfo via EDID physical size.
// Returns 0 if the data is unavailable or out of the sanity range.
static float
dpi_from_xrandr_output(Display *display, XRRMonitorInfo *monitor) {
    XRRScreenResources *res = XRRGetScreenResources(display, DefaultRootWindow(display));
    if (!res)
        return 0.0f;

    float dpi = 0.0f;
    for (int i = 0; i < monitor->noutput && dpi == 0.0f; i++) {
        XRROutputInfo *out = XRRGetOutputInfo(display, res, monitor->outputs[i]);
        if (!out)
            continue;

        // XWayland virtual outputs report EDID physical sizes from the real
        // panel, which do not reflect the Windows DPI scaling factor.
        // Skip them so we fall through to Xft.dpi or 1.0.
        if (out->name && strncmp(out->name, "XWAYLAND", 8) == 0) {
            XRRFreeOutputInfo(out);
            continue;
        }

        if (out->connection == RR_Connected &&
            out->mm_width  > 10 &&
            out->mm_height > 10 &&
            out->crtc      != None) {
            XRRCrtcInfo *crtc = XRRGetCrtcInfo(display, res, out->crtc);
            if (crtc && crtc->width > 0 && crtc->height > 0) {
                float dpi_x = crtc->width  * 25.4f / (float) out->mm_width;
                float dpi_y = crtc->height * 25.4f / (float) out->mm_height;
                float candidate = (dpi_x + dpi_y) * 0.5f;
                if (candidate >= kDPI_MIN && candidate <= kDPI_MAX)
                    dpi = candidate;
            }
            if (crtc)
                XRRFreeCrtcInfo(crtc);
        }
        XRRFreeOutputInfo(out);
    }

    XRRFreeScreenResources(res);
    return dpi;
}
#endif // HAVE_XRANDR

// Returns the DPI from the Xft.dpi X resource (set by GNOME, KDE, etc.).
// Returns 0 if the resource is not set or out of the sanity range.
static float
dpi_from_xft(Display *display) {
    if (!display)
        return 0.0f;
    char *resource_manager = XResourceManagerString(display);
    if (!resource_manager)
        return 0.0f;

    XrmDatabase db = XrmGetStringDatabase(resource_manager);
    if (!db)
        return 0.0f;

    float dpi = 0.0f;
    char    *type  = NULL;
    XrmValue value = { 0, NULL };
    if (XrmGetResource(db, "Xft.dpi", "Xft.Dpi", &type, &value) && value.addr) {
        float candidate = (float) atof(value.addr);
        if (candidate >= kDPI_MIN && candidate <= kDPI_MAX)
            dpi = candidate;
    }

    XrmDestroyDatabase(db);
    return dpi;
}

// Fills scale_x/scale_y for a monitor. Uses XRandR EDID first, then Xft.dpi,
// then falls back to 1.0.
static void
fill_scale(Display *display,
#if defined(HAVE_XRANDR)
           XRRMonitorInfo *monitor,
#endif
           float *scale_x, float *scale_y) {
    float dpi = 0.0f;

#if defined(HAVE_XRANDR)
    if (monitor)
        dpi = dpi_from_xrandr_output(display, monitor);
#endif

    if (dpi == 0.0f)
        dpi = dpi_from_xft(display);

    float scale = (dpi > 0.0f) ? (dpi / kDPI_BASE) : 1.0f;
    if (scale_x) *scale_x = scale;
    if (scale_y) *scale_y = scale;
}

// ---------------------------------------------------------------------------
// Helper: open a temporary Display for standalone queries (before any window
// is created). Caller must XCloseDisplay the result.
// ---------------------------------------------------------------------------
static Display *
open_display(void) {
    return XOpenDisplay(NULL);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

int
mfb_get_num_monitors(void) {
#if defined(HAVE_XRANDR)
    Display *display = open_display();
    if (!display)
        return 1;

    int event_base, error_base, count = 1;
    if (XRRQueryExtension(display, &event_base, &error_base)) {
        int n = 0;
        XRRMonitorInfo *monitors = XRRGetMonitors(display, DefaultRootWindow(display), True, &n);
        if (monitors) {
            count = n;
            XRRFreeMonitors(monitors);
        }
    }
    XCloseDisplay(display);
    return count;
#else
    return 1;
#endif
}

//-------------------------------------
bool
mfb_get_monitor_info(unsigned index, mfb_monitor_info *out_info) {
    if (out_info == NULL)
        return false;

    memset(out_info, 0, sizeof(*out_info));

#if defined(HAVE_XRANDR)
    Display *display = open_display();
    if (!display) {
        if (index != 0) return false;
        fill_scale(NULL, NULL, &out_info->scale_x, &out_info->scale_y);
        out_info->is_primary = true;
        return true;
    }

    int event_base, error_base;
    bool ok = false;
    if (XRRQueryExtension(display, &event_base, &error_base)) {
        int n = 0;
        // True = primary monitor first
        XRRMonitorInfo *monitors = XRRGetMonitors(display, DefaultRootWindow(display), True, &n);
        if (monitors && index < (unsigned) n) {
            XRRMonitorInfo *m = &monitors[index];

            out_info->logical_x      = m->x;
            out_info->logical_y      = m->y;
            out_info->logical_width  = (unsigned) m->width;
            out_info->logical_height = (unsigned) m->height;
            out_info->is_primary     = (m->primary != 0);

            fill_scale(display, m, &out_info->scale_x, &out_info->scale_y);

            // Physical size = logical size * scale (X11 logical == physical pixels,
            // scale comes from DPI / 96; framebuffer coords match logical coords)
            out_info->physical_width  = (unsigned) (m->width  * out_info->scale_x);
            out_info->physical_height = (unsigned) (m->height * out_info->scale_y);

            char *name = XGetAtomName(display, m->name);
            if (name) {
                strncpy(out_info->name, name, sizeof(out_info->name) - 1);
                out_info->name[sizeof(out_info->name) - 1] = '\0';
                XFree(name);
            }
            ok = true;
        }
        if (monitors)
            XRRFreeMonitors(monitors);
    }
    XCloseDisplay(display);

    if (!ok && index == 0) {
        out_info->scale_x    = 1.0f;
        out_info->scale_y    = 1.0f;
        out_info->is_primary = true;
        ok = true;
    }
    return ok;
#else
    if (index != 0)
        return false;

    {
        Display *display = open_display();
        fill_scale(display, &out_info->scale_x, &out_info->scale_y);
        if (display)
            XCloseDisplay(display);
    }

    out_info->is_primary = true;
    return true;
#endif
}

//-------------------------------------
static mfb_monitor_info g_window_monitor_cache;

mfb_monitor_info *
mfb_get_window_monitor(struct mfb_window *window) {
    if (window == NULL)
        return NULL;

#if defined(HAVE_XRANDR)
    SWindowData     *window_data = (SWindowData *) window;
    SWindowData_X11 *x11         = (SWindowData_X11 *) window_data->specific;
    if (x11 == NULL)
        return NULL;

    int event_base, error_base;
    if (!XRRQueryExtension(x11->display, &event_base, &error_base))
        goto fallback;

    Window child;
    int win_x = 0, win_y = 0;
    XTranslateCoordinates(x11->display, x11->window,
                          DefaultRootWindow(x11->display),
                          0, 0, &win_x, &win_y, &child);

    XWindowAttributes wa;
    XGetWindowAttributes(x11->display, x11->window, &wa);
    int cx = win_x + wa.width  / 2;
    int cy = win_y + wa.height / 2;

    int n = 0;
    XRRMonitorInfo *monitors = XRRGetMonitors(x11->display,
                                               DefaultRootWindow(x11->display),
                                               True, &n);
    if (!monitors)
        goto fallback;

    int best = 0;
    for (int i = 0; i < n; i++) {
        XRRMonitorInfo *m = &monitors[i];
        if (cx >= m->x && cx < m->x + m->width &&
            cy >= m->y && cy < m->y + m->height) {
            best = i;
            break;
        }
    }

    mfb_monitor_info *info = &g_window_monitor_cache;
    memset(info, 0, sizeof(*info));
    XRRMonitorInfo *m = &monitors[best];
    info->logical_x      = m->x;
    info->logical_y      = m->y;
    info->logical_width  = (unsigned) m->width;
    info->logical_height = (unsigned) m->height;
    info->is_primary     = (m->primary != 0);

    fill_scale(x11->display, m, &info->scale_x, &info->scale_y);
    info->physical_width  = (unsigned) (m->width  * info->scale_x);
    info->physical_height = (unsigned) (m->height * info->scale_y);

    char *name = XGetAtomName(x11->display, m->name);
    if (name) {
        strncpy(info->name, name, sizeof(info->name) - 1);
        info->name[sizeof(info->name) - 1] = '\0';
        XFree(name);
    }
    XRRFreeMonitors(monitors);
    return info;

fallback:
#endif
    mfb_get_monitor_info(0, &g_window_monitor_cache);
    return &g_window_monitor_cache;
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
    if (flags & MFB_WF_SIZE_PHYSICAL) {
        if (target.scale_x > 0.0f) open_w = (unsigned) (width  / target.scale_x);
        if (target.scale_y > 0.0f) open_h = (unsigned) (height / target.scale_y);
    }

    unsigned open_flags = flags & ~(unsigned)(MFB_WF_SIZE_LOGICAL | MFB_WF_SIZE_PHYSICAL);
    struct mfb_window *window = mfb_open_ex(title, open_w, open_h, open_flags);
    if (window == NULL)
        return NULL;

#if defined(HAVE_XRANDR)
    SWindowData     *window_data = (SWindowData *) window;
    SWindowData_X11 *x11         = (SWindowData_X11 *) window_data->specific;
    if (x11 != NULL) {
        XWindowAttributes wa;
        XGetWindowAttributes(x11->display, x11->window, &wa);
        int x = target.logical_x + ((int) target.logical_width  - wa.width)  / 2;
        int y = target.logical_y + ((int) target.logical_height - wa.height) / 2;
        XMoveWindow(x11->display, x11->window, x, y);
        XFlush(x11->display);
    }
#endif

    return window;
}
