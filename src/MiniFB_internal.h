#pragma once

#include "MiniFB_ex.h"

#define kCall(f, ...)   if((f)) (f)(__VA_ARGS__);
#define kUnused(var)    (void) var;

#if defined(__cplusplus)
extern "C" {
#endif

    short int keycodes[512];
    void init_keycodes();
    void keyboard_default(Key key, KeyMod mod, bool isPressed);

    extern mfb_active_func          s_active;
    extern mfb_resize_func          s_resize;
    extern mfb_keyboard_func        s_keyboard;
    extern mfb_char_input_func      s_char_input;
    extern mfb_mouse_btn_func       s_mouse_btn;
    extern mfb_mouse_move_func      s_mouse_move;
    extern mfb_mouse_scroll_func    s_mouse_wheel;

#if defined(__cplusplus)
}
#endif
