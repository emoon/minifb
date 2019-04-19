#include <MiniFB.h>
#include <MiniFB_internal.h>
#include "WinWindowData.h"
#include <stdlib.h>

extern SWindowData g_window_data;

extern WNDCLASS     s_wc;
extern HDC          s_hdc;
extern BITMAPINFO   *s_bitmapInfo;
extern long         s_window_style;

eBool s_mouse_inside = eTrue;


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int translate_mod();
int translate_key(unsigned int wParam, unsigned long lParam);

eBool ManageMessagesEx(HWND hWnd, unsigned int message, unsigned int wParam, unsigned long lParam) 
{
    switch (message)
    {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYUP:
        {
            int kb_key     = translate_key((unsigned int)wParam, (unsigned long)lParam);
            int is_pressed = !((lParam >> 31) & 1);
            int kb_mods    = translate_mod();

            if (kb_key == KB_KEY_UNKNOWN)
                return FALSE;

            kCall(s_keyboard, kb_key, kb_mods, is_pressed);
            break;
        }

        case WM_CHAR:
        case WM_SYSCHAR:
        case WM_UNICHAR: 
        {

            if(message == WM_UNICHAR && wParam == UNICODE_NOCHAR) {
                // WM_UNICHAR is not sent by Windows, but is sent by some third-party input method engine
                // Returning TRUE here announces support for this message
                return TRUE;
            }

            kCall(s_char_input, wParam);
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
            eMouseButton button     = MOUSE_BTN_0;
            eKeyMod      kb_mods    = translate_mod();
            int          is_pressed = 0;
            switch (message) {
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
            kCall(s_mouse_btn, button, kb_mods, is_pressed);
            break;
        }

        case WM_MOUSEWHEEL:
            kCall(s_mouse_wheel, translate_mod(), 0.0f, (SHORT)HIWORD(wParam) / (float)WHEEL_DELTA);
            break;

        case WM_MOUSEHWHEEL:
            // This message is only sent on Windows Vista and later
            // NOTE: The X-axis is inverted for consistency with macOS and X11
            kCall(s_mouse_wheel, translate_mod(), -((SHORT)HIWORD(wParam) / (float)WHEEL_DELTA), 0.0f);
            break;

        case WM_MOUSEMOVE:
            if(s_mouse_inside == eFalse) {
                s_mouse_inside = eTrue;
                TRACKMOUSEEVENT tme;
                ZeroMemory(&tme, sizeof(tme));
                tme.cbSize = sizeof(tme);
                tme.dwFlags = TME_LEAVE;
                tme.hwndTrack = hWnd;
                TrackMouseEvent(&tme);
            }
            kCall(s_mouse_move, ((int)(short)LOWORD(lParam)), ((int)(short)HIWORD(lParam)));
            break;

        case WM_MOUSELEAVE:
            s_mouse_inside = eFalse;
            break;

        case WM_SIZE:
        {
            g_window_data.dst_width  = LOWORD(lParam);
            g_window_data.dst_height = HIWORD(lParam);
            g_window_data.window_width  = g_window_data.dst_width;
            g_window_data.window_height = g_window_data.dst_height;
            kCall(s_resize, g_window_data.dst_width, g_window_data.dst_height);
            break;
        }

        case WM_SETFOCUS:
            kCall(s_active, eTrue);
            break;

        case WM_KILLFOCUS:
            kCall(s_active, eFalse);
            break;

        case WM_CLOSE:
        {
            g_window_data.close = 1;
            break;
        }

        default:
        {
            return eFalse;
        }
    }

    return eTrue;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void keyboard_default(eKey key, eKeyMod mod, eBool isPressed) {
    kUnused(mod);
    kUnused(isPressed);
    if (key == KB_KEY_ESCAPE)
        g_window_data.close = eTrue;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

int mfb_open_ex(const char* title, int width, int height, int flags) {
    RECT rect = { 0 };
    int  x, y;

    init_keycodes();

    g_window_data.buffer_width = width;
    g_window_data.buffer_height = height;

    s_window_style = WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME;
    if (flags & WF_FULLSCREEN) {
        flags = WF_FULLSCREEN;  // Remove all other flags
        x = 0;
        y = 0;
        rect.right  = GetSystemMetrics(SM_CXSCREEN);
        rect.bottom = GetSystemMetrics(SM_CYSCREEN);
        s_window_style = WS_POPUP & ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZE | WS_MAXIMIZE | WS_SYSMENU);

        DEVMODE settings;
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
        if (rect.bottom > height) {
            height -= (rect.bottom - height);
            rect.bottom += (rect.bottom - height);
            rect.top = 0;
        }
        x = 0;
        y = 0;
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

    s_wc.style         = CS_OWNDC | CS_VREDRAW | CS_HREDRAW;
    s_wc.lpfnWndProc   = WndProc;
    s_wc.hCursor       = LoadCursor(0, IDC_ARROW);
    s_wc.lpszClassName = title;
    RegisterClass(&s_wc);

    if (g_window_data.dst_width == 0)
        g_window_data.dst_width = width;

    if (g_window_data.dst_height == 0)
        g_window_data.dst_height = height;

    g_window_data.window_width  = rect.right;
    g_window_data.window_height = rect.bottom;

    g_window_data.window = CreateWindowEx(
        0,
        title, title,
        s_window_style,
        x, y,
        g_window_data.window_width, g_window_data.window_height,
        0, 0, 0, 0);

    if (!g_window_data.window)
        return 0;

    if (flags & WF_ALWAYS_ON_TOP)
        SetWindowPos(g_window_data.window, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

    ShowWindow(g_window_data.window, SW_NORMAL);

    s_bitmapInfo = (BITMAPINFO *) calloc(1, sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD) * 3);
    s_bitmapInfo->bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    s_bitmapInfo->bmiHeader.biPlanes      = 1;
    s_bitmapInfo->bmiHeader.biBitCount    = 32;
    s_bitmapInfo->bmiHeader.biCompression = BI_BITFIELDS;
    s_bitmapInfo->bmiHeader.biWidth       = g_window_data.buffer_width;
    s_bitmapInfo->bmiHeader.biHeight      = -(LONG)g_window_data.buffer_height;
    s_bitmapInfo->bmiColors[0].rgbRed     = 0xff;
    s_bitmapInfo->bmiColors[1].rgbGreen   = 0xff;
    s_bitmapInfo->bmiColors[2].rgbBlue    = 0xff;

    s_hdc = GetDC(g_window_data.window);

    if (s_keyboard == 0x0) {
        mfb_keyboard_callback(keyboard_default);
    }

    return 1;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

eBool mfb_set_viewport(unsigned offset_x, unsigned offset_y, unsigned width, unsigned height)
{
    if(offset_x + width > g_window_data.window_width) {
        return eFalse;
    }
    if(offset_y + height > g_window_data.window_height) {
        return eFalse;
    }

    g_window_data.dst_offset_x = offset_x;
    g_window_data.dst_offset_y = offset_y;

    g_window_data.dst_width = width;
    g_window_data.dst_height = height;

    //g_window_data.buffer_width = width;
    //g_window_data.buffer_height = height;
    //if (s_bitmapInfo)
    //{
    //    s_bitmapInfo->bmiHeader.biWidth = width;
    //    s_bitmapInfo->bmiHeader.biHeight = height;
    //}

    return eTrue;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int translate_mod() {
    int mods = 0;

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

extern short int keycodes[512];

void init_keycodes() {

    // Clear keys
    for (size_t i = 0; i < sizeof(keycodes) / sizeof(keycodes[0]); ++i) 
        keycodes[i] = 0;

    keycodes[0x00B] = KB_KEY_0;
    keycodes[0x002] = KB_KEY_1;
    keycodes[0x003] = KB_KEY_2;
    keycodes[0x004] = KB_KEY_3;
    keycodes[0x005] = KB_KEY_4;
    keycodes[0x006] = KB_KEY_5;
    keycodes[0x007] = KB_KEY_6;
    keycodes[0x008] = KB_KEY_7;
    keycodes[0x009] = KB_KEY_8;
    keycodes[0x00A] = KB_KEY_9;
    keycodes[0x01E] = KB_KEY_A;
    keycodes[0x030] = KB_KEY_B;
    keycodes[0x02E] = KB_KEY_C;
    keycodes[0x020] = KB_KEY_D;
    keycodes[0x012] = KB_KEY_E;
    keycodes[0x021] = KB_KEY_F;
    keycodes[0x022] = KB_KEY_G;
    keycodes[0x023] = KB_KEY_H;
    keycodes[0x017] = KB_KEY_I;
    keycodes[0x024] = KB_KEY_J;
    keycodes[0x025] = KB_KEY_K;
    keycodes[0x026] = KB_KEY_L;
    keycodes[0x032] = KB_KEY_M;
    keycodes[0x031] = KB_KEY_N;
    keycodes[0x018] = KB_KEY_O;
    keycodes[0x019] = KB_KEY_P;
    keycodes[0x010] = KB_KEY_Q;
    keycodes[0x013] = KB_KEY_R;
    keycodes[0x01F] = KB_KEY_S;
    keycodes[0x014] = KB_KEY_T;
    keycodes[0x016] = KB_KEY_U;
    keycodes[0x02F] = KB_KEY_V;
    keycodes[0x011] = KB_KEY_W;
    keycodes[0x02D] = KB_KEY_X;
    keycodes[0x015] = KB_KEY_Y;
    keycodes[0x02C] = KB_KEY_Z;

    keycodes[0x028] = KB_KEY_APOSTROPHE;
    keycodes[0x02B] = KB_KEY_BACKSLASH;
    keycodes[0x033] = KB_KEY_COMMA;
    keycodes[0x00D] = KB_KEY_EQUAL;
    keycodes[0x029] = KB_KEY_GRAVE_ACCENT;
    keycodes[0x01A] = KB_KEY_LEFT_BRACKET;
    keycodes[0x00C] = KB_KEY_MINUS;
    keycodes[0x034] = KB_KEY_PERIOD;
    keycodes[0x01B] = KB_KEY_RIGHT_BRACKET;
    keycodes[0x027] = KB_KEY_SEMICOLON;
    keycodes[0x035] = KB_KEY_SLASH;
    keycodes[0x056] = KB_KEY_WORLD_2;

    keycodes[0x00E] = KB_KEY_BACKSPACE;
    keycodes[0x153] = KB_KEY_DELETE;
    keycodes[0x14F] = KB_KEY_END;
    keycodes[0x01C] = KB_KEY_ENTER;
    keycodes[0x001] = KB_KEY_ESCAPE;
    keycodes[0x147] = KB_KEY_HOME;
    keycodes[0x152] = KB_KEY_INSERT;
    keycodes[0x15D] = KB_KEY_MENU;
    keycodes[0x151] = KB_KEY_PAGE_DOWN;
    keycodes[0x149] = KB_KEY_PAGE_UP;
    keycodes[0x045] = KB_KEY_PAUSE;
    keycodes[0x146] = KB_KEY_PAUSE;
    keycodes[0x039] = KB_KEY_SPACE;
    keycodes[0x00F] = KB_KEY_TAB;
    keycodes[0x03A] = KB_KEY_CAPS_LOCK;
    keycodes[0x145] = KB_KEY_NUM_LOCK;
    keycodes[0x046] = KB_KEY_SCROLL_LOCK;
    keycodes[0x03B] = KB_KEY_F1;
    keycodes[0x03C] = KB_KEY_F2;
    keycodes[0x03D] = KB_KEY_F3;
    keycodes[0x03E] = KB_KEY_F4;
    keycodes[0x03F] = KB_KEY_F5;
    keycodes[0x040] = KB_KEY_F6;
    keycodes[0x041] = KB_KEY_F7;
    keycodes[0x042] = KB_KEY_F8;
    keycodes[0x043] = KB_KEY_F9;
    keycodes[0x044] = KB_KEY_F10;
    keycodes[0x057] = KB_KEY_F11;
    keycodes[0x058] = KB_KEY_F12;
    keycodes[0x064] = KB_KEY_F13;
    keycodes[0x065] = KB_KEY_F14;
    keycodes[0x066] = KB_KEY_F15;
    keycodes[0x067] = KB_KEY_F16;
    keycodes[0x068] = KB_KEY_F17;
    keycodes[0x069] = KB_KEY_F18;
    keycodes[0x06A] = KB_KEY_F19;
    keycodes[0x06B] = KB_KEY_F20;
    keycodes[0x06C] = KB_KEY_F21;
    keycodes[0x06D] = KB_KEY_F22;
    keycodes[0x06E] = KB_KEY_F23;
    keycodes[0x076] = KB_KEY_F24;
    keycodes[0x038] = KB_KEY_LEFT_ALT;
    keycodes[0x01D] = KB_KEY_LEFT_CONTROL;
    keycodes[0x02A] = KB_KEY_LEFT_SHIFT;
    keycodes[0x15B] = KB_KEY_LEFT_SUPER;
    keycodes[0x137] = KB_KEY_PRINT_SCREEN;
    keycodes[0x138] = KB_KEY_RIGHT_ALT;
    keycodes[0x11D] = KB_KEY_RIGHT_CONTROL;
    keycodes[0x036] = KB_KEY_RIGHT_SHIFT;
    keycodes[0x15C] = KB_KEY_RIGHT_SUPER;
    keycodes[0x150] = KB_KEY_DOWN;
    keycodes[0x14B] = KB_KEY_LEFT;
    keycodes[0x14D] = KB_KEY_RIGHT;
    keycodes[0x148] = KB_KEY_UP;

    keycodes[0x052] = KB_KEY_KP_0;
    keycodes[0x04F] = KB_KEY_KP_1;
    keycodes[0x050] = KB_KEY_KP_2;
    keycodes[0x051] = KB_KEY_KP_3;
    keycodes[0x04B] = KB_KEY_KP_4;
    keycodes[0x04C] = KB_KEY_KP_5;
    keycodes[0x04D] = KB_KEY_KP_6;
    keycodes[0x047] = KB_KEY_KP_7;
    keycodes[0x048] = KB_KEY_KP_8;
    keycodes[0x049] = KB_KEY_KP_9;
    keycodes[0x04E] = KB_KEY_KP_ADD;
    keycodes[0x053] = KB_KEY_KP_DECIMAL;
    keycodes[0x135] = KB_KEY_KP_DIVIDE;
    keycodes[0x11C] = KB_KEY_KP_ENTER;
    keycodes[0x037] = KB_KEY_KP_MULTIPLY;
    keycodes[0x04A] = KB_KEY_KP_SUBTRACT;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int translate_key(unsigned int wParam, unsigned long lParam) {
    if (wParam == VK_CONTROL) {
        MSG next;
        DWORD time;

        if (lParam & 0x01000000)
            return KB_KEY_RIGHT_CONTROL;

        time = GetMessageTime();
        if (PeekMessageW(&next, NULL, 0, 0, PM_NOREMOVE))
            if (next.message == WM_KEYDOWN || next.message == WM_SYSKEYDOWN || next.message == WM_KEYUP || next.message == WM_SYSKEYUP)
                if (next.wParam == VK_MENU && (next.lParam & 0x01000000) && next.time == time)
                    return KB_KEY_UNKNOWN;

        return KB_KEY_LEFT_CONTROL;
    }

    if (wParam == VK_PROCESSKEY)
        return KB_KEY_UNKNOWN;

    return keycodes[HIWORD(lParam) & 0x1FF];
}
