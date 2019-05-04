#include <MiniFB.h>
#include <MiniFB_internal.h>
#include "WaylandWindowData.h"
#include <linux/input.h>
#include <wayland-client.h>

extern SWindowData g_window_data;

int mfb_open_ex(const char* title, int width, int height, int flags) {
    // TODO: Not yet
    kUnused(flags);
    return mfb_open(title, width, height);
}

eBool mfb_set_viewport(unsigned offset_x, unsigned offset_y, unsigned width, unsigned height) {

    if(offset_x + width > g_window_data.window_width) {
        return eFalse;
    }
    if(offset_y + height > g_window_data.window_height) {
        return eFalse;
    }

    // TODO: Not yet
    // g_window_data.dst_offset_x = offset_x;
    // g_window_data.dst_offset_y = offset_y;
    // g_window_data.dst_width    = width;
    // g_window_data.dst_height   = height;

    return eFalse;
}

extern short int keycodes[512];

void init_keycodes(void)
{
    // Clear keys
    for (size_t i = 0; i < sizeof(keycodes) / sizeof(keycodes[0]); ++i) 
        keycodes[i] = 0;

    keycodes[KEY_GRAVE]      = KB_KEY_GRAVE_ACCENT;
    keycodes[KEY_1]          = KB_KEY_1;
    keycodes[KEY_2]          = KB_KEY_2;
    keycodes[KEY_3]          = KB_KEY_3;
    keycodes[KEY_4]          = KB_KEY_4;
    keycodes[KEY_5]          = KB_KEY_5;
    keycodes[KEY_6]          = KB_KEY_6;
    keycodes[KEY_7]          = KB_KEY_7;
    keycodes[KEY_8]          = KB_KEY_8;
    keycodes[KEY_9]          = KB_KEY_9;
    keycodes[KEY_0]          = KB_KEY_0;
    keycodes[KEY_SPACE]      = KB_KEY_SPACE;
    keycodes[KEY_MINUS]      = KB_KEY_MINUS;
    keycodes[KEY_EQUAL]      = KB_KEY_EQUAL;
    keycodes[KEY_Q]          = KB_KEY_Q;
    keycodes[KEY_W]          = KB_KEY_W;
    keycodes[KEY_E]          = KB_KEY_E;
    keycodes[KEY_R]          = KB_KEY_R;
    keycodes[KEY_T]          = KB_KEY_T;
    keycodes[KEY_Y]          = KB_KEY_Y;
    keycodes[KEY_U]          = KB_KEY_U;
    keycodes[KEY_I]          = KB_KEY_I;
    keycodes[KEY_O]          = KB_KEY_O;
    keycodes[KEY_P]          = KB_KEY_P;
    keycodes[KEY_LEFTBRACE]  = KB_KEY_LEFT_BRACKET;
    keycodes[KEY_RIGHTBRACE] = KB_KEY_RIGHT_BRACKET;
    keycodes[KEY_A]          = KB_KEY_A;
    keycodes[KEY_S]          = KB_KEY_S;
    keycodes[KEY_D]          = KB_KEY_D;
    keycodes[KEY_F]          = KB_KEY_F;
    keycodes[KEY_G]          = KB_KEY_G;
    keycodes[KEY_H]          = KB_KEY_H;
    keycodes[KEY_J]          = KB_KEY_J;
    keycodes[KEY_K]          = KB_KEY_K;
    keycodes[KEY_L]          = KB_KEY_L;
    keycodes[KEY_SEMICOLON]  = KB_KEY_SEMICOLON;
    keycodes[KEY_APOSTROPHE] = KB_KEY_APOSTROPHE;
    keycodes[KEY_Z]          = KB_KEY_Z;
    keycodes[KEY_X]          = KB_KEY_X;
    keycodes[KEY_C]          = KB_KEY_C;
    keycodes[KEY_V]          = KB_KEY_V;
    keycodes[KEY_B]          = KB_KEY_B;
    keycodes[KEY_N]          = KB_KEY_N;
    keycodes[KEY_M]          = KB_KEY_M;
    keycodes[KEY_COMMA]      = KB_KEY_COMMA;
    keycodes[KEY_DOT]        = KB_KEY_PERIOD;
    keycodes[KEY_SLASH]      = KB_KEY_SLASH;
    keycodes[KEY_BACKSLASH]  = KB_KEY_BACKSLASH;
    keycodes[KEY_ESC]        = KB_KEY_ESCAPE;
    keycodes[KEY_TAB]        = KB_KEY_TAB;
    keycodes[KEY_LEFTSHIFT]  = KB_KEY_LEFT_SHIFT;
    keycodes[KEY_RIGHTSHIFT] = KB_KEY_RIGHT_SHIFT;
    keycodes[KEY_LEFTCTRL]   = KB_KEY_LEFT_CONTROL;
    keycodes[KEY_RIGHTCTRL]  = KB_KEY_RIGHT_CONTROL;
    keycodes[KEY_LEFTALT]    = KB_KEY_LEFT_ALT;
    keycodes[KEY_RIGHTALT]   = KB_KEY_RIGHT_ALT;
    keycodes[KEY_LEFTMETA]   = KB_KEY_LEFT_SUPER;
    keycodes[KEY_RIGHTMETA]  = KB_KEY_RIGHT_SUPER;
    keycodes[KEY_MENU]       = KB_KEY_MENU;
    keycodes[KEY_NUMLOCK]    = KB_KEY_NUM_LOCK;
    keycodes[KEY_CAPSLOCK]   = KB_KEY_CAPS_LOCK;
    keycodes[KEY_PRINT]      = KB_KEY_PRINT_SCREEN;
    keycodes[KEY_SCROLLLOCK] = KB_KEY_SCROLL_LOCK;
    keycodes[KEY_PAUSE]      = KB_KEY_PAUSE;
    keycodes[KEY_DELETE]     = KB_KEY_DELETE;
    keycodes[KEY_BACKSPACE]  = KB_KEY_BACKSPACE;
    keycodes[KEY_ENTER]      = KB_KEY_ENTER;
    keycodes[KEY_HOME]       = KB_KEY_HOME;
    keycodes[KEY_END]        = KB_KEY_END;
    keycodes[KEY_PAGEUP]     = KB_KEY_PAGE_UP;
    keycodes[KEY_PAGEDOWN]   = KB_KEY_PAGE_DOWN;
    keycodes[KEY_INSERT]     = KB_KEY_INSERT;
    keycodes[KEY_LEFT]       = KB_KEY_LEFT;
    keycodes[KEY_RIGHT]      = KB_KEY_RIGHT;
    keycodes[KEY_DOWN]       = KB_KEY_DOWN;
    keycodes[KEY_UP]         = KB_KEY_UP;
    keycodes[KEY_F1]         = KB_KEY_F1;
    keycodes[KEY_F2]         = KB_KEY_F2;
    keycodes[KEY_F3]         = KB_KEY_F3;
    keycodes[KEY_F4]         = KB_KEY_F4;
    keycodes[KEY_F5]         = KB_KEY_F5;
    keycodes[KEY_F6]         = KB_KEY_F6;
    keycodes[KEY_F7]         = KB_KEY_F7;
    keycodes[KEY_F8]         = KB_KEY_F8;
    keycodes[KEY_F9]         = KB_KEY_F9;
    keycodes[KEY_F10]        = KB_KEY_F10;
    keycodes[KEY_F11]        = KB_KEY_F11;
    keycodes[KEY_F12]        = KB_KEY_F12;
    keycodes[KEY_F13]        = KB_KEY_F13;
    keycodes[KEY_F14]        = KB_KEY_F14;
    keycodes[KEY_F15]        = KB_KEY_F15;
    keycodes[KEY_F16]        = KB_KEY_F16;
    keycodes[KEY_F17]        = KB_KEY_F17;
    keycodes[KEY_F18]        = KB_KEY_F18;
    keycodes[KEY_F19]        = KB_KEY_F19;
    keycodes[KEY_F20]        = KB_KEY_F20;
    keycodes[KEY_F21]        = KB_KEY_F21;
    keycodes[KEY_F22]        = KB_KEY_F22;
    keycodes[KEY_F23]        = KB_KEY_F23;
    keycodes[KEY_F24]        = KB_KEY_F24;
    keycodes[KEY_KPSLASH]    = KB_KEY_KP_DIVIDE;
    keycodes[KEY_KPDOT]      = KB_KEY_KP_MULTIPLY;
    keycodes[KEY_KPMINUS]    = KB_KEY_KP_SUBTRACT;
    keycodes[KEY_KPPLUS]     = KB_KEY_KP_ADD;
    keycodes[KEY_KP0]        = KB_KEY_KP_0;
    keycodes[KEY_KP1]        = KB_KEY_KP_1;
    keycodes[KEY_KP2]        = KB_KEY_KP_2;
    keycodes[KEY_KP3]        = KB_KEY_KP_3;
    keycodes[KEY_KP4]        = KB_KEY_KP_4;
    keycodes[KEY_KP5]        = KB_KEY_KP_5;
    keycodes[KEY_KP6]        = KB_KEY_KP_6;
    keycodes[KEY_KP7]        = KB_KEY_KP_7;
    keycodes[KEY_KP8]        = KB_KEY_KP_8;
    keycodes[KEY_KP9]        = KB_KEY_KP_9;
    keycodes[KEY_KPCOMMA]    = KB_KEY_KP_DECIMAL;
    keycodes[KEY_KPEQUAL]    = KB_KEY_KP_EQUAL;
    keycodes[KEY_KPENTER]    = KB_KEY_KP_ENTER;
}
