#include <MiniFB.h>
#include <MiniFB_internal.h>
#include "WinWindowData.h"

#include <stdlib.h>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


SWindowData g_window_data  = { 0 };

WNDCLASS    s_wc           = { 0 };
HDC         s_hdc          = 0;
BITMAPINFO  *s_bitmapInfo  = 0x0;
long        s_window_style   = WS_POPUP | WS_SYSMENU | WS_CAPTION;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool ManageMessagesEx(HWND hWnd, unsigned int message, unsigned int wParam, unsigned long lParam);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    LRESULT res = 0;

    if (ManageMessagesEx(hWnd, (unsigned int)message, (unsigned int)wParam, (unsigned long)lParam) == true) {
        return res;
    }

    switch (message)
    {
        case WM_PAINT:
        {
            if (g_window_data.draw_buffer)
            {
                if (g_window_data.dst_offset_x > 0) {
                    BitBlt(s_hdc, 0, g_window_data.dst_offset_y, g_window_data.dst_offset_x, g_window_data.dst_height, 0, 0, 0, BLACKNESS);
                }
                if (g_window_data.dst_offset_y > 0) {
                    BitBlt(s_hdc, 0, 0, g_window_data.window_width, g_window_data.dst_offset_y, 0, 0, 0, BLACKNESS);
                }
                uint32_t offsetY = g_window_data.dst_offset_y + g_window_data.dst_height;
                if (offsetY < g_window_data.window_height) {
                    BitBlt(s_hdc, 0, offsetY, g_window_data.window_width, g_window_data.window_height-offsetY, 0, 0, 0, BLACKNESS);
                }
                uint32_t offsetX = g_window_data.dst_offset_x + g_window_data.dst_width;
                if (offsetX < g_window_data.window_width) {
                    BitBlt(s_hdc, offsetX, g_window_data.dst_offset_y, g_window_data.window_width-offsetX, g_window_data.dst_height, 0, 0, 0, BLACKNESS);
                }

                StretchDIBits(s_hdc, g_window_data.dst_offset_x, g_window_data.dst_offset_y, g_window_data.dst_width, g_window_data.dst_height, 0, 0, g_window_data.buffer_width, g_window_data.buffer_height, g_window_data.draw_buffer, 
                              s_bitmapInfo, DIB_RGB_COLORS, SRCCOPY);

                ValidateRect(hWnd, NULL);
            }

            break;
        }

        // Already managed in ManageMessagesEx
        case WM_KEYDOWN:
        {
            if ((wParam & 0xFF) == 27)
                g_window_data.close = true;

            break;
        }

        case WM_DESTROY:
        case WM_CLOSE:
        {
            g_window_data.close = true;
            break;
        }

        default:
        {
            res = DefWindowProc(hWnd, message, wParam, lParam);
        }
    }

    return res;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int mfb_open(const char* title, int width, int height) {
    return mfb_open_ex(title, width, height, 0);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int mfb_update(void* buffer)
{
    MSG msg;
    
    if (buffer == 0x0)
        return -2;

    if (g_window_data.close == true)
        return -1;

    g_window_data.draw_buffer = buffer;

    InvalidateRect(g_window_data.window, NULL, TRUE);
    SendMessage(g_window_data.window, WM_PAINT, 0, 0);

    while (g_window_data.close == false && PeekMessage(&msg, g_window_data.window, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void mfb_close()
{
    g_window_data.draw_buffer = 0x0;
    if (s_bitmapInfo != 0x0) {
        free(s_bitmapInfo);
    }
    if (g_window_data.window != 0 && s_hdc != 0) {
        ReleaseDC(g_window_data.window, s_hdc);
        DestroyWindow(g_window_data.window);
    }

    g_window_data.window = 0;
    s_hdc = 0;
    s_bitmapInfo = 0x0;
    g_window_data.close = true;
}

