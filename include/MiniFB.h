#ifndef _MINIFB_H_
#define _MINIFB_H_

#include "MiniFB_enums.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define MFB_RGB(r, g, b) (((unsigned int) r) << 16) | (((unsigned int) g) << 8) | (b)

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Create a window that is used to display the buffer sent into the mfb_update function, returns 0 if fails
struct Window *mfb_open(const char *title, int width, int height);
struct Window *mfb_open_ex(const char *title, int width, int height, int flags);

// Update the display. Input buffer is assumed to be a 32-bit buffer of the size given in the open call
// Will return a negative status if something went wrong or the user want to exit.
UpdateState mfb_update(struct Window *window, void* buffer);

// Close the window
void mfb_close(struct Window *window);

// Set user data
void mfb_set_user_data(struct Window *window, void *user_data);
void *mfb_get_user_data(struct Window *window);

// Set viewport (useful when resize)
bool mfb_set_viewport(struct Window *window, unsigned offset_x, unsigned offset_y, unsigned width, unsigned height);

void mfb_active_callback(struct Window *window, mfb_active_func callback);
void mfb_resize_callback(struct Window *window, mfb_resize_func callback);
void mfb_keyboard_callback(struct Window *window, mfb_keyboard_func callback);
void mfb_char_input_callback(struct Window *window, mfb_char_input_func callback);
void mfb_mouse_button_callback(struct Window *window, mfb_mouse_btn_func callback);
void mfb_mouse_move_callback(struct Window *window, mfb_mouse_move_func callback);
void mfb_mouse_scroll_callback(struct Window *window, mfb_mouse_scroll_func callback);

const char *mfb_get_key_name(Key key);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}

#include "MiniFB_cpp.h"
#endif

#endif
