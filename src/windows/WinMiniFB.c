#include <MiniFB.h>
#include <MiniFB_internal.h>
#include <WindowData.h>
#include "WindowData_Win.h"

#include <stdlib.h>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

long    s_window_style = WS_POPUP | WS_SYSMENU | WS_CAPTION;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t translate_mod();
mfb_key  translate_key(unsigned int wParam, unsigned long lParam);
void     destroy_window_data(SWindowData *window_data);

LRESULT CALLBACK 
WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    LRESULT res = 0;

    SWindowData     *window_data     = (SWindowData *) GetWindowLongPtr(hWnd, GWLP_USERDATA);
    SWindowData_Win *window_data_win = 0x0;
    if (window_data != 0x0) {
        window_data_win = (SWindowData_Win *) window_data->specific;
    }

    switch (message)
    {
        case WM_PAINT:
        {
            if (window_data && window_data->draw_buffer && window_data_win)
            {
                //if (window_data->dst_offset_x > 0) {
                //    BitBlt(window_data_win->hdc, 0, window_data->dst_offset_y, window_data->dst_offset_x, window_data->dst_height, 0, 0, 0, BLACKNESS);
                //}
                //if (window_data->dst_offset_y > 0) {
                //    BitBlt(window_data_win->hdc, 0, 0, window_data->window_width, window_data->dst_offset_y, 0, 0, 0, BLACKNESS);
                //}
                //uint32_t offsetY = window_data->dst_offset_y + window_data->dst_height;
                //if (offsetY < window_data->window_height) {
                //    BitBlt(window_data_win->hdc, 0, offsetY, window_data->window_width, window_data->window_height - offsetY, 0, 0, 0, BLACKNESS);
                //}
                //uint32_t offsetX = window_data->dst_offset_x + window_data->dst_width;
                //if (offsetX < window_data->window_width) {
                //    BitBlt(window_data_win->hdc, offsetX, window_data->dst_offset_y, window_data->window_width - offsetX, window_data->dst_height, 0, 0, 0, BLACKNESS);
                //}

                StretchDIBits(window_data_win->hdc, window_data->dst_offset_x, window_data->dst_offset_y, window_data->dst_width, window_data->dst_height, 0, 0, window_data->buffer_width, window_data->buffer_height, window_data->draw_buffer, 
                              window_data_win->bitmapInfo, DIB_RGB_COLORS, SRCCOPY);
            }
            ValidateRect(hWnd, 0x0);
            break;
        }

        case WM_DESTROY:
        case WM_CLOSE:
        {
            if (window_data) {
                window_data->close = true;
            }
            break;
        }

        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYUP:
        {
            if (window_data) {
                mfb_key key_code      = translate_key((unsigned int)wParam, (unsigned long)lParam);
                int is_pressed        = !((lParam >> 31) & 1);
                window_data->mod_keys = translate_mod();

                if (key_code == KB_KEY_UNKNOWN)
                    return FALSE;

                window_data->key_status[key_code] = (uint8_t) is_pressed;
                kCall(keyboard_func, key_code, window_data->mod_keys, is_pressed);
            }
            break;
        }

        case WM_CHAR:
        case WM_SYSCHAR:
        case WM_UNICHAR: 
        {

            if (window_data) {
                if (message == WM_UNICHAR && wParam == UNICODE_NOCHAR) {
                    // WM_UNICHAR is not sent by Windows, but is sent by some third-party input method engine
                    // Returning TRUE here announces support for this message
                    return TRUE;
                }

                kCall(char_input_func, (unsigned int) wParam);
            }
            break;
        }

        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MBUTTONUP:
        case WM_XBUTTONUP:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONDBLCLK:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONDBLCLK:
        case WM_XBUTTONDOWN:
        case WM_XBUTTONDBLCLK:
        {
            if (window_data) {
                mfb_mouse_button button = MOUSE_BTN_0;
                window_data->mod_keys   = translate_mod();
                int          is_pressed = 0;
                switch(message) {
                case WM_LBUTTONDOWN:
                    is_pressed = 1;
                case WM_LBUTTONUP:
                    button = MOUSE_BTN_1;
                    break;
                case WM_RBUTTONDOWN:
                    is_pressed = 1;
                case WM_RBUTTONUP:
                    button = MOUSE_BTN_2;
                    break;
                case WM_MBUTTONDOWN:
                    is_pressed = 1;
                case WM_MBUTTONUP:
                    button = MOUSE_BTN_3;
                    break;

                default:
                    button = (GET_XBUTTON_WPARAM(wParam) == XBUTTON1 ? MOUSE_BTN_5 : MOUSE_BTN_6);
                    if (message == WM_XBUTTONDOWN) {
                        is_pressed = 1;
                    }
                }
                kCall(mouse_btn_func, button, window_data->mod_keys, is_pressed);
            }
            break;
        }

        case WM_MOUSEWHEEL:
            if (window_data) {
                kCall(mouse_wheel_func, translate_mod(), 0.0f, (SHORT)HIWORD(wParam) / (float)WHEEL_DELTA);
            }
            break;

        case WM_MOUSEHWHEEL:
            // This message is only sent on Windows Vista and later
            // NOTE: The X-axis is inverted for consistency with macOS and X11
            if (window_data) {
                kCall(mouse_wheel_func, translate_mod(), -((SHORT)HIWORD(wParam) / (float)WHEEL_DELTA), 0.0f);
            }
            break;

        case WM_MOUSEMOVE:
            if (window_data) {
                if (window_data_win->mouse_inside == false) {
                    window_data_win->mouse_inside = true;
                    TRACKMOUSEEVENT tme;
                    ZeroMemory(&tme, sizeof(tme));
                    tme.cbSize = sizeof(tme);
                    tme.dwFlags = TME_LEAVE;
                    tme.hwndTrack = hWnd;
                    TrackMouseEvent(&tme);
                }
                window_data->mouse_pos_x = (int)(short) LOWORD(lParam);
                window_data->mouse_pos_y = (int)(short) HIWORD(lParam);
                kCall(mouse_move_func, window_data->mouse_pos_x, window_data->mouse_pos_y);
            }
            break;

        case WM_MOUSELEAVE:
            if (window_data) {
                window_data_win->mouse_inside = false;
            }
            break;

        case WM_SIZE:
            if (window_data) {
                window_data->dst_offset_x = 0;
                window_data->dst_offset_y = 0;
                window_data->dst_width = LOWORD(lParam);
                window_data->dst_height = HIWORD(lParam);
                window_data->window_width = window_data->dst_width;
                window_data->window_height = window_data->dst_height;
                BitBlt(window_data_win->hdc, 0, 0, window_data->window_width, window_data->window_height, 0, 0, 0, BLACKNESS);
                kCall(resize_func, window_data->dst_width, window_data->dst_height);
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
        {
            res = DefWindowProc(hWnd, message, wParam, lParam);
        }
    }

    return res;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct mfb_window *
mfb_open_ex(const char *title, unsigned width, unsigned height, unsigned flags) {
    RECT rect = { 0 };
    int  x = 0, y = 0;

    init_keycodes();

    SWindowData *window_data = malloc(sizeof(SWindowData));
    if (window_data == 0x0) {
        return 0x0;
    }
    memset(window_data, 0, sizeof(SWindowData));

    SWindowData_Win *window_data_win = malloc(sizeof(SWindowData_Win));
    if(window_data_win == 0x0) {
        free(window_data);
        return 0x0;
    }
    memset(window_data_win, 0, sizeof(SWindowData_Win));
    window_data->specific = window_data_win;

    window_data->buffer_width  = width;
    window_data->buffer_height = height;
    window_data->buffer_stride = width * 4;

    s_window_style = WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME;
    if (flags & WF_FULLSCREEN) {
        flags = WF_FULLSCREEN;  // Remove all other flags
        rect.right  = GetSystemMetrics(SM_CXSCREEN);
        rect.bottom = GetSystemMetrics(SM_CYSCREEN);
        s_window_style = WS_POPUP & ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZE | WS_MAXIMIZE | WS_SYSMENU);

        DEVMODE settings = { 0 };
        EnumDisplaySettings(0, 0, &settings);
        settings.dmPelsWidth  = GetSystemMetrics(SM_CXSCREEN);
        settings.dmPelsHeight = GetSystemMetrics(SM_CYSCREEN);
        settings.dmBitsPerPel = 32;
        settings.dmFields     = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;

        if (ChangeDisplaySettings(&settings, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL) {
            flags = WF_FULLSCREEN_DESKTOP;
        }
    }

    if (flags & WF_BORDERLESS) {
        s_window_style = WS_POPUP;
    }

    if (flags & WF_RESIZABLE) {
        s_window_style |= WS_MAXIMIZEBOX | WS_SIZEBOX;
    }

    if (flags & WF_FULLSCREEN_DESKTOP) {
        s_window_style = WS_OVERLAPPEDWINDOW;

        width  = GetSystemMetrics(SM_CXFULLSCREEN);
        height = GetSystemMetrics(SM_CYFULLSCREEN);

        rect.right  = width;
        rect.bottom = height;
        AdjustWindowRect(&rect, s_window_style, 0);
        if (rect.left < 0) {
            width += rect.left * 2;
            rect.right += rect.left;
            rect.left = 0;
        }
        if (rect.bottom > (LONG) height) {
            height -= (rect.bottom - height);
            rect.bottom += (rect.bottom - height);
            rect.top = 0;
        }
    }
    else if (!(flags & WF_FULLSCREEN)) {
        rect.right  = width;
        rect.bottom = height;

        AdjustWindowRect(&rect, s_window_style, 0);

        rect.right  -= rect.left;
        rect.bottom -= rect.top;

        x = (GetSystemMetrics(SM_CXSCREEN) - rect.right) / 2;
        y = (GetSystemMetrics(SM_CYSCREEN) - rect.bottom + rect.top) / 2;
    }

    window_data_win->wc.style         = CS_OWNDC | CS_VREDRAW | CS_HREDRAW;
    window_data_win->wc.lpfnWndProc   = WndProc;
    window_data_win->wc.hCursor       = LoadCursor(0, IDC_ARROW);
    window_data_win->wc.lpszClassName = title;
    RegisterClass(&window_data_win->wc);

    if (window_data->dst_width == 0)
        window_data->dst_width = width;

    if (window_data->dst_height == 0)
        window_data->dst_height = height;

    window_data->window_width  = rect.right;
    window_data->window_height = rect.bottom;

    window_data_win->window = CreateWindowEx(
        0,
        title, title,
        s_window_style,
        x, y,
        window_data->window_width, window_data->window_height,
        0, 0, 0, 0);

    if (!window_data_win->window) {
        free(window_data);
        free(window_data_win);
        return 0x0;
    }

    SetWindowLongPtr(window_data_win->window, GWLP_USERDATA, (LONG_PTR) window_data);

    if (flags & WF_ALWAYS_ON_TOP)
        SetWindowPos(window_data_win->window, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

    ShowWindow(window_data_win->window, SW_NORMAL);

    window_data_win->bitmapInfo = (BITMAPINFO *) calloc(1, sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD) * 3);
    if(window_data_win->bitmapInfo == 0x0) {
        free(window_data);
        free(window_data_win);
        return 0x0;
    }
    window_data_win->bitmapInfo->bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    window_data_win->bitmapInfo->bmiHeader.biPlanes      = 1;
    window_data_win->bitmapInfo->bmiHeader.biBitCount    = 32;
    window_data_win->bitmapInfo->bmiHeader.biCompression = BI_BITFIELDS;
    window_data_win->bitmapInfo->bmiHeader.biWidth       = window_data->buffer_width;
    window_data_win->bitmapInfo->bmiHeader.biHeight      = -(LONG)window_data->buffer_height;
    window_data_win->bitmapInfo->bmiColors[0].rgbRed     = 0xff;
    window_data_win->bitmapInfo->bmiColors[1].rgbGreen   = 0xff;
    window_data_win->bitmapInfo->bmiColors[2].rgbBlue    = 0xff;

    window_data_win->hdc = GetDC(window_data_win->window);

    window_data_win->timer = mfb_timer_create();

    mfb_set_keyboard_callback((struct mfb_window *) window_data, keyboard_default);

    return (struct mfb_window *) window_data;
}

struct mfb_window *
mfb_open(const char *title, unsigned width, unsigned height) {
    return mfb_open_ex(title, width, height, 0);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

mfb_update_state 
mfb_update(struct mfb_window *window, void *buffer) {
    MSG msg;
    
    if (window == 0x0) {
        return STATE_INVALID_WINDOW;
    }

    SWindowData *window_data = (SWindowData *)window;
    if (window_data->close) {
        destroy_window_data(window_data);
        return STATE_EXIT;
    }

    if (buffer == 0x0) {
        return STATE_INVALID_BUFFER;
    }

    window_data->draw_buffer = buffer;

    SWindowData_Win *window_data_win = (SWindowData_Win *) window_data->specific;
    InvalidateRect(window_data_win->window, 0x0, TRUE);
    SendMessage(window_data_win->window, WM_PAINT, 0, 0);

    while (window_data->close == false && PeekMessage(&msg, window_data_win->window, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return STATE_OK;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

mfb_update_state 
mfb_update_events(struct mfb_window *window) {
    MSG msg;
    
    if (window == 0x0) {
        return STATE_INVALID_WINDOW;
    }

    SWindowData *window_data = (SWindowData *)window;
    if (window_data->close) {
        destroy_window_data(window_data);
        return STATE_EXIT;
    }

    SWindowData_Win *window_data_win = (SWindowData_Win *) window_data->specific;
    while (window_data->close == false && PeekMessage(&msg, window_data_win->window, 0, 0, PM_REMOVE)) {
        //if(msg.message == WM_PAINT)
        //    return STATE_OK;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return STATE_OK;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extern double   g_time_for_frame;

bool
mfb_wait_sync(struct mfb_window *window) {
    MSG msg;
    
    if (window == 0x0) {
        return false;
    }

    SWindowData *window_data = (SWindowData *)window;
    if (window_data->close) {
        destroy_window_data(window_data);
        return false;
    }

    SWindowData_Win *window_data_win = (SWindowData_Win *) window_data->specific;
    double      current;
    uint32_t    millis = 1;
    while (1) {
        if(PeekMessage(&msg, window_data_win->window, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        if (window_data->close) {
            destroy_window_data(window_data);
            return false;
        }

        current = mfb_timer_now(window_data_win->timer);;
        if (current >= g_time_for_frame) {
            mfb_timer_reset(window_data_win->timer);
            return true;
        }
        else if(current >= g_time_for_frame * 0.8) {
            millis = 0;
        }

        Sleep(millis);
    }

    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void 
destroy_window_data(SWindowData *window_data) {
    if (window_data == 0x0)
        return;

    SWindowData_Win *window_data_win = (SWindowData_Win *) window_data->specific;

    window_data->draw_buffer = 0x0;
    if (window_data_win->bitmapInfo != 0x0) {
        free(window_data_win->bitmapInfo);
    }

    if (window_data_win->window != 0 && window_data_win->hdc != 0) {
        ReleaseDC(window_data_win->window, window_data_win->hdc);
        DestroyWindow(window_data_win->window);
    }

    window_data_win->window = 0;
    window_data_win->hdc = 0;
    window_data_win->bitmapInfo = 0x0;
    mfb_timer_destroy(window_data_win->timer);
    window_data_win->timer = 0x0;
    window_data->close = true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t 
translate_mod() {
    uint32_t mods = 0;

    if (GetKeyState(VK_SHIFT) & 0x8000)
        mods |= KB_MOD_SHIFT;
    if (GetKeyState(VK_CONTROL) & 0x8000)
        mods |= KB_MOD_CONTROL;
    if (GetKeyState(VK_MENU) & 0x8000)
        mods |= KB_MOD_ALT;
    if ((GetKeyState(VK_LWIN) | GetKeyState(VK_RWIN)) & 0x8000)
        mods |= KB_MOD_SUPER;
    if (GetKeyState(VK_CAPITAL) & 1)
        mods |= KB_MOD_CAPS_LOCK;
    if (GetKeyState(VK_NUMLOCK) & 1)
        mods |= KB_MOD_NUM_LOCK;

    return mods;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extern short int g_keycodes[512];

void 
init_keycodes() {

    // Clear keys
    for (size_t i = 0; i < sizeof(g_keycodes) / sizeof(g_keycodes[0]); ++i) 
        g_keycodes[i] = 0;

    g_keycodes[0x00B] = KB_KEY_0;
    g_keycodes[0x002] = KB_KEY_1;
    g_keycodes[0x003] = KB_KEY_2;
    g_keycodes[0x004] = KB_KEY_3;
    g_keycodes[0x005] = KB_KEY_4;
    g_keycodes[0x006] = KB_KEY_5;
    g_keycodes[0x007] = KB_KEY_6;
    g_keycodes[0x008] = KB_KEY_7;
    g_keycodes[0x009] = KB_KEY_8;
    g_keycodes[0x00A] = KB_KEY_9;
    g_keycodes[0x01E] = KB_KEY_A;
    g_keycodes[0x030] = KB_KEY_B;
    g_keycodes[0x02E] = KB_KEY_C;
    g_keycodes[0x020] = KB_KEY_D;
    g_keycodes[0x012] = KB_KEY_E;
    g_keycodes[0x021] = KB_KEY_F;
    g_keycodes[0x022] = KB_KEY_G;
    g_keycodes[0x023] = KB_KEY_H;
    g_keycodes[0x017] = KB_KEY_I;
    g_keycodes[0x024] = KB_KEY_J;
    g_keycodes[0x025] = KB_KEY_K;
    g_keycodes[0x026] = KB_KEY_L;
    g_keycodes[0x032] = KB_KEY_M;
    g_keycodes[0x031] = KB_KEY_N;
    g_keycodes[0x018] = KB_KEY_O;
    g_keycodes[0x019] = KB_KEY_P;
    g_keycodes[0x010] = KB_KEY_Q;
    g_keycodes[0x013] = KB_KEY_R;
    g_keycodes[0x01F] = KB_KEY_S;
    g_keycodes[0x014] = KB_KEY_T;
    g_keycodes[0x016] = KB_KEY_U;
    g_keycodes[0x02F] = KB_KEY_V;
    g_keycodes[0x011] = KB_KEY_W;
    g_keycodes[0x02D] = KB_KEY_X;
    g_keycodes[0x015] = KB_KEY_Y;
    g_keycodes[0x02C] = KB_KEY_Z;

    g_keycodes[0x028] = KB_KEY_APOSTROPHE;
    g_keycodes[0x02B] = KB_KEY_BACKSLASH;
    g_keycodes[0x033] = KB_KEY_COMMA;
    g_keycodes[0x00D] = KB_KEY_EQUAL;
    g_keycodes[0x029] = KB_KEY_GRAVE_ACCENT;
    g_keycodes[0x01A] = KB_KEY_LEFT_BRACKET;
    g_keycodes[0x00C] = KB_KEY_MINUS;
    g_keycodes[0x034] = KB_KEY_PERIOD;
    g_keycodes[0x01B] = KB_KEY_RIGHT_BRACKET;
    g_keycodes[0x027] = KB_KEY_SEMICOLON;
    g_keycodes[0x035] = KB_KEY_SLASH;
    g_keycodes[0x056] = KB_KEY_WORLD_2;

    g_keycodes[0x00E] = KB_KEY_BACKSPACE;
    g_keycodes[0x153] = KB_KEY_DELETE;
    g_keycodes[0x14F] = KB_KEY_END;
    g_keycodes[0x01C] = KB_KEY_ENTER;
    g_keycodes[0x001] = KB_KEY_ESCAPE;
    g_keycodes[0x147] = KB_KEY_HOME;
    g_keycodes[0x152] = KB_KEY_INSERT;
    g_keycodes[0x15D] = KB_KEY_MENU;
    g_keycodes[0x151] = KB_KEY_PAGE_DOWN;
    g_keycodes[0x149] = KB_KEY_PAGE_UP;
    g_keycodes[0x045] = KB_KEY_PAUSE;
    g_keycodes[0x146] = KB_KEY_PAUSE;
    g_keycodes[0x039] = KB_KEY_SPACE;
    g_keycodes[0x00F] = KB_KEY_TAB;
    g_keycodes[0x03A] = KB_KEY_CAPS_LOCK;
    g_keycodes[0x145] = KB_KEY_NUM_LOCK;
    g_keycodes[0x046] = KB_KEY_SCROLL_LOCK;
    g_keycodes[0x03B] = KB_KEY_F1;
    g_keycodes[0x03C] = KB_KEY_F2;
    g_keycodes[0x03D] = KB_KEY_F3;
    g_keycodes[0x03E] = KB_KEY_F4;
    g_keycodes[0x03F] = KB_KEY_F5;
    g_keycodes[0x040] = KB_KEY_F6;
    g_keycodes[0x041] = KB_KEY_F7;
    g_keycodes[0x042] = KB_KEY_F8;
    g_keycodes[0x043] = KB_KEY_F9;
    g_keycodes[0x044] = KB_KEY_F10;
    g_keycodes[0x057] = KB_KEY_F11;
    g_keycodes[0x058] = KB_KEY_F12;
    g_keycodes[0x064] = KB_KEY_F13;
    g_keycodes[0x065] = KB_KEY_F14;
    g_keycodes[0x066] = KB_KEY_F15;
    g_keycodes[0x067] = KB_KEY_F16;
    g_keycodes[0x068] = KB_KEY_F17;
    g_keycodes[0x069] = KB_KEY_F18;
    g_keycodes[0x06A] = KB_KEY_F19;
    g_keycodes[0x06B] = KB_KEY_F20;
    g_keycodes[0x06C] = KB_KEY_F21;
    g_keycodes[0x06D] = KB_KEY_F22;
    g_keycodes[0x06E] = KB_KEY_F23;
    g_keycodes[0x076] = KB_KEY_F24;
    g_keycodes[0x038] = KB_KEY_LEFT_ALT;
    g_keycodes[0x01D] = KB_KEY_LEFT_CONTROL;
    g_keycodes[0x02A] = KB_KEY_LEFT_SHIFT;
    g_keycodes[0x15B] = KB_KEY_LEFT_SUPER;
    g_keycodes[0x137] = KB_KEY_PRINT_SCREEN;
    g_keycodes[0x138] = KB_KEY_RIGHT_ALT;
    g_keycodes[0x11D] = KB_KEY_RIGHT_CONTROL;
    g_keycodes[0x036] = KB_KEY_RIGHT_SHIFT;
    g_keycodes[0x15C] = KB_KEY_RIGHT_SUPER;
    g_keycodes[0x150] = KB_KEY_DOWN;
    g_keycodes[0x14B] = KB_KEY_LEFT;
    g_keycodes[0x14D] = KB_KEY_RIGHT;
    g_keycodes[0x148] = KB_KEY_UP;

    g_keycodes[0x052] = KB_KEY_KP_0;
    g_keycodes[0x04F] = KB_KEY_KP_1;
    g_keycodes[0x050] = KB_KEY_KP_2;
    g_keycodes[0x051] = KB_KEY_KP_3;
    g_keycodes[0x04B] = KB_KEY_KP_4;
    g_keycodes[0x04C] = KB_KEY_KP_5;
    g_keycodes[0x04D] = KB_KEY_KP_6;
    g_keycodes[0x047] = KB_KEY_KP_7;
    g_keycodes[0x048] = KB_KEY_KP_8;
    g_keycodes[0x049] = KB_KEY_KP_9;
    g_keycodes[0x04E] = KB_KEY_KP_ADD;
    g_keycodes[0x053] = KB_KEY_KP_DECIMAL;
    g_keycodes[0x135] = KB_KEY_KP_DIVIDE;
    g_keycodes[0x11C] = KB_KEY_KP_ENTER;
    g_keycodes[0x037] = KB_KEY_KP_MULTIPLY;
    g_keycodes[0x04A] = KB_KEY_KP_SUBTRACT;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

mfb_key 
translate_key(unsigned int wParam, unsigned long lParam) {
    if (wParam == VK_CONTROL) {
        MSG next;
        DWORD time;

        if (lParam & 0x01000000)
            return KB_KEY_RIGHT_CONTROL;

        time = GetMessageTime();
        if (PeekMessageW(&next, 0x0, 0, 0, PM_NOREMOVE))
            if (next.message == WM_KEYDOWN || next.message == WM_SYSKEYDOWN || next.message == WM_KEYUP || next.message == WM_SYSKEYUP)
                if (next.wParam == VK_MENU && (next.lParam & 0x01000000) && next.time == time)
                    return KB_KEY_UNKNOWN;

        return KB_KEY_LEFT_CONTROL;
    }

    if (wParam == VK_PROCESSKEY)
        return KB_KEY_UNKNOWN;

    return (mfb_key) g_keycodes[HIWORD(lParam) & 0x1FF];
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool 
mfb_set_viewport(struct mfb_window *window, unsigned offset_x, unsigned offset_y, unsigned width, unsigned height) {
    SWindowData     *window_data     = (SWindowData *) window;

    if (offset_x + width > window_data->window_width) {
        return false;
    }
    if (offset_y + height > window_data->window_height) {
        return false;
    }

    window_data->dst_offset_x = offset_x;
    window_data->dst_offset_y = offset_y;

    window_data->dst_width = width;
    window_data->dst_height = height;

    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extern double   g_timer_frequency;
extern double   g_timer_resolution;

uint64_t 
mfb_timer_tick() {
    int64_t     counter;

    QueryPerformanceCounter((LARGE_INTEGER *) &counter);

    return counter;
}

void 
mfb_timer_init() {
    uint64_t    frequency;

    QueryPerformanceFrequency((LARGE_INTEGER *) &frequency);

    g_timer_frequency  = (double) ((int64_t) frequency);
    g_timer_resolution = 1.0 / g_timer_frequency;
}
