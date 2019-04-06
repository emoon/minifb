#pragma once

#include "MiniFB_ex_enums.h"

#if defined(__cplusplus)
extern "C" {
#endif

    int mfb_open_ex(const char* name, int width, int height, int flags);
    eBool mfb_set_viewport(unsigned offset_x, unsigned offset_y, unsigned width, unsigned height);

    typedef void(*mfb_active_func)(eBool isActive);
    typedef void(*mfb_resize_func)(int width, int height);
    typedef void(*mfb_keyboard_func)(eKey key, eKeyMod mod, eBool isPressed);
    typedef void(*mfb_char_input_func)(unsigned int code);
    typedef void(*mfb_mouse_btn_func)(eMouseButton button, eKeyMod mod, eBool isPressed);
    typedef void(*mfb_mouse_move_func)(int x, int y);
    typedef void(*mfb_mouse_scroll_func)(eKeyMod mod, float deltaX, float deltaY);

    void mfb_active_callback(mfb_active_func callback);
    void mfb_resize_callback(mfb_resize_func callback);
    void mfb_keyboard_callback(mfb_keyboard_func callback);
    void mfb_char_input_callback(mfb_char_input_func callback);
    void mfb_mouse_button_callback(mfb_mouse_btn_func callback);
    void mfb_mouse_move_callback(mfb_mouse_move_func callback);
    void mfb_mouse_scroll_callback(mfb_mouse_scroll_func callback);

#if defined(__cplusplus)
}
#endif
