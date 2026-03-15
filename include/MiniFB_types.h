#pragma once

#include "MiniFB_enums.h"

// Opaque pointers
//-------------------------------------
struct mfb_window;
struct mfb_timer;

// Event callbacks
//-------------------------------------
typedef void(*mfb_active_func)(struct mfb_window *window, bool is_active);
typedef void(*mfb_resize_func)(struct mfb_window *window, int width, int height);
typedef bool(*mfb_close_func)(struct mfb_window *window);
typedef void(*mfb_keyboard_func)(struct mfb_window *window, mfb_key key, mfb_key_mod mod, bool is_pressed);
typedef void(*mfb_char_input_func)(struct mfb_window *window, unsigned int code);
typedef void(*mfb_mouse_button_func)(struct mfb_window *window, mfb_mouse_button button, mfb_key_mod mod, bool is_pressed);
typedef void(*mfb_mouse_move_func)(struct mfb_window *window, int x, int y);
typedef void(*mfb_mouse_scroll_func)(struct mfb_window *window, mfb_key_mod mod, float delta_x, float delta_y);

// Log
//-------------------------------------
typedef struct {
    mfb_log_level level;
    const char   *file;
    const char   *func;
    int           line;
} mfb_log_info;

typedef void (*mfb_log_func)(const mfb_log_info *info, const char *tag, const char *message);
