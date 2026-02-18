#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "MiniFB_keylist.h"

// Enums
typedef enum {
    STATE_OK             =  0,
    STATE_EXIT           = -1,
    STATE_INVALID_WINDOW = -2,
    STATE_INVALID_BUFFER = -3,
    STATE_INTERNAL_ERROR = -4,
} mfb_update_state;

typedef enum {
    MOUSE_BTN_0, // No mouse button
    MOUSE_BTN_1,
    MOUSE_BTN_2,
    MOUSE_BTN_3,
    MOUSE_BTN_4,
    MOUSE_BTN_5,
    MOUSE_BTN_6,
    MOUSE_BTN_7,
} mfb_mouse_button;
#define MOUSE_LEFT   MOUSE_BTN_1
#define MOUSE_RIGHT  MOUSE_BTN_2
#define MOUSE_MIDDLE MOUSE_BTN_3

typedef enum {
    #define KEY_VALUE(NAME, VALUE, _) NAME = VALUE,
KEY_LIST(KEY_VALUE)
    #undef KEY_VALUE
}  mfb_key;
#define KB_KEY_LAST     KB_KEY_MENU

typedef enum {
    KB_MOD_SHIFT        = 0x0001,
    KB_MOD_CONTROL      = 0x0002,
    KB_MOD_ALT          = 0x0004,
    KB_MOD_SUPER        = 0x0008,
    KB_MOD_CAPS_LOCK    = 0x0010,
    KB_MOD_NUM_LOCK     = 0x0020
} mfb_key_mod;

typedef enum {
    WF_RESIZABLE          = 0x01,
    WF_FULLSCREEN         = 0x02,
    WF_FULLSCREEN_DESKTOP = 0x04,
    WF_BORDERLESS         = 0x08,
    WF_ALWAYS_ON_TOP      = 0x10,
} mfb_window_flags;

typedef enum {
    MFB_LOG_TRACE = 0,
    MFB_LOG_DEBUG,
    MFB_LOG_INFO,
    MFB_LOG_WARNING,
    MFB_LOG_ERROR,
} mfb_log_level;

// Opaque pointer
struct mfb_window;
struct mfb_timer;

// Event callbacks
typedef void(*mfb_active_func)(struct mfb_window *window, bool isActive);
typedef void(*mfb_resize_func)(struct mfb_window *window, int width, int height);
typedef bool(*mfb_close_func)(struct mfb_window* window);
typedef void(*mfb_keyboard_func)(struct mfb_window *window, mfb_key key, mfb_key_mod mod, bool isPressed);
typedef void(*mfb_char_input_func)(struct mfb_window *window, unsigned int code);
typedef void(*mfb_mouse_button_func)(struct mfb_window *window, mfb_mouse_button button, mfb_key_mod mod, bool isPressed);
typedef void(*mfb_mouse_move_func)(struct mfb_window *window, int x, int y);
typedef void(*mfb_mouse_scroll_func)(struct mfb_window *window, mfb_key_mod mod, float deltaX, float deltaY);

// Log
typedef void (*mfb_log_func)(mfb_log_level level, const char *message);