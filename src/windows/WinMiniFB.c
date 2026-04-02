#include <MiniFB.h>
#include <MiniFB_internal.h>
#include <WindowData.h>
#include "WindowData_Win.h"
#if defined(USE_OPENGL_API)
    #include "gl/MiniFB_GL.h"
#endif
#include <stdlib.h>

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

#define MFB_DPI_AWARENESS_CONTEXT_UNAWARE               ((HANDLE) -1)
#define MFB_DPI_AWARENESS_CONTEXT_SYSTEM_AWARE          ((HANDLE) -2)
#define MFB_DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE     ((HANDLE) -3)
#define MFB_DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2  ((HANDLE) -4)
#define MFB_DPI_AWARENESS_CONTEXT_UNAWARE_GDISCALED     ((HANDLE) -5)

// Windows message constants (not available in older Windows SDK versions)
#if !defined(WM_GETDPISCALEDSIZE)
    #define WM_GETDPISCALEDSIZE 0x02E4
#endif

#if !defined(WM_DPICHANGED)
    #define WM_DPICHANGED 0x02E0
#endif

// user32.dll
typedef BOOL(WINAPI *PFN_SetProcessDPIAware)(void);
typedef BOOL(WINAPI *PFN_SetProcessDpiAwarenessContext)(HANDLE);
typedef UINT(WINAPI *PFN_GetDpiForWindow)(HWND);
typedef BOOL(WINAPI *PFN_EnableNonClientDpiScaling)(HWND);
typedef BOOL(WINAPI *PFN_AdjustWindowRectExForDpi)(LPRECT lpRect, DWORD dwStyle, BOOL bMenu, DWORD dwExStyle, UINT dpi);

HMODULE                           mfb_user32_dll                    = NULL;
PFN_SetProcessDPIAware            mfb_SetProcessDPIAware            = NULL;
PFN_SetProcessDpiAwarenessContext mfb_SetProcessDpiAwarenessContext = NULL;
PFN_GetDpiForWindow               mfb_GetDpiForWindow               = NULL;
PFN_EnableNonClientDpiScaling     mfb_EnableNonClientDpiScaling     = NULL;
PFN_AdjustWindowRectExForDpi      mfb_AdjustWindowRectExForDpi      = NULL;

// shcore.dll
typedef HRESULT(WINAPI *PFN_SetProcessDpiAwareness)(mfb_PROCESS_DPI_AWARENESS);
typedef HRESULT(WINAPI *PFN_GetDpiForMonitor)(HMONITOR, mfb_MONITOR_DPI_TYPE, UINT *, UINT *);

HMODULE                           mfb_shcore_dll                    = NULL;
PFN_SetProcessDpiAwareness        mfb_SetProcessDpiAwareness        = NULL;
PFN_GetDpiForMonitor              mfb_GetDpiForMonitor              = NULL;

void
load_functions() {
    if (mfb_user32_dll == NULL) {
        mfb_user32_dll = LoadLibraryA("user32.dll");
        if (mfb_user32_dll != NULL) {
            mfb_SetProcessDPIAware = (PFN_SetProcessDPIAware) GetProcAddress(mfb_user32_dll, "SetProcessDPIAware");
            mfb_SetProcessDpiAwarenessContext = (PFN_SetProcessDpiAwarenessContext) GetProcAddress(mfb_user32_dll, "SetProcessDpiAwarenessContext");
            mfb_GetDpiForWindow = (PFN_GetDpiForWindow) GetProcAddress(mfb_user32_dll, "GetDpiForWindow");
            mfb_EnableNonClientDpiScaling = (PFN_EnableNonClientDpiScaling) GetProcAddress(mfb_user32_dll, "EnableNonClientDpiScaling");
            mfb_AdjustWindowRectExForDpi = (PFN_AdjustWindowRectExForDpi) GetProcAddress(mfb_user32_dll, "AdjustWindowRectExForDpi");
        }
    }

    if (mfb_shcore_dll == NULL) {
        mfb_shcore_dll = LoadLibraryA("shcore.dll");
        if (mfb_shcore_dll != NULL) {
            mfb_SetProcessDpiAwareness = (PFN_SetProcessDpiAwareness) GetProcAddress(mfb_shcore_dll, "SetProcessDpiAwareness");
            mfb_GetDpiForMonitor = (PFN_GetDpiForMonitor) GetProcAddress(mfb_shcore_dll, "GetDpiForMonitor");
        }
    }
}

// NOT Thread safe. Just convenient (Don't do this at home guys)
//-------------------------------------
char *
get_error_message() {
    static char buffer[256];

    buffer[0] = 0;
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                  NULL,  // Not used with FORMAT_MESSAGE_FROM_SYSTEM
                  GetLastError(),
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  buffer,
                  sizeof(buffer),
                  NULL);

    return buffer;
}

//-------------------------------------
void
dpi_aware() {
    if (mfb_SetProcessDpiAwarenessContext != NULL) {
        if (mfb_SetProcessDpiAwarenessContext(MFB_DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2) == false) {
            uint32_t error = GetLastError();
            if (error == ERROR_INVALID_PARAMETER) {
                error = NO_ERROR;
                if (mfb_SetProcessDpiAwarenessContext(MFB_DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE) == false) {
                    error = GetLastError();
                }
            }
            if (error != NO_ERROR) {
                MFB_LOG(MFB_LOG_WARNING, "SetProcessDpiAwarenessContext failed: %s", get_error_message());
            }
        }
    }
    else if (mfb_SetProcessDpiAwareness != NULL) {
        if (mfb_SetProcessDpiAwareness(mfb_PROCESS_PER_MONITOR_DPI_AWARE) != S_OK) {
            MFB_LOG(MFB_LOG_WARNING, "SetProcessDpiAwareness failed: %s", get_error_message());
        }
    }
    else if (mfb_SetProcessDPIAware != NULL) {
        if (mfb_SetProcessDPIAware() == false) {
            MFB_LOG(MFB_LOG_WARNING, "SetProcessDPIAware failed: %s", get_error_message());
        }
    }
}

//-------------------------------------
void
get_monitor_scale(HWND hWnd, float *scale_x, float *scale_y) {
    UINT    x = USER_DEFAULT_SCREEN_DPI;
    UINT    y = USER_DEFAULT_SCREEN_DPI;
    bool    got_dpi = false;

    if (mfb_GetDpiForMonitor != NULL) {
        HMONITOR monitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
        if (monitor != NULL && SUCCEEDED(mfb_GetDpiForMonitor(monitor, mfb_MDT_EFFECTIVE_DPI, &x, &y))) {
            got_dpi = true;
        }
    }

    if (!got_dpi) {
        const HDC dc = GetDC(hWnd);
        if (dc != NULL) {
            x = GetDeviceCaps(dc, LOGPIXELSX);
            y = GetDeviceCaps(dc, LOGPIXELSY);
            ReleaseDC(hWnd, dc);
        }
    }

    if (scale_x) {
        *scale_x = x / (float) USER_DEFAULT_SCREEN_DPI;
        if (*scale_x == 0) {
            *scale_x = 1;
        }
    }

    if (scale_y) {
        *scale_y = y / (float) USER_DEFAULT_SCREEN_DPI;
        if (*scale_y == 0) {
            *scale_y = 1;
        }
    }
}

//-------------------------------------
static unsigned g_window_counter = 0;   // (not thread safe)

//-------------------------------------
static void
release_window_counter(void) {
    if (g_window_counter > 0) {
        --g_window_counter;
        if (g_window_counter == 0) {
            timeEndPeriod(1);
        }
    }
}

//-------------------------------------
static void
destroy_window_data(SWindowData *window_data) {
    if (window_data == NULL) {
        return;
    }

    release_cpp_stub((struct mfb_window *) window_data);

    SWindowData_Win *window_data_specific = (SWindowData_Win *) window_data->specific;
    if (window_data_specific == NULL) {
        release_window_counter();
        free(window_data);
        return;
    }

#if !defined(USE_OPENGL_API)
    if (window_data_specific->bitmap_info != NULL) {
        free(window_data_specific->bitmap_info);
        window_data_specific->bitmap_info = NULL;
    }
#else
    destroy_GL_context(window_data);
#endif

    if (window_data_specific->window != 0 && window_data_specific->hdc != 0) {
        ReleaseDC(window_data_specific->window, window_data_specific->hdc);
    }
    if (window_data_specific->window != 0 && IsWindow(window_data_specific->window)) {
        SetWindowLongPtr(window_data_specific->window, GWLP_USERDATA, 0);
        DestroyWindow(window_data_specific->window);
    }

    window_data_specific->window = 0;
    window_data_specific->hdc    = 0;

    mfb_timer_destroy(window_data_specific->timer);
    window_data_specific->timer = NULL;

    window_data->draw_buffer = NULL;
    window_data->close       = true;

    // To be able to sleep 1 ms on Windows (not thread safe)
    release_window_counter();

    memset(window_data_specific, 0, sizeof(SWindowData_Win));
    free(window_data_specific);
    window_data->specific = NULL;

    memset(window_data, 0, sizeof(SWindowData));
    free(window_data);
}

//-------------------------------------
void     init_keycodes();
uint32_t translate_mod();
mfb_key  translate_key(unsigned int wParam, unsigned long lParam);

//-------------------------------------
LRESULT CALLBACK
WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    LRESULT res = 0;

    SWindowData     *window_data     = (SWindowData *) GetWindowLongPtr(hWnd, GWLP_USERDATA);
    SWindowData_Win *window_data_specific = NULL;
    if (window_data != NULL) {
        window_data_specific = (SWindowData_Win *) window_data->specific;
    }

    switch (message) {
        case WM_NCCREATE:
            if (mfb_EnableNonClientDpiScaling)
                mfb_EnableNonClientDpiScaling(hWnd);

            return DefWindowProc(hWnd, message, wParam, lParam);

        // This message is only sent on Windows 10 v1703+ with Per Monitor v2 awareness
        case WM_GETDPISCALEDSIZE:
        {
            const uint32_t dpi = (uint32_t) wParam;

            // Scale factor relative to current DPI
            const uint32_t current_dpi = mfb_GetDpiForWindow != NULL ? mfb_GetDpiForWindow(hWnd) : USER_DEFAULT_SCREEN_DPI;
            const float scale = dpi / (float) current_dpi;

            RECT client_rect;

            GetClientRect(hWnd, &client_rect); // Client area
            LONG client_width  = client_rect.right  - client_rect.left;
            LONG client_height = client_rect.bottom - client_rect.top;

            // Desired client size in physical pixels on this monitor
            client_rect.left   = 0;
            client_rect.top    = 0;
            client_rect.right  = (LONG) (client_width  * scale);
            client_rect.bottom = (LONG) (client_height * scale);

            if (mfb_AdjustWindowRectExForDpi != NULL) {
                // Turn client size into window size for this DPI
                DWORD style   = (DWORD) (GetWindowLongPtr(hWnd, GWL_STYLE));
                DWORD exStyle = (DWORD) (GetWindowLongPtr(hWnd, GWL_EXSTYLE));
                if (!mfb_AdjustWindowRectExForDpi(&client_rect, style, FALSE, exStyle, dpi)) {
                    return 0; // Not handled
                }
            }
            else {
                return 0; // Not handled
            }

            SIZE *new_size = (SIZE *) lParam;
            new_size->cx   = client_rect.right  - client_rect.left;
            new_size->cy   = client_rect.bottom - client_rect.top;

            // Handled
            return 1;
        }

        case WM_DPICHANGED:
        {
            if (window_data) {
                const RECT *   pRect  = (RECT *) (lParam);
                const uint32_t width  = pRect->right  - pRect->left;
                const uint32_t height = pRect->bottom - pRect->top;

                MFB_LOG(MFB_LOG_DEBUG, "WM_DPICHANGED: applying suggested bounds %ux%u at (%ld,%ld)",
                        width, height, pRect->left, pRect->top);

                SetWindowPos(hWnd,
                            NULL,
                            pRect->left,
                            pRect->top,
                            width,
                            height,
                            SWP_NOZORDER | SWP_NOACTIVATE);
            }
            return 0;
        }


#if !defined(USE_OPENGL_API)
        case WM_PAINT:
            if (window_data && window_data->draw_buffer && window_data_specific) {
                StretchDIBits(window_data_specific->hdc, window_data->dst_offset_x, window_data->dst_offset_y, window_data->dst_width, window_data->dst_height, 0, 0, window_data->buffer_width, window_data->buffer_height, window_data->draw_buffer,
                              window_data_specific->bitmap_info, DIB_RGB_COLORS, SRCCOPY);
            }
            ValidateRect(hWnd, NULL);
            break;
#endif
        case WM_CLOSE:
            if (window_data) {
                // Obtain a confirmation of close
                if (!window_data->close_func || window_data->close_func((struct mfb_window *) window_data)) {
                    window_data->close = true;
                }
            }
            break;

        case WM_DESTROY:
            if (window_data) {
                window_data->close = true;
            }
            break;

        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYUP:
            if (window_data) {
                mfb_key key_code      = translate_key((unsigned int) wParam, (unsigned long) lParam);
                int is_pressed        = !((lParam >> 31) & 1);
                window_data->mod_keys = translate_mod();

                if (key_code == MFB_KB_KEY_UNKNOWN)
                    return FALSE;

                window_data->key_status[key_code] = (uint8_t) is_pressed;
                kCall(keyboard_func, key_code, window_data->mod_keys, is_pressed);
            }
            break;

        case WM_CHAR:
        case WM_SYSCHAR:
            if (window_data && window_data_specific) {
                if (wParam >= 0xd800 && wParam <= 0xdbff) {
                    window_data_specific->high_surrogate = (WCHAR) wParam;
                }
                else {
                    unsigned int codepoint = 0;
                    if (wParam >= 0xdc00 && wParam <= 0xdfff) {
                        if (window_data_specific->high_surrogate != 0) {
                            codepoint += (window_data_specific->high_surrogate - 0xd800) << 10;
                            codepoint += (WCHAR) wParam - 0xdc00;
                            codepoint += 0x10000;
                        }
                    }
                    else {
                        codepoint = (WCHAR) wParam;
                    }
                    window_data_specific->high_surrogate = 0;
                    kCall(char_input_func, codepoint);
                }
            }
            break;

        case WM_UNICHAR:
            if (window_data) {
                if (wParam == UNICODE_NOCHAR) {
                    // WM_UNICHAR is not sent by Windows, but is sent by some third-party input method engine
                    // Returning TRUE here announces support for this message
                    return TRUE;
                }

                kCall(char_input_func, (unsigned int) wParam);
            }
            break;

        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MBUTTONUP:
        case WM_XBUTTONUP:
        case WM_LBUTTONDOWN:
        //case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDOWN:
        //case WM_RBUTTONDBLCLK:
        case WM_MBUTTONDOWN:
        //case WM_MBUTTONDBLCLK:
        case WM_XBUTTONDOWN:
        //case WM_XBUTTONDBLCLK:
            if (window_data) {
                mfb_mouse_button button = MFB_MOUSE_BTN_0;
                window_data->mod_keys   = translate_mod();
                int          is_pressed = 0;

                switch(message) {
                    //case WM_LBUTTONDBLCLK:
                    case WM_LBUTTONDOWN:
                        is_pressed = 1;
                    case WM_LBUTTONUP:
                        button = MFB_MOUSE_BTN_1;
                        break;
                    //case WM_RBUTTONDBLCLK:
                    case WM_RBUTTONDOWN:
                        is_pressed = 1;
                    case WM_RBUTTONUP:
                        button = MFB_MOUSE_BTN_2;
                        break;
                    //case WM_MBUTTONDBLCLK:
                    case WM_MBUTTONDOWN:
                        is_pressed = 1;
                    case WM_MBUTTONUP:
                        button = MFB_MOUSE_BTN_3;
                        break;

                    default:
                        button = (GET_XBUTTON_WPARAM(wParam) == XBUTTON1 ? MFB_MOUSE_BTN_5 : MFB_MOUSE_BTN_6);
                        if (message == WM_XBUTTONDOWN) { //|| message == WM_XBUTTONDBLCLK) {
                            is_pressed = 1;
                        }
                }
                if (button > MFB_MOUSE_BTN_7) {
                    MFB_LOG(MFB_LOG_WARNING, "Mouse button %d exceeds MFB_MOUSE_BTN_7; ignoring.", button);
                }
                else {
                    window_data->mouse_button_status[button] = is_pressed;
                    kCall(mouse_btn_func, button, window_data->mod_keys, is_pressed);
                }
            }
            break;

        case WM_MOUSEWHEEL:
            if (window_data) {
                window_data->mouse_wheel_y = (SHORT) HIWORD(wParam) / (float) WHEEL_DELTA;
                kCall(mouse_wheel_func, translate_mod(), 0.0f, window_data->mouse_wheel_y);
            }
            break;

        case WM_MOUSEHWHEEL:
            // This message is only sent on Windows Vista and later
            // NOTE: The X-axis is inverted for consistency with macOS and X11
            if (window_data) {
                window_data->mouse_wheel_x = -((SHORT) HIWORD(wParam) / (float) WHEEL_DELTA);
                kCall(mouse_wheel_func, translate_mod(), window_data->mouse_wheel_x, 0.0f);
            }
            break;

        case WM_MOUSEMOVE:
            if (window_data) {
                if (window_data_specific->mouse_inside == false) {
                    window_data_specific->mouse_inside = true;
                    TRACKMOUSEEVENT tme;
                    ZeroMemory(&tme, sizeof(tme));
                    tme.cbSize = sizeof(tme);
                    tme.dwFlags = TME_LEAVE;
                    tme.hwndTrack = hWnd;
                    TrackMouseEvent(&tme);

                    if (window_data->is_cursor_visible == false) {
                        ShowCursor(FALSE);
                    }
                }
                window_data->mouse_pos_x = (int)(short) LOWORD(lParam);
                window_data->mouse_pos_y = (int)(short) HIWORD(lParam);
                kCall(mouse_move_func, window_data->mouse_pos_x, window_data->mouse_pos_y);
            }
            break;

        case WM_MOUSELEAVE:
            if (window_data_specific) {
                window_data_specific->mouse_inside = false;
            }

            if (window_data && window_data->is_cursor_visible == false) {
                ShowCursor(TRUE);
            }
            break;

        case WM_SIZE:
            if (window_data) {
                if (wParam == SIZE_MINIMIZED) {
                    return res;
                }

                window_data->window_width  = LOWORD(lParam);
                window_data->window_height = HIWORD(lParam);
                resize_dst(window_data, window_data->window_width, window_data->window_height);

#if !defined(USE_OPENGL_API)
                BitBlt(window_data_specific->hdc, 0, 0, window_data->window_width, window_data->window_height, 0, 0, 0, BLACKNESS);
#else
                resize_GL(window_data);
#endif
                if (window_data->window_width != 0 && window_data->window_height != 0) {
                    MFB_LOG(MFB_LOG_DEBUG, "WM_SIZE: window=%ux%u framebuffer=%ux%u",
                            window_data->window_width, window_data->window_height,
                            window_data->buffer_width, window_data->buffer_height);
                    kCall(resize_func, window_data->window_width, window_data->window_height);
                }
            }
            break;

        case WM_SETFOCUS:
            if (window_data) {
                window_data->is_active = true;
                kCall(active_func, true);
            }
            break;

        case WM_KILLFOCUS:
            if (window_data) {
                window_data->is_active = false;
                kCall(active_func, false);
            }
            break;

        default:
            res = DefWindowProc(hWnd, message, wParam, lParam);
    }

    return res;
}

//-------------------------------------
static inline void
update_events(SWindowData *window_data, HWND window) {
    MSG msg;

    if (window_data != NULL) {
        window_data->mouse_wheel_x = 0.0f;
        window_data->mouse_wheel_y = 0.0f;
    }

    while (PeekMessage(&msg, window, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

//-------------------------------------
struct mfb_window *
mfb_open_ex(const char *title, unsigned width, unsigned height, unsigned flags) {
    const unsigned  known_flags = MFB_WF_RESIZABLE | MFB_WF_FULLSCREEN | MFB_WF_FULLSCREEN_DESKTOP | MFB_WF_BORDERLESS | MFB_WF_ALWAYS_ON_TOP;
    const char      *window_title = (title != NULL && title[0] != '\0') ? title : "minifb";
    RECT            rect = { 0 };
    int             x = 0, y = 0;
    int             show_window_cmd = SW_NORMAL;
    uint32_t        buffer_stride = 0;
    SWindowData     *window_data = NULL;
    SWindowData_Win *window_data_specific = NULL;
    long            window_style = WS_POPUP | WS_SYSMENU | WS_CAPTION;

    if (!calculate_buffer_layout(width, height, &buffer_stride, NULL)) {
        MFB_LOG(MFB_LOG_ERROR, "WinMiniFB: invalid window size %ux%u.", width, height);
        return NULL;
    }

    if ((flags & ~known_flags) != 0u) {
        MFB_LOG(MFB_LOG_WARNING, "WinMiniFB: unknown window flags 0x%x will be ignored.", flags & ~known_flags);
    }

    if ((flags & MFB_WF_FULLSCREEN) && (flags & MFB_WF_FULLSCREEN_DESKTOP)) {
        MFB_LOG(MFB_LOG_WARNING, "WinMiniFB: MFB_WF_FULLSCREEN and MFB_WF_FULLSCREEN_DESKTOP were both requested; MFB_WF_FULLSCREEN takes precedence.");
    }

    if (g_window_counter == 0) {
        timeBeginPeriod(1); // To be able to sleep 1 ms on Windows
        load_functions();
        dpi_aware();
        init_keycodes();
    }
    ++g_window_counter;

    window_data = malloc(sizeof(SWindowData));
    if (window_data == NULL) {
        MFB_LOG(MFB_LOG_ERROR, "Cannot allocate SWindowData");
        release_window_counter();
        return NULL;
    }
    memset(window_data, 0, sizeof(SWindowData));

    window_data_specific = malloc(sizeof(SWindowData_Win));
    if (window_data_specific == NULL) {
        MFB_LOG(MFB_LOG_ERROR, "Cannot allocate SWindowData_Win");
        free(window_data);
        release_window_counter();
        return NULL;
    }
    memset(window_data_specific, 0, sizeof(SWindowData_Win));

    window_data->specific      = window_data_specific;
    window_data->buffer_width  = width;
    window_data->buffer_height = height;
    window_data->buffer_stride = buffer_stride;

    window_data->is_cursor_visible = true;

    window_style = WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME;
    if (flags & MFB_WF_FULLSCREEN) {
        flags = MFB_WF_FULLSCREEN;  // Remove all other flags
        rect.right  = GetSystemMetrics(SM_CXSCREEN);
        rect.bottom = GetSystemMetrics(SM_CYSCREEN);
        window_style = WS_POPUP & ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZE | WS_MAXIMIZE | WS_SYSMENU);

        DEVMODE settings = { 0 };
        EnumDisplaySettings(0, 0, &settings);
        settings.dmPelsWidth  = GetSystemMetrics(SM_CXSCREEN);
        settings.dmPelsHeight = GetSystemMetrics(SM_CYSCREEN);
        settings.dmBitsPerPel = 32;
        settings.dmFields     = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;

        if (ChangeDisplaySettings(&settings, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL) {
            MFB_LOG(MFB_LOG_WARNING, "ChangeDisplaySettings fullscreen failed, falling back to fullscreen desktop");
            flags = MFB_WF_FULLSCREEN_DESKTOP;
        }
    }

    if (flags & MFB_WF_BORDERLESS) {
        window_style = WS_POPUP;
    }

    if (flags & MFB_WF_RESIZABLE) {
        window_style |= WS_MAXIMIZEBOX | WS_SIZEBOX;
    }

    if (flags & MFB_WF_FULLSCREEN_DESKTOP) {
        window_style = WS_OVERLAPPEDWINDOW;
        show_window_cmd = SW_MAXIMIZE;

        width  = GetSystemMetrics(SM_CXFULLSCREEN);
        height = GetSystemMetrics(SM_CYFULLSCREEN);

        rect.right  = width;
        rect.bottom = height;
        AdjustWindowRect(&rect, window_style, 0);
        if (rect.left < 0) {
            width += rect.left * 2;
            rect.right += rect.left;
            rect.left = 0;
        }
        if (rect.bottom > (LONG) height) {
            LONG delta = rect.bottom - (LONG) height;
            height -= delta;
            rect.bottom -= delta;
            rect.top = 0;
        }
    }
    else if (!(flags & MFB_WF_FULLSCREEN)) {
        rect.right  = (LONG) width;
        rect.bottom = (LONG) height;

        // width/height already describe the requested client drawable size.
        // Only expand the outer frame for the monitor DPI; do not scale the
        // client area itself a second time here.
        if (mfb_AdjustWindowRectExForDpi != NULL) {
            float scale_x = 1.0f;
            UINT dpi = USER_DEFAULT_SCREEN_DPI;

            get_monitor_scale(0, &scale_x, NULL);
            if (scale_x > 0.0f) {
                dpi = (UINT) (scale_x * USER_DEFAULT_SCREEN_DPI + 0.5f);
                if (dpi == 0) {
                    dpi = USER_DEFAULT_SCREEN_DPI;
                }
            }

            if (!mfb_AdjustWindowRectExForDpi(&rect, window_style, FALSE, 0, dpi)) {
                AdjustWindowRect(&rect, window_style, 0);
            }
        }
        else {
            AdjustWindowRect(&rect, window_style, 0);
        }

        rect.right  -= rect.left;
        rect.bottom -= rect.top;

        x = (GetSystemMetrics(SM_CXSCREEN) - rect.right) / 2;
        y = (GetSystemMetrics(SM_CYSCREEN) - rect.bottom + rect.top) / 2;
    }

    // Disable Double Clicks Not adding CS_DBLCLKS
    window_data_specific->wc.style         = CS_OWNDC | CS_VREDRAW | CS_HREDRAW;
    window_data_specific->wc.lpfnWndProc   = WndProc;
    window_data_specific->wc.hCursor       = LoadCursor(0, IDC_ARROW);
    window_data_specific->wc.lpszClassName = "minifb";
    if (RegisterClass(&window_data_specific->wc) == 0) {
        uint32_t error = GetLastError();
        if (error != ERROR_CLASS_ALREADY_EXISTS) {
            MFB_LOG(MFB_LOG_ERROR, "RegisterClass failed: %s", get_error_message());
            free(window_data);
            free(window_data_specific);
            release_window_counter();
            return NULL;
        }
    }

    calc_dst_factor(window_data, width, height);

    window_data_specific->window = CreateWindowEx(
        0,
        "minifb", window_title,
        window_style,
        x, y,
        rect.right, rect.bottom,
        0, 0, 0, 0);

    if (!window_data_specific->window) {
        MFB_LOG(MFB_LOG_ERROR, "CreateWindowEx failed: %s", get_error_message());
        free(window_data);
        free(window_data_specific);
        release_window_counter();
        return NULL;
    }

    if (GetClientRect(window_data_specific->window, &rect)) {
        window_data->window_width  = (uint32_t) (rect.right - rect.left);
        window_data->window_height = (uint32_t) (rect.bottom - rect.top);
    }

    SetWindowLongPtr(window_data_specific->window, GWLP_USERDATA, (LONG_PTR) window_data);

    if (flags & MFB_WF_ALWAYS_ON_TOP)
        SetWindowPos(window_data_specific->window, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

    window_data_specific->hdc = GetDC(window_data_specific->window);
    if (window_data_specific->hdc == NULL) {
        MFB_LOG(MFB_LOG_ERROR, "GetDC failed: %s", get_error_message());
        DestroyWindow(window_data_specific->window);
        free(window_data);
        free(window_data_specific);
        release_window_counter();
        return NULL;
    }

#if !defined(USE_OPENGL_API)

    window_data_specific->bitmap_info = (BITMAPINFO *) calloc(1, sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD) * 3);
    if (window_data_specific->bitmap_info == NULL) {
        MFB_LOG(MFB_LOG_ERROR, "Cannot allocate BITMAPINFO");
        ReleaseDC(window_data_specific->window, window_data_specific->hdc);
        DestroyWindow(window_data_specific->window);
        free(window_data);
        free(window_data_specific);
        release_window_counter();
        return NULL;
    }

    window_data_specific->bitmap_info->bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    window_data_specific->bitmap_info->bmiHeader.biPlanes      = 1;
    window_data_specific->bitmap_info->bmiHeader.biBitCount    = 32;
    window_data_specific->bitmap_info->bmiHeader.biCompression = BI_BITFIELDS;
    window_data_specific->bitmap_info->bmiHeader.biWidth       = window_data->buffer_width;
    window_data_specific->bitmap_info->bmiHeader.biHeight      = -(LONG) window_data->buffer_height;
    window_data_specific->bitmap_info->bmiColors[0].rgbRed     = 0xff;
    window_data_specific->bitmap_info->bmiColors[1].rgbGreen   = 0xff;
    window_data_specific->bitmap_info->bmiColors[2].rgbBlue    = 0xff;

#else

    if (!create_GL_context(window_data)) {
        MFB_LOG(MFB_LOG_ERROR, "create_GL_context failed");
        ReleaseDC(window_data_specific->window, window_data_specific->hdc);
        DestroyWindow(window_data_specific->window);
        free(window_data);
        free(window_data_specific);
        release_window_counter();
        return NULL;
    }

#endif

    window_data_specific->timer = mfb_timer_create();
    if (window_data_specific->timer == NULL) {
        MFB_LOG(MFB_LOG_ERROR, "mfb_timer_create failed");
#if !defined(USE_OPENGL_API)
        free(window_data_specific->bitmap_info);
        window_data_specific->bitmap_info = NULL;
#else
        destroy_GL_context(window_data);
#endif
        ReleaseDC(window_data_specific->window, window_data_specific->hdc);
        DestroyWindow(window_data_specific->window);
        free(window_data);
        free(window_data_specific);
        release_window_counter();
        return NULL;
    }

    mfb_set_keyboard_callback((struct mfb_window *) window_data, keyboard_default);

    #if defined(USE_OPENGL_API)
        MFB_LOG(MFB_LOG_DEBUG, "Window created using OpenGL API");
    #else
        MFB_LOG(MFB_LOG_DEBUG, "Window created using GDI API");
    #endif

    ShowWindow(window_data_specific->window, show_window_cmd);

    window_data->is_initialized = true;
    return (struct mfb_window *) window_data;
}

//-------------------------------------
mfb_update_state
mfb_update_ex(struct mfb_window *window, void *buffer, unsigned width, unsigned height) {
    SWindowData *window_data = (SWindowData *) window;
    uint32_t buffer_stride = 0;
    if (window_data == NULL) {
        MFB_LOG(MFB_LOG_DEBUG, "mfb_update_ex: invalid window");
        return MFB_STATE_INVALID_WINDOW;
    }

    // Early exit
    if (window_data->close) {
        MFB_LOG(MFB_LOG_DEBUG, "mfb_update_ex: window requested close");
        destroy_window_data(window_data);
        return MFB_STATE_EXIT;
    }

    if (buffer == NULL) {
        MFB_LOG(MFB_LOG_DEBUG, "mfb_update_ex: invalid buffer");
        return MFB_STATE_INVALID_BUFFER;
    }

    if (!calculate_buffer_layout(width, height, &buffer_stride, NULL)) {
        MFB_LOG(MFB_LOG_DEBUG, "mfb_update_ex: invalid buffer size %ux%u", width, height);
        return MFB_STATE_INVALID_BUFFER;
    }

    SWindowData_Win *window_data_specific = (SWindowData_Win *) window_data->specific;
    if (window_data_specific ==  NULL) {
        MFB_LOG(MFB_LOG_DEBUG, "mfb_update_ex: invalid window specific data");
        return MFB_STATE_INVALID_WINDOW;
    }

    window_data->draw_buffer   = buffer;
    window_data->buffer_width  = width;
    window_data->buffer_stride = buffer_stride;
    window_data->buffer_height = height;

#if !defined(USE_OPENGL_API)

    window_data_specific->bitmap_info->bmiHeader.biWidth = window_data->buffer_width;
    window_data_specific->bitmap_info->bmiHeader.biHeight = -(LONG) window_data->buffer_height;
    InvalidateRect(window_data_specific->window, NULL, TRUE);
    SendMessage(window_data_specific->window, WM_PAINT, 0, 0);

#else

    redraw_GL(window_data, buffer);

#endif

    update_events(window_data, window_data_specific->window);
    if (window_data->close) {
        MFB_LOG(MFB_LOG_DEBUG, "mfb_update_ex: window closed after event processing");
        destroy_window_data(window_data);
        return MFB_STATE_EXIT;
    }

    return MFB_STATE_OK;
}

//-------------------------------------
mfb_update_state
mfb_update_events(struct mfb_window *window) {
    SWindowData *window_data = (SWindowData *) window;
    if (window_data == NULL) {
        MFB_LOG(MFB_LOG_DEBUG, "mfb_update_events: invalid window");
        return MFB_STATE_INVALID_WINDOW;
    }

    // Early exit
    if (window_data->close) {
        MFB_LOG(MFB_LOG_DEBUG, "mfb_update_events: window requested close");
        destroy_window_data(window_data);
        return MFB_STATE_EXIT;
    }

    SWindowData_Win *window_data_specific = (SWindowData_Win *) window_data->specific;
    if (window_data_specific == NULL) {
        MFB_LOG(MFB_LOG_DEBUG, "mfb_update_events: invalid window specific data");
        return MFB_STATE_INVALID_WINDOW;
    }

    update_events(window_data, window_data_specific->window);
    if (window_data->close) {
        MFB_LOG(MFB_LOG_DEBUG, "mfb_update_events: window closed after event processing");
        destroy_window_data(window_data);
        return MFB_STATE_EXIT;
    }

    return MFB_STATE_OK;
}

//-------------------------------------
extern double   g_time_for_frame;
extern bool     g_use_hardware_sync;

//-------------------------------------
bool
mfb_wait_sync(struct mfb_window *window) {
    SWindowData *window_data = (SWindowData *) window;
    if (window_data == NULL) {
        MFB_LOG(MFB_LOG_DEBUG, "mfb_wait_sync: invalid window");
        return false;
    }
    if (window_data->close) {
        MFB_LOG(MFB_LOG_DEBUG, "mfb_wait_sync: window requested close");
        destroy_window_data(window_data);
        return false;
    }

    SWindowData_Win *window_data_specific = (SWindowData_Win *) window_data->specific;
    if (window_data_specific == NULL) {
        MFB_LOG(MFB_LOG_DEBUG, "mfb_wait_sync: invalid window specific data");
        return false;
    }

    update_events(window_data, NULL);
    if (window_data->close) {
        MFB_LOG(MFB_LOG_DEBUG, "mfb_wait_sync: window closed while polling events");
        destroy_window_data(window_data);
        return false;
    }

    // Hardware sync: no software pacing
    if (g_use_hardware_sync) {
        return true;
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

            MsgWaitForMultipleObjectsEx(
                0,
                NULL,
                (DWORD) timeout_ms,
                QS_ALLINPUT,
                MWMO_INPUTAVAILABLE | MWMO_ALERTABLE
            );
        }
        else {
            SwitchToThread();
        }

        update_events(window_data, NULL);
        if (window_data->close) {
            MFB_LOG(MFB_LOG_DEBUG, "mfb_wait_sync: window closed while waiting for frame sync");
            destroy_window_data(window_data);
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

    g_keycodes[0x00B] = MFB_KB_KEY_0;
    g_keycodes[0x002] = MFB_KB_KEY_1;
    g_keycodes[0x003] = MFB_KB_KEY_2;
    g_keycodes[0x004] = MFB_KB_KEY_3;
    g_keycodes[0x005] = MFB_KB_KEY_4;
    g_keycodes[0x006] = MFB_KB_KEY_5;
    g_keycodes[0x007] = MFB_KB_KEY_6;
    g_keycodes[0x008] = MFB_KB_KEY_7;
    g_keycodes[0x009] = MFB_KB_KEY_8;
    g_keycodes[0x00A] = MFB_KB_KEY_9;
    g_keycodes[0x01E] = MFB_KB_KEY_A;
    g_keycodes[0x030] = MFB_KB_KEY_B;
    g_keycodes[0x02E] = MFB_KB_KEY_C;
    g_keycodes[0x020] = MFB_KB_KEY_D;
    g_keycodes[0x012] = MFB_KB_KEY_E;
    g_keycodes[0x021] = MFB_KB_KEY_F;
    g_keycodes[0x022] = MFB_KB_KEY_G;
    g_keycodes[0x023] = MFB_KB_KEY_H;
    g_keycodes[0x017] = MFB_KB_KEY_I;
    g_keycodes[0x024] = MFB_KB_KEY_J;
    g_keycodes[0x025] = MFB_KB_KEY_K;
    g_keycodes[0x026] = MFB_KB_KEY_L;
    g_keycodes[0x032] = MFB_KB_KEY_M;
    g_keycodes[0x031] = MFB_KB_KEY_N;
    g_keycodes[0x018] = MFB_KB_KEY_O;
    g_keycodes[0x019] = MFB_KB_KEY_P;
    g_keycodes[0x010] = MFB_KB_KEY_Q;
    g_keycodes[0x013] = MFB_KB_KEY_R;
    g_keycodes[0x01F] = MFB_KB_KEY_S;
    g_keycodes[0x014] = MFB_KB_KEY_T;
    g_keycodes[0x016] = MFB_KB_KEY_U;
    g_keycodes[0x02F] = MFB_KB_KEY_V;
    g_keycodes[0x011] = MFB_KB_KEY_W;
    g_keycodes[0x02D] = MFB_KB_KEY_X;
    g_keycodes[0x015] = MFB_KB_KEY_Y;
    g_keycodes[0x02C] = MFB_KB_KEY_Z;

    g_keycodes[0x028] = MFB_KB_KEY_APOSTROPHE;
    g_keycodes[0x02B] = MFB_KB_KEY_BACKSLASH;
    g_keycodes[0x033] = MFB_KB_KEY_COMMA;
    g_keycodes[0x00D] = MFB_KB_KEY_EQUAL;
    g_keycodes[0x029] = MFB_KB_KEY_GRAVE_ACCENT;
    g_keycodes[0x01A] = MFB_KB_KEY_LEFT_BRACKET;
    g_keycodes[0x00C] = MFB_KB_KEY_MINUS;
    g_keycodes[0x034] = MFB_KB_KEY_PERIOD;
    g_keycodes[0x01B] = MFB_KB_KEY_RIGHT_BRACKET;
    g_keycodes[0x027] = MFB_KB_KEY_SEMICOLON;
    g_keycodes[0x035] = MFB_KB_KEY_SLASH;
    g_keycodes[0x056] = MFB_KB_KEY_WORLD_2;

    g_keycodes[0x00E] = MFB_KB_KEY_BACKSPACE;
    g_keycodes[0x153] = MFB_KB_KEY_DELETE;
    g_keycodes[0x14F] = MFB_KB_KEY_END;
    g_keycodes[0x01C] = MFB_KB_KEY_ENTER;
    g_keycodes[0x001] = MFB_KB_KEY_ESCAPE;
    g_keycodes[0x147] = MFB_KB_KEY_HOME;
    g_keycodes[0x152] = MFB_KB_KEY_INSERT;
    g_keycodes[0x15D] = MFB_KB_KEY_MENU;
    g_keycodes[0x151] = MFB_KB_KEY_PAGE_DOWN;
    g_keycodes[0x149] = MFB_KB_KEY_PAGE_UP;
    g_keycodes[0x045] = MFB_KB_KEY_PAUSE;
    g_keycodes[0x146] = MFB_KB_KEY_PAUSE;
    g_keycodes[0x039] = MFB_KB_KEY_SPACE;
    g_keycodes[0x00F] = MFB_KB_KEY_TAB;
    g_keycodes[0x03A] = MFB_KB_KEY_CAPS_LOCK;
    g_keycodes[0x145] = MFB_KB_KEY_NUM_LOCK;
    g_keycodes[0x046] = MFB_KB_KEY_SCROLL_LOCK;
    g_keycodes[0x03B] = MFB_KB_KEY_F1;
    g_keycodes[0x03C] = MFB_KB_KEY_F2;
    g_keycodes[0x03D] = MFB_KB_KEY_F3;
    g_keycodes[0x03E] = MFB_KB_KEY_F4;
    g_keycodes[0x03F] = MFB_KB_KEY_F5;
    g_keycodes[0x040] = MFB_KB_KEY_F6;
    g_keycodes[0x041] = MFB_KB_KEY_F7;
    g_keycodes[0x042] = MFB_KB_KEY_F8;
    g_keycodes[0x043] = MFB_KB_KEY_F9;
    g_keycodes[0x044] = MFB_KB_KEY_F10;
    g_keycodes[0x057] = MFB_KB_KEY_F11;
    g_keycodes[0x058] = MFB_KB_KEY_F12;
    g_keycodes[0x064] = MFB_KB_KEY_F13;
    g_keycodes[0x065] = MFB_KB_KEY_F14;
    g_keycodes[0x066] = MFB_KB_KEY_F15;
    g_keycodes[0x067] = MFB_KB_KEY_F16;
    g_keycodes[0x068] = MFB_KB_KEY_F17;
    g_keycodes[0x069] = MFB_KB_KEY_F18;
    g_keycodes[0x06A] = MFB_KB_KEY_F19;
    g_keycodes[0x06B] = MFB_KB_KEY_F20;
    g_keycodes[0x06C] = MFB_KB_KEY_F21;
    g_keycodes[0x06D] = MFB_KB_KEY_F22;
    g_keycodes[0x06E] = MFB_KB_KEY_F23;
    g_keycodes[0x076] = MFB_KB_KEY_F24;
    g_keycodes[0x038] = MFB_KB_KEY_LEFT_ALT;
    g_keycodes[0x01D] = MFB_KB_KEY_LEFT_CONTROL;
    g_keycodes[0x02A] = MFB_KB_KEY_LEFT_SHIFT;
    g_keycodes[0x15B] = MFB_KB_KEY_LEFT_SUPER;
    g_keycodes[0x137] = MFB_KB_KEY_PRINT_SCREEN;
    g_keycodes[0x138] = MFB_KB_KEY_RIGHT_ALT;
    g_keycodes[0x11D] = MFB_KB_KEY_RIGHT_CONTROL;
    g_keycodes[0x036] = MFB_KB_KEY_RIGHT_SHIFT;
    g_keycodes[0x15C] = MFB_KB_KEY_RIGHT_SUPER;
    g_keycodes[0x150] = MFB_KB_KEY_DOWN;
    g_keycodes[0x14B] = MFB_KB_KEY_LEFT;
    g_keycodes[0x14D] = MFB_KB_KEY_RIGHT;
    g_keycodes[0x148] = MFB_KB_KEY_UP;

    g_keycodes[0x052] = MFB_KB_KEY_KP_0;
    g_keycodes[0x04F] = MFB_KB_KEY_KP_1;
    g_keycodes[0x050] = MFB_KB_KEY_KP_2;
    g_keycodes[0x051] = MFB_KB_KEY_KP_3;
    g_keycodes[0x04B] = MFB_KB_KEY_KP_4;
    g_keycodes[0x04C] = MFB_KB_KEY_KP_5;
    g_keycodes[0x04D] = MFB_KB_KEY_KP_6;
    g_keycodes[0x047] = MFB_KB_KEY_KP_7;
    g_keycodes[0x048] = MFB_KB_KEY_KP_8;
    g_keycodes[0x049] = MFB_KB_KEY_KP_9;
    g_keycodes[0x04E] = MFB_KB_KEY_KP_ADD;
    g_keycodes[0x053] = MFB_KB_KEY_KP_DECIMAL;
    g_keycodes[0x135] = MFB_KB_KEY_KP_DIVIDE;
    g_keycodes[0x11C] = MFB_KB_KEY_KP_ENTER;
    g_keycodes[0x059] = MFB_KB_KEY_KP_EQUAL;
    g_keycodes[0x037] = MFB_KB_KEY_KP_MULTIPLY;
    g_keycodes[0x04A] = MFB_KB_KEY_KP_SUBTRACT;
}

//-------------------------------------
mfb_key
translate_key(unsigned int wParam, unsigned long lParam) {
    if (wParam == 0x92) // VK_OEM_NEC_EQUAL
        return MFB_KB_KEY_KP_EQUAL;

    if (wParam == VK_CONTROL) {
        MSG   next;
        DWORD time;

        if (lParam & 0x01000000)
            return MFB_KB_KEY_RIGHT_CONTROL;

        time = GetMessageTime();
        if (PeekMessageW(&next, NULL, 0, 0, PM_NOREMOVE))
            if (next.message == WM_KEYDOWN || next.message == WM_SYSKEYDOWN || next.message == WM_KEYUP || next.message == WM_SYSKEYUP)
                if (next.wParam == VK_MENU && (next.lParam & 0x01000000) && next.time == time)
                    return MFB_KB_KEY_UNKNOWN;

        return MFB_KB_KEY_LEFT_CONTROL;
    }

    if (wParam == VK_PROCESSKEY)
        return MFB_KB_KEY_UNKNOWN;

    return (mfb_key) g_keycodes[HIWORD(lParam) & 0x1FF];
}

//-------------------------------------
uint32_t
translate_mod() {
    uint32_t mods = 0;

    if (GetKeyState(VK_SHIFT) & 0x8000)
        mods |= MFB_KB_MOD_SHIFT;
    if (GetKeyState(VK_CONTROL) & 0x8000)
        mods |= MFB_KB_MOD_CONTROL;
    if (GetKeyState(VK_MENU) & 0x8000)
        mods |= MFB_KB_MOD_ALT;
    if ((GetKeyState(VK_LWIN) | GetKeyState(VK_RWIN)) & 0x8000)
        mods |= MFB_KB_MOD_SUPER;
    if (GetKeyState(VK_CAPITAL) & 1)
        mods |= MFB_KB_MOD_CAPS_LOCK;
    if (GetKeyState(VK_NUMLOCK) & 1)
        mods |= MFB_KB_MOD_NUM_LOCK;

    return mods;
}

//-------------------------------------
bool
mfb_set_viewport(struct mfb_window *window, unsigned offset_x, unsigned offset_y, unsigned width, unsigned height) {
    SWindowData     *window_data     = (SWindowData *) window;
    SWindowData_Win *window_data_specific = NULL;

    if (!mfb_validate_viewport(window_data, offset_x, offset_y, width, height, "WinMiniFB")) {
        return false;
    }

    window_data_specific = (SWindowData_Win *) window_data->specific;
    if (window_data_specific == NULL) {
        MFB_LOG(MFB_LOG_ERROR, "WinMiniFB: mfb_set_viewport missing Windows-specific window data.");
        return false;
    }

    window_data->dst_offset_x = offset_x;
    window_data->dst_offset_y = offset_y;
    window_data->dst_width    = width;
    window_data->dst_height   = height;

    calc_dst_factor(window_data, window_data->window_width, window_data->window_height);

#if !defined(USE_OPENGL_API)
    window_data_specific = (SWindowData_Win *) window_data->specific;
    BitBlt(window_data_specific->hdc, 0, 0, window_data->window_width, window_data->window_height, 0, 0, 0, BLACKNESS);
#endif

    return true;
}

//-------------------------------------
void
mfb_set_title(struct mfb_window *window, const char *title) {
    if (window == 0x0 || title == 0x0) {
        return;
    }

    SWindowData *window_data = (SWindowData *) window;
    SWindowData_Win *window_data_specific = (SWindowData_Win *) window_data->specific;
    if (window_data_specific == 0x0) {
        return;
    }

    SetWindowTextA(window_data_specific->window, title);
}

//-------------------------------------
void
mfb_get_monitor_scale(struct mfb_window *window, float *scale_x, float *scale_y) {
    float x = 1.0f;
    float y = 1.0f;
    HWND hWnd = NULL;

    SWindowData *window_data = (SWindowData *) window;
    if (window_data != NULL) {
        SWindowData_Win *window_data_specific = (SWindowData_Win *) window_data->specific;
        if (window_data_specific != NULL) {
            hWnd = window_data_specific->window;
        }
    }
    get_monitor_scale(hWnd, &x, &y);

    if (scale_x) {
        *scale_x = x;
    }
    if (scale_y) {
        *scale_y = y;
    }
}

//-------------------------------------
extern double   g_timer_frequency;
extern double   g_timer_resolution;

//-------------------------------------
void
mfb_timer_init() {
    uint64_t    frequency;

    QueryPerformanceFrequency((LARGE_INTEGER *) &frequency);

    g_timer_frequency  = (double) ((int64_t) frequency);
    g_timer_resolution = 1.0 / g_timer_frequency;
}

//-------------------------------------
uint64_t
mfb_timer_tick() {
    int64_t     counter;

    QueryPerformanceCounter((LARGE_INTEGER *) &counter);

    return counter;
}

//-------------------------------------
void
mfb_show_cursor(struct mfb_window *window, bool show) {
    SWindowData *window_data = (SWindowData *) window;
    if (window_data == NULL)
        return;

    SWindowData_Win *window_data_specific = (SWindowData_Win *) window_data->specific;
    if (window_data_specific == NULL)
        return;

    if ((window_data_specific->mouse_inside) && (window_data->is_cursor_visible != show)) {
        ShowCursor((BOOL) show);
    }

    window_data->is_cursor_visible = show;
}
