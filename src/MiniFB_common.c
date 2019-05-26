#include "MiniFB.h"

//-------------------------------------
mfb_active_func         g_active_func      = 0x0;
mfb_resize_func         g_resize_func      = 0x0;
mfb_keyboard_func       g_keyboard_func    = 0x0;
mfb_char_input_func     g_char_input_func  = 0x0;
mfb_mouse_btn_func      g_mouse_btn_func   = 0x0;
mfb_mouse_move_func     g_mouse_move_func  = 0x0;
mfb_mouse_scroll_func   g_mouse_wheel_func = 0x0;

void                    *g_user_data       = 0x0;

//-------------------------------------
void mfb_active_callback(mfb_active_func callback) {
    g_active_func = callback;
}

//-------------------------------------
void mfb_resize_callback(mfb_resize_func callback) {
    g_resize_func = callback;
}

//-------------------------------------
void mfb_keyboard_callback(mfb_keyboard_func callback) {
    g_keyboard_func = callback;
}

//-------------------------------------
void mfb_char_input_callback(mfb_char_input_func callback) {
    g_char_input_func = callback;
}

//-------------------------------------
void mfb_mouse_button_callback(mfb_mouse_btn_func callback) {
    g_mouse_btn_func = callback;
}

//-------------------------------------
void mfb_mouse_move_callback(mfb_mouse_move_func callback) {
    g_mouse_move_func = callback;
}

//-------------------------------------
void mfb_mouse_scroll_callback(mfb_mouse_scroll_func callback) {
    g_mouse_wheel_func = callback;
}

//-------------------------------------
void mfb_set_user_data(void *user_data) {
    g_user_data = user_data;
}