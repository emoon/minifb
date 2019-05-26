#pragma once

#include "MiniFB.h"

extern void *g_user_data;

#define kCall(f, ...)   if((f)) (f)(g_user_data, __VA_ARGS__);
#define kUnused(var)    (void) var;

#if defined(__cplusplus)
extern "C" {
#endif

    short int keycodes[512];
    void init_keycodes();
    void keyboard_default(void *user_data, Key key, KeyMod mod, bool isPressed);

    extern mfb_active_func          g_active_func;
    extern mfb_resize_func          g_resize_func;
    extern mfb_keyboard_func        g_keyboard_func;
    extern mfb_char_input_func      g_char_input_func;
    extern mfb_mouse_btn_func       g_mouse_btn_func;
    extern mfb_mouse_move_func      g_mouse_move_func;
    extern mfb_mouse_scroll_func    g_mouse_wheel_func;

#if defined(__cplusplus)
}
#endif
