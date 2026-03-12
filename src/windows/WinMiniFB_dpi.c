#include "WinMiniFB_dpi.h"
#include <stdio.h>

//-------------------------------------
HMODULE                           mfb_user32_dll                    = NULL;
PFN_SetProcessDPIAware            mfb_SetProcessDPIAware            = NULL;
PFN_SetProcessDpiAwarenessContext mfb_SetProcessDpiAwarenessContext = NULL;
PFN_GetDpiForWindow               mfb_GetDpiForWindow               = NULL;
PFN_EnableNonClientDpiScaling     mfb_EnableNonClientDpiScaling     = NULL;
PFN_AdjustWindowRectExForDpi      mfb_AdjustWindowRectExForDpi      = NULL;

HMODULE                           mfb_shcore_dll                    = NULL;
PFN_SetProcessDpiAwareness        mfb_SetProcessDpiAwareness        = NULL;
PFN_GetDpiForMonitor              mfb_GetDpiForMonitor              = NULL;

//-------------------------------------
void
load_functions(void) {
    if (mfb_user32_dll == NULL) {
        mfb_user32_dll = LoadLibraryA("user32.dll");
        if (mfb_user32_dll != NULL) {
            mfb_SetProcessDPIAware            = (PFN_SetProcessDPIAware)            GetProcAddress(mfb_user32_dll, "SetProcessDPIAware");
            mfb_SetProcessDpiAwarenessContext = (PFN_SetProcessDpiAwarenessContext) GetProcAddress(mfb_user32_dll, "SetProcessDpiAwarenessContext");
            mfb_GetDpiForWindow               = (PFN_GetDpiForWindow)               GetProcAddress(mfb_user32_dll, "GetDpiForWindow");
            mfb_EnableNonClientDpiScaling     = (PFN_EnableNonClientDpiScaling)     GetProcAddress(mfb_user32_dll, "EnableNonClientDpiScaling");
            mfb_AdjustWindowRectExForDpi      = (PFN_AdjustWindowRectExForDpi)      GetProcAddress(mfb_user32_dll, "AdjustWindowRectExForDpi");
        }
    }

    if (mfb_shcore_dll == NULL) {
        mfb_shcore_dll = LoadLibraryA("shcore.dll");
        if (mfb_shcore_dll != NULL) {
            mfb_SetProcessDpiAwareness = (PFN_SetProcessDpiAwareness) GetProcAddress(mfb_shcore_dll, "SetProcessDpiAwareness");
            mfb_GetDpiForMonitor       = (PFN_GetDpiForMonitor)       GetProcAddress(mfb_shcore_dll, "GetDpiForMonitor");
        }
    }
}

//-------------------------------------
// NOT Thread safe. Just convenient (Don't do this at home guys)
static char s_error_buffer[256];

char *
GetErrorMessage(void) {
    s_error_buffer[0] = 0;
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                  NULL,
                  GetLastError(),
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  s_error_buffer,
                  sizeof(s_error_buffer),
                  NULL);
    return s_error_buffer;
}

//-------------------------------------
void
dpi_aware(void) {
    if (mfb_SetProcessDpiAwarenessContext != NULL) {
        if (mfb_SetProcessDpiAwarenessContext(mfb_DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2) == false) {
            uint32_t error = GetLastError();
            if (error == ERROR_ACCESS_DENIED) {
                // Already set (called more than once, or set via application manifest).
                return;
            }
            if (error == ERROR_INVALID_PARAMETER) {
                error = NO_ERROR;
                if (mfb_SetProcessDpiAwarenessContext(mfb_DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE) == false) {
                    error = GetLastError();
                    if (error == ERROR_ACCESS_DENIED)
                        return;  // Already set
                }
            }
            if (error != NO_ERROR) {
                fprintf(stderr, "Error (SetProcessDpiAwarenessContext): %s\n", GetErrorMessage());
            }
        }
    }
    else if (mfb_SetProcessDpiAwareness != NULL) {
        if (mfb_SetProcessDpiAwareness(mfb_PROCESS_PER_MONITOR_DPI_AWARE) != S_OK) {
            fprintf(stderr, "Error (SetProcessDpiAwareness): %s\n", GetErrorMessage());
        }
    }
    else if (mfb_SetProcessDPIAware != NULL) {
        if (mfb_SetProcessDPIAware() == false) {
            fprintf(stderr, "Error (SetProcessDPIAware): %s\n", GetErrorMessage());
        }
    }
}

//-------------------------------------
void
get_monitor_scale(HWND hWnd, float *scale_x, float *scale_y) {
    UINT x, y;

    if (mfb_GetDpiForMonitor != NULL) {
        HMONITOR monitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
        mfb_GetDpiForMonitor(monitor, mfb_MDT_EFFECTIVE_DPI, &x, &y);
    }
    else {
        const HDC dc = GetDC(hWnd);
        x = GetDeviceCaps(dc, LOGPIXELSX);
        y = GetDeviceCaps(dc, LOGPIXELSY);
        ReleaseDC(NULL, dc);
    }

    if (scale_x) {
        *scale_x = x / (float) USER_DEFAULT_SCREEN_DPI;
        if (*scale_x == 0) *scale_x = 1;
    }
    if (scale_y) {
        *scale_y = y / (float) USER_DEFAULT_SCREEN_DPI;
        if (*scale_y == 0) *scale_y = 1;
    }
}
