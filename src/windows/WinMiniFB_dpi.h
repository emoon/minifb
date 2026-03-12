#pragma once

#include <windows.h>
#include <stdint.h>
#include <stdbool.h>

// Copied (and modified) from Windows Kit 10 to avoid setting _WIN32_WINNT to a higher version
//-------------------------------------
typedef enum mfb_PROCESS_DPI_AWARENESS {
    mfb_PROCESS_DPI_UNAWARE           = 0,
    mfb_PROCESS_SYSTEM_DPI_AWARE      = 1,
    mfb_PROCESS_PER_MONITOR_DPI_AWARE = 2
} mfb_PROCESS_DPI_AWARENESS;

typedef enum mfb_MONITOR_DPI_TYPE {
    mfb_MDT_EFFECTIVE_DPI             = 0,
    mfb_MDT_ANGULAR_DPI               = 1,
    mfb_MDT_RAW_DPI                   = 2,
    mfb_MDT_DEFAULT                   = mfb_MDT_EFFECTIVE_DPI
} mfb_MONITOR_DPI_TYPE;

#define mfb_DPI_AWARENESS_CONTEXT_UNAWARE               ((HANDLE) -1)
#define mfb_DPI_AWARENESS_CONTEXT_SYSTEM_AWARE          ((HANDLE) -2)
#define mfb_DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE     ((HANDLE) -3)
#define mfb_DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2  ((HANDLE) -4)
#define mfb_DPI_AWARENESS_CONTEXT_UNAWARE_GDISCALED     ((HANDLE) -5)

// Windows message constants (not available in older Windows SDK versions)
#if !defined(WM_GETDPISCALEDSIZE)
    #define WM_GETDPISCALEDSIZE 0x02E4
#endif
#if !defined(WM_DPICHANGED)
    #define WM_DPICHANGED 0x02E0
#endif

// user32.dll
typedef BOOL(WINAPI    *PFN_SetProcessDPIAware)(void);
typedef BOOL(WINAPI    *PFN_SetProcessDpiAwarenessContext)(HANDLE);
typedef UINT(WINAPI    *PFN_GetDpiForWindow)(HWND);
typedef BOOL(WINAPI    *PFN_EnableNonClientDpiScaling)(HWND);
typedef BOOL(WINAPI    *PFN_AdjustWindowRectExForDpi)(LPRECT lpRect, DWORD dwStyle, BOOL bMenu, DWORD dwExStyle, UINT dpi);

extern HMODULE                           mfb_user32_dll;
extern PFN_SetProcessDPIAware            mfb_SetProcessDPIAware;
extern PFN_SetProcessDpiAwarenessContext mfb_SetProcessDpiAwarenessContext;
extern PFN_GetDpiForWindow               mfb_GetDpiForWindow;
extern PFN_EnableNonClientDpiScaling     mfb_EnableNonClientDpiScaling;
extern PFN_AdjustWindowRectExForDpi      mfb_AdjustWindowRectExForDpi;

// shcore.dll
typedef HRESULT(WINAPI *PFN_SetProcessDpiAwareness)(mfb_PROCESS_DPI_AWARENESS);
typedef HRESULT(WINAPI *PFN_GetDpiForMonitor)(HMONITOR, mfb_MONITOR_DPI_TYPE, UINT *, UINT *);

extern HMODULE                    mfb_shcore_dll;
extern PFN_SetProcessDpiAwareness mfb_SetProcessDpiAwareness;
extern PFN_GetDpiForMonitor       mfb_GetDpiForMonitor;

//-------------------------------------
void load_functions(void);
void dpi_aware(void);
void get_monitor_scale(HWND hWnd, float *scale_x, float *scale_y);
