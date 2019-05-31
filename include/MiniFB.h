#ifndef _MINIFB_H_
#define _MINIFB_H_

#include "MiniFB_enums.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define MFB_RGB(r, g, b) (((unsigned int)r) << 16) | (((unsigned int)g) << 8) | b

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Create a window that is used to display the buffer sent into the mfb_update function, returns 0 if fails
int mfb_open(const char* name, int width, int height);
int mfb_open_ex(const char* name, int width, int height, int flags);

// Update the display. Input buffer is assumed to be a 32-bit buffer of the size given in the open call
// Will return -1 when ESC key is pressed (later on will return keycode and -1 on other close signal) 
int mfb_update(void* buffer);

// Close the window
void mfb_close();

// Set user data
void mfb_set_user_data(void *user_data);

// Set viewport (useful when resize)
bool mfb_set_viewport(unsigned offset_x, unsigned offset_y, unsigned width, unsigned height);

// Event callbacks
typedef void(*mfb_active_func)(void *user_data, bool isActive);
typedef void(*mfb_resize_func)(void *user_data, int width, int height);
typedef void(*mfb_keyboard_func)(void *user_data, Key key, KeyMod mod, bool isPressed);
typedef void(*mfb_char_input_func)(void *user_data, unsigned int code);
typedef void(*mfb_mouse_btn_func)(void *user_data, MouseButton button, KeyMod mod, bool isPressed);
typedef void(*mfb_mouse_move_func)(void *user_data, int x, int y);
typedef void(*mfb_mouse_scroll_func)(void *user_data, KeyMod mod, float deltaX, float deltaY);

void mfb_active_callback(mfb_active_func callback);
void mfb_resize_callback(mfb_resize_func callback);
void mfb_keyboard_callback(mfb_keyboard_func callback);
void mfb_char_input_callback(mfb_char_input_func callback);
void mfb_mouse_button_callback(mfb_mouse_btn_func callback);
void mfb_mouse_move_callback(mfb_mouse_move_func callback);
void mfb_mouse_scroll_callback(mfb_mouse_scroll_func callback);

const char *mfb_get_key_name(Key key);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}

#include "MiniFB_cpp.h"
#endif

#endif
