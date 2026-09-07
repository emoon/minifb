#include "MiniFB_evdev_keys.h"

#include "MiniFB_internal.h"

#if defined(__linux__)

#include <linux/input-event-codes.h>

#include <stddef.h>

//-------------------------------------
// The table is built apart from g_keycodes because each backend reaches the same physical
// key at a different number, and the offset between them is all that changes.
//-------------------------------------
static short int s_evdev_keycodes[MFB_MAX_KEYS];
static bool      s_table_built = false;

//-------------------------------------
static void
build_evdev_table(void) {
    for (size_t i = 0; i < MFB_MAX_KEYS; ++i) {
        s_evdev_keycodes[i] = MFB_KB_KEY_UNKNOWN;
    }

    s_evdev_keycodes[KEY_GRAVE]      = MFB_KB_KEY_GRAVE_ACCENT;
    s_evdev_keycodes[KEY_1]          = MFB_KB_KEY_1;
    s_evdev_keycodes[KEY_2]          = MFB_KB_KEY_2;
    s_evdev_keycodes[KEY_3]          = MFB_KB_KEY_3;
    s_evdev_keycodes[KEY_4]          = MFB_KB_KEY_4;
    s_evdev_keycodes[KEY_5]          = MFB_KB_KEY_5;
    s_evdev_keycodes[KEY_6]          = MFB_KB_KEY_6;
    s_evdev_keycodes[KEY_7]          = MFB_KB_KEY_7;
    s_evdev_keycodes[KEY_8]          = MFB_KB_KEY_8;
    s_evdev_keycodes[KEY_9]          = MFB_KB_KEY_9;
    s_evdev_keycodes[KEY_0]          = MFB_KB_KEY_0;
    s_evdev_keycodes[KEY_SPACE]      = MFB_KB_KEY_SPACE;
    s_evdev_keycodes[KEY_MINUS]      = MFB_KB_KEY_MINUS;
    s_evdev_keycodes[KEY_EQUAL]      = MFB_KB_KEY_EQUAL;
    s_evdev_keycodes[KEY_Q]          = MFB_KB_KEY_Q;
    s_evdev_keycodes[KEY_W]          = MFB_KB_KEY_W;
    s_evdev_keycodes[KEY_E]          = MFB_KB_KEY_E;
    s_evdev_keycodes[KEY_R]          = MFB_KB_KEY_R;
    s_evdev_keycodes[KEY_T]          = MFB_KB_KEY_T;
    s_evdev_keycodes[KEY_Y]          = MFB_KB_KEY_Y;
    s_evdev_keycodes[KEY_U]          = MFB_KB_KEY_U;
    s_evdev_keycodes[KEY_I]          = MFB_KB_KEY_I;
    s_evdev_keycodes[KEY_O]          = MFB_KB_KEY_O;
    s_evdev_keycodes[KEY_P]          = MFB_KB_KEY_P;
    s_evdev_keycodes[KEY_LEFTBRACE]  = MFB_KB_KEY_LEFT_BRACKET;
    s_evdev_keycodes[KEY_RIGHTBRACE] = MFB_KB_KEY_RIGHT_BRACKET;
    s_evdev_keycodes[KEY_A]          = MFB_KB_KEY_A;
    s_evdev_keycodes[KEY_S]          = MFB_KB_KEY_S;
    s_evdev_keycodes[KEY_D]          = MFB_KB_KEY_D;
    s_evdev_keycodes[KEY_F]          = MFB_KB_KEY_F;
    s_evdev_keycodes[KEY_G]          = MFB_KB_KEY_G;
    s_evdev_keycodes[KEY_H]          = MFB_KB_KEY_H;
    s_evdev_keycodes[KEY_J]          = MFB_KB_KEY_J;
    s_evdev_keycodes[KEY_K]          = MFB_KB_KEY_K;
    s_evdev_keycodes[KEY_L]          = MFB_KB_KEY_L;
    s_evdev_keycodes[KEY_SEMICOLON]  = MFB_KB_KEY_SEMICOLON;
    s_evdev_keycodes[KEY_APOSTROPHE] = MFB_KB_KEY_APOSTROPHE;
    s_evdev_keycodes[KEY_Z]          = MFB_KB_KEY_Z;
    s_evdev_keycodes[KEY_X]          = MFB_KB_KEY_X;
    s_evdev_keycodes[KEY_C]          = MFB_KB_KEY_C;
    s_evdev_keycodes[KEY_V]          = MFB_KB_KEY_V;
    s_evdev_keycodes[KEY_B]          = MFB_KB_KEY_B;
    s_evdev_keycodes[KEY_N]          = MFB_KB_KEY_N;
    s_evdev_keycodes[KEY_M]          = MFB_KB_KEY_M;
    s_evdev_keycodes[KEY_COMMA]      = MFB_KB_KEY_COMMA;
    s_evdev_keycodes[KEY_DOT]        = MFB_KB_KEY_PERIOD;
    s_evdev_keycodes[KEY_SLASH]      = MFB_KB_KEY_SLASH;
    s_evdev_keycodes[KEY_BACKSLASH]  = MFB_KB_KEY_BACKSLASH;
    s_evdev_keycodes[KEY_ESC]        = MFB_KB_KEY_ESCAPE;
    s_evdev_keycodes[KEY_TAB]        = MFB_KB_KEY_TAB;
    s_evdev_keycodes[KEY_LEFTSHIFT]  = MFB_KB_KEY_LEFT_SHIFT;
    s_evdev_keycodes[KEY_RIGHTSHIFT] = MFB_KB_KEY_RIGHT_SHIFT;
    s_evdev_keycodes[KEY_LEFTCTRL]   = MFB_KB_KEY_LEFT_CONTROL;
    s_evdev_keycodes[KEY_RIGHTCTRL]  = MFB_KB_KEY_RIGHT_CONTROL;
    s_evdev_keycodes[KEY_LEFTALT]    = MFB_KB_KEY_LEFT_ALT;
    s_evdev_keycodes[KEY_RIGHTALT]   = MFB_KB_KEY_RIGHT_ALT;
    s_evdev_keycodes[KEY_LEFTMETA]   = MFB_KB_KEY_LEFT_SUPER;
    s_evdev_keycodes[KEY_RIGHTMETA]  = MFB_KB_KEY_RIGHT_SUPER;
    s_evdev_keycodes[KEY_MENU]       = MFB_KB_KEY_MENU;
    s_evdev_keycodes[KEY_NUMLOCK]    = MFB_KB_KEY_NUM_LOCK;
    s_evdev_keycodes[KEY_CAPSLOCK]   = MFB_KB_KEY_CAPS_LOCK;
    s_evdev_keycodes[KEY_PRINT]      = MFB_KB_KEY_PRINT_SCREEN;
    // The kernel's HID table maps the physical keys below to these codes,
    // not to KEY_PRINT/KEY_MENU above; keep both so devices that do emit the
    // legacy codes still work.
    s_evdev_keycodes[KEY_SYSRQ]      = MFB_KB_KEY_PRINT_SCREEN;
    s_evdev_keycodes[KEY_COMPOSE]    = MFB_KB_KEY_MENU;
    s_evdev_keycodes[KEY_102ND]      = MFB_KB_KEY_WORLD_2;
    s_evdev_keycodes[KEY_SCROLLLOCK] = MFB_KB_KEY_SCROLL_LOCK;
    s_evdev_keycodes[KEY_PAUSE]      = MFB_KB_KEY_PAUSE;
    s_evdev_keycodes[KEY_DELETE]     = MFB_KB_KEY_DELETE;
    s_evdev_keycodes[KEY_BACKSPACE]  = MFB_KB_KEY_BACKSPACE;
    s_evdev_keycodes[KEY_ENTER]      = MFB_KB_KEY_ENTER;
    s_evdev_keycodes[KEY_HOME]       = MFB_KB_KEY_HOME;
    s_evdev_keycodes[KEY_END]        = MFB_KB_KEY_END;
    s_evdev_keycodes[KEY_PAGEUP]     = MFB_KB_KEY_PAGE_UP;
    s_evdev_keycodes[KEY_PAGEDOWN]   = MFB_KB_KEY_PAGE_DOWN;
    s_evdev_keycodes[KEY_INSERT]     = MFB_KB_KEY_INSERT;
    s_evdev_keycodes[KEY_LEFT]       = MFB_KB_KEY_LEFT;
    s_evdev_keycodes[KEY_RIGHT]      = MFB_KB_KEY_RIGHT;
    s_evdev_keycodes[KEY_DOWN]       = MFB_KB_KEY_DOWN;
    s_evdev_keycodes[KEY_UP]         = MFB_KB_KEY_UP;
    s_evdev_keycodes[KEY_F1]         = MFB_KB_KEY_F1;
    s_evdev_keycodes[KEY_F2]         = MFB_KB_KEY_F2;
    s_evdev_keycodes[KEY_F3]         = MFB_KB_KEY_F3;
    s_evdev_keycodes[KEY_F4]         = MFB_KB_KEY_F4;
    s_evdev_keycodes[KEY_F5]         = MFB_KB_KEY_F5;
    s_evdev_keycodes[KEY_F6]         = MFB_KB_KEY_F6;
    s_evdev_keycodes[KEY_F7]         = MFB_KB_KEY_F7;
    s_evdev_keycodes[KEY_F8]         = MFB_KB_KEY_F8;
    s_evdev_keycodes[KEY_F9]         = MFB_KB_KEY_F9;
    s_evdev_keycodes[KEY_F10]        = MFB_KB_KEY_F10;
    s_evdev_keycodes[KEY_F11]        = MFB_KB_KEY_F11;
    s_evdev_keycodes[KEY_F12]        = MFB_KB_KEY_F12;
    s_evdev_keycodes[KEY_F13]        = MFB_KB_KEY_F13;
    s_evdev_keycodes[KEY_F14]        = MFB_KB_KEY_F14;
    s_evdev_keycodes[KEY_F15]        = MFB_KB_KEY_F15;
    s_evdev_keycodes[KEY_F16]        = MFB_KB_KEY_F16;
    s_evdev_keycodes[KEY_F17]        = MFB_KB_KEY_F17;
    s_evdev_keycodes[KEY_F18]        = MFB_KB_KEY_F18;
    s_evdev_keycodes[KEY_F19]        = MFB_KB_KEY_F19;
    s_evdev_keycodes[KEY_F20]        = MFB_KB_KEY_F20;
    s_evdev_keycodes[KEY_F21]        = MFB_KB_KEY_F21;
    s_evdev_keycodes[KEY_F22]        = MFB_KB_KEY_F22;
    s_evdev_keycodes[KEY_F23]        = MFB_KB_KEY_F23;
    s_evdev_keycodes[KEY_F24]        = MFB_KB_KEY_F24;
    s_evdev_keycodes[KEY_KPSLASH]    = MFB_KB_KEY_KP_DIVIDE;
    s_evdev_keycodes[KEY_KPASTERISK] = MFB_KB_KEY_KP_MULTIPLY;
    s_evdev_keycodes[KEY_KPDOT]      = MFB_KB_KEY_KP_DECIMAL;
    s_evdev_keycodes[KEY_KPMINUS]    = MFB_KB_KEY_KP_SUBTRACT;
    s_evdev_keycodes[KEY_KPPLUS]     = MFB_KB_KEY_KP_ADD;
    s_evdev_keycodes[KEY_KP0]        = MFB_KB_KEY_KP_0;
    s_evdev_keycodes[KEY_KP1]        = MFB_KB_KEY_KP_1;
    s_evdev_keycodes[KEY_KP2]        = MFB_KB_KEY_KP_2;
    s_evdev_keycodes[KEY_KP3]        = MFB_KB_KEY_KP_3;
    s_evdev_keycodes[KEY_KP4]        = MFB_KB_KEY_KP_4;
    s_evdev_keycodes[KEY_KP5]        = MFB_KB_KEY_KP_5;
    s_evdev_keycodes[KEY_KP6]        = MFB_KB_KEY_KP_6;
    s_evdev_keycodes[KEY_KP7]        = MFB_KB_KEY_KP_7;
    s_evdev_keycodes[KEY_KP8]        = MFB_KB_KEY_KP_8;
    s_evdev_keycodes[KEY_KP9]        = MFB_KB_KEY_KP_9;
    s_evdev_keycodes[KEY_KPCOMMA]    = MFB_KB_KEY_KP_DECIMAL;
    s_evdev_keycodes[KEY_KPEQUAL]    = MFB_KB_KEY_KP_EQUAL;
    s_evdev_keycodes[KEY_KPENTER]    = MFB_KB_KEY_KP_ENTER;
}

//-------------------------------------
bool
mfb_init_evdev_keycodes(unsigned keycode_offset) {
    if (s_table_built == false) {
        build_evdev_table();
        s_table_built = true;
    }

    for (size_t i = 0; i < MFB_MAX_KEYS; ++i) {
        g_keycodes[i] = MFB_KB_KEY_UNKNOWN;
    }

    for (size_t code = 0; code + keycode_offset < MFB_MAX_KEYS; ++code) {
        g_keycodes[code + keycode_offset] = s_evdev_keycodes[code];
    }

    return true;
}

#else

//-------------------------------------
bool
mfb_init_evdev_keycodes(unsigned keycode_offset) {
    kUnused(keycode_offset);
    return false;
}

#endif
