#include "MiniFB_ex.h"

//-------------------------------------
mfb_active_func         s_active      = 0x0;
mfb_resize_func         s_resize      = 0x0;
mfb_keyboard_func       s_keyboard    = 0x0;
mfb_char_input_func     s_char_input  = 0x0;
mfb_mouse_btn_func      s_mouse_btn   = 0x0;
mfb_mouse_move_func     s_mouse_move  = 0x0;
mfb_mouse_scroll_func   s_mouse_wheel = 0x0;

//-------------------------------------
void mfb_active_callback(mfb_active_func callback) {
    s_active = callback;
}

//-------------------------------------
void mfb_resize_callback(mfb_resize_func callback) {
    s_resize = callback;
}

//-------------------------------------
void mfb_keyboard_callback(mfb_keyboard_func callback) {
    s_keyboard = callback;
}

//-------------------------------------
void mfb_char_input_callback(mfb_char_input_func callback) {
    s_char_input = callback;
}

//-------------------------------------
void mfb_mouse_button_callback(mfb_mouse_btn_func callback) {
    s_mouse_btn = callback;
}

//-------------------------------------
void mfb_mouse_move_callback(mfb_mouse_move_func callback) {
    s_mouse_move = callback;
}

//-------------------------------------
void mfb_mouse_scroll_callback(mfb_mouse_scroll_func callback) {
    s_mouse_wheel = callback;
}


