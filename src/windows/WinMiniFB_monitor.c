#include <MiniFB.h>
#include <MiniFB_internal.h>
#include <WindowData.h>
#include "WindowData_Win.h"
#include "WinMiniFB_dpi.h"

#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Internal helper: get scale for a given HMONITOR using shared DPI functions.
// load_functions() + dpi_aware() must have been called first.
// ---------------------------------------------------------------------------
static void
get_scale_for_hmonitor(HMONITOR hMonitor, float *scale_x, float *scale_y) {
    UINT x = 96, y = 96;

    if (mfb_GetDpiForMonitor != NULL)
        mfb_GetDpiForMonitor(hMonitor, mfb_MDT_EFFECTIVE_DPI, &x, &y);
    else {
        HDC dc = GetDC(NULL);
        x = (UINT) GetDeviceCaps(dc, LOGPIXELSX);
        y = (UINT) GetDeviceCaps(dc, LOGPIXELSY);
        ReleaseDC(NULL, dc);
    }

    if (scale_x) { *scale_x = x / (float) USER_DEFAULT_SCREEN_DPI; if (*scale_x == 0.0f) *scale_x = 1.0f; }
    if (scale_y) { *scale_y = y / (float) USER_DEFAULT_SCREEN_DPI; if (*scale_y == 0.0f) *scale_y = 1.0f; }
}

// ---------------------------------------------------------------------------
// Monitor enumeration helper
// ---------------------------------------------------------------------------

#define kMAX_MONITORS 32

typedef struct {
    mfb_monitor_info infos[kMAX_MONITORS];
    unsigned         count;
} MonitorList;

static BOOL CALLBACK
enum_monitor_proc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
    (void) hdcMonitor;
    (void) lprcMonitor;
    MonitorList *list = (MonitorList *) dwData;
    if (list->count >= kMAX_MONITORS)
        return TRUE;

    MONITORINFOEXW mi;
    memset(&mi, 0, sizeof(mi));
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfoW(hMonitor, (MONITORINFO *) &mi))
        return TRUE;

    mfb_monitor_info *info = &list->infos[list->count];
    memset(info, 0, sizeof(*info));

    info->logical_x      = (int) mi.rcMonitor.left;
    info->logical_y      = (int) mi.rcMonitor.top;
    info->logical_width  = (unsigned) (mi.rcMonitor.right  - mi.rcMonitor.left);
    info->logical_height = (unsigned) (mi.rcMonitor.bottom - mi.rcMonitor.top);
    info->is_primary     = (mi.dwFlags & MONITORINFOF_PRIMARY) != 0;

    float sx = 1.0f, sy = 1.0f;
    get_scale_for_hmonitor(hMonitor, &sx, &sy);
    info->scale_x         = sx;
    info->scale_y         = sy;
    info->physical_width  = (unsigned) (info->logical_width  * sx);
    info->physical_height = (unsigned) (info->logical_height * sy);

    WideCharToMultiByte(CP_UTF8, 0, mi.szDevice, -1,
                        info->name, (int) sizeof(info->name) - 1,
                        NULL, NULL);
    info->name[sizeof(info->name) - 1] = '\0';

    list->count++;
    return TRUE;
}

// Populate list, primary monitor first.
static void
enumerate_monitors(MonitorList *list) {
    // Ensure DPI awareness and function pointers are ready before querying DPI.
    load_functions();
    dpi_aware();

    list->count = 0;
    EnumDisplayMonitors(NULL, NULL, enum_monitor_proc, (LPARAM) list);

    // Bubble primary to index 0
    if (list->count > 1 && !list->infos[0].is_primary) {
        for (unsigned i = 1; i < list->count; i++) {
            if (list->infos[i].is_primary) {
                mfb_monitor_info tmp = list->infos[0];
                list->infos[0]       = list->infos[i];
                list->infos[i]       = tmp;
                break;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

int
mfb_get_num_monitors(void) {
    return GetSystemMetrics(SM_CMONITORS);
}

//-------------------------------------
bool
mfb_get_monitor_info(unsigned index, mfb_monitor_info *out_info) {
    if (out_info == NULL)
        return false;

    MonitorList list;
    enumerate_monitors(&list);

    if (index >= list.count)
        return false;

    *out_info = list.infos[index];
    return true;
}

//-------------------------------------
static mfb_monitor_info g_window_monitor_cache;

mfb_monitor_info *
mfb_get_window_monitor(struct mfb_window *window) {
    if (window == NULL)
        return NULL;

    SWindowData     *window_data  = (SWindowData *) window;
    SWindowData_Win *win_specific = (SWindowData_Win *) window_data->specific;
    if (win_specific == NULL)
        return NULL;

    HMONITOR hMon = MonitorFromWindow(win_specific->window, MONITOR_DEFAULTTONEAREST);

    MONITORINFOEXW mi;
    memset(&mi, 0, sizeof(mi));
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfoW(hMon, (MONITORINFO *) &mi))
        return NULL;

    mfb_monitor_info *info = &g_window_monitor_cache;
    memset(info, 0, sizeof(*info));

    info->logical_x      = (int) mi.rcMonitor.left;
    info->logical_y      = (int) mi.rcMonitor.top;
    info->logical_width  = (unsigned) (mi.rcMonitor.right  - mi.rcMonitor.left);
    info->logical_height = (unsigned) (mi.rcMonitor.bottom - mi.rcMonitor.top);
    info->is_primary     = (mi.dwFlags & MONITORINFOF_PRIMARY) != 0;

    float sx = 1.0f, sy = 1.0f;
    get_scale_for_hmonitor(hMon, &sx, &sy);
    info->scale_x         = sx;
    info->scale_y         = sy;
    info->physical_width  = (unsigned) (info->logical_width  * sx);
    info->physical_height = (unsigned) (info->logical_height * sy);

    WideCharToMultiByte(CP_UTF8, 0, mi.szDevice, -1,
                        info->name, (int) sizeof(info->name) - 1,
                        NULL, NULL);
    info->name[sizeof(info->name) - 1] = '\0';

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
    if (!mfb_get_monitor_info(monitor_index, &target)) {
        if (!mfb_get_monitor_info(0, &target))
            memset(&target, 0, sizeof(target));
    }

    unsigned open_w = width, open_h = height;
    if (flags & MFB_WF_SIZE_PHYSICAL) {
        if (target.scale_x > 0.0f) open_w = (unsigned) (width  / target.scale_x);
        if (target.scale_y > 0.0f) open_h = (unsigned) (height / target.scale_y);
    }

    unsigned open_flags = flags & ~(unsigned)(MFB_WF_SIZE_LOGICAL | MFB_WF_SIZE_PHYSICAL);

    struct mfb_window *window = mfb_open_ex(title, open_w, open_h, open_flags);
    if (window == NULL)
        return NULL;

    SWindowData     *window_data  = (SWindowData *) window;
    SWindowData_Win *win_specific = (SWindowData_Win *) window_data->specific;
    if (win_specific != NULL && win_specific->window != NULL) {
        RECT wr;
        GetWindowRect(win_specific->window, &wr);
        int win_w = wr.right  - wr.left;
        int win_h = wr.bottom - wr.top;

        int cx = target.logical_x + (int)(target.logical_width  / 2);
        int cy = target.logical_y + (int)(target.logical_height / 2);
        int x  = cx - win_w / 2;
        int y  = cy - win_h / 2;

        SetWindowPos(win_specific->window, NULL, x, y, 0, 0,
                     SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }

    return window;
}
