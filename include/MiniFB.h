#ifndef _MINIFB_H_
#define _MINIFB_H_

#include "MiniFB_enums.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define MFB_RGB(r, g, b)    (((uint32_t) r) << 16) | (((uint32_t) g) << 8) | ((uint32_t) b)

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Create a window that is used to display the buffer sent into the mfb_update function, returns 0 if fails
struct Window * mfb_open(const char *title, unsigned width, unsigned height);
struct Window * mfb_open_ex(const char *title, unsigned width, unsigned height, unsigned flags);

// Update the display
// Input buffer is assumed to be a 32-bit buffer of the size given in the open call
// Will return a negative status if something went wrong or the user want to exit
// Also updates the window events
UpdateState     mfb_update(struct Window *window, void *buffer);

// Only updates the window events
UpdateState     mfb_update_events(struct Window *window);

// Close the window
void            mfb_close(struct Window *window);

// Set user data
void            mfb_set_user_data(struct Window *window, void *user_data);
void *          mfb_get_user_data(struct Window *window);

// Set viewport (useful when resize)
bool            mfb_set_viewport(struct Window *window, unsigned offset_x, unsigned offset_y, unsigned width, unsigned height);

void            mfb_set_active_callback(struct Window *window, mfb_active_func callback);
void            mfb_set_resize_callback(struct Window *window, mfb_resize_func callback);
void            mfb_set_keyboard_callback(struct Window *window, mfb_keyboard_func callback);
void            mfb_set_char_input_callback(struct Window *window, mfb_char_input_func callback);
void            mfb_set_mouse_button_callback(struct Window *window, mfb_mouse_button_func callback);
void            mfb_set_mouse_move_callback(struct Window *window, mfb_mouse_move_func callback);
void            mfb_set_mouse_scroll_callback(struct Window *window, mfb_mouse_scroll_func callback);

const char *    mfb_get_key_name(Key key);

bool            mfb_is_window_active(struct Window *window);
unsigned        mfb_get_window_width(struct Window *window);
unsigned        mfb_get_window_height(struct Window *window);
int             mfb_get_mouse_x(struct Window *window);             // Last mouse pos X
int             mfb_get_mouse_y(struct Window *window);             // Last mouse pos Y
float           mfb_get_mouse_scroll_x(struct Window *window);      // Mouse wheel X as a sum. When you call this function it resets.
float           mfb_get_mouse_scroll_y(struct Window *window);      // Mouse wheel Y as a sum. When you call this function it resets.
const uint8_t * mfb_get_mouse_button_buffer(struct Window *window); // One byte for every button. Press (1), Release 0. (up to 8 buttons)
const uint8_t * mfb_get_key_buffer(struct Window *window);          // One byte for every key. Press (1), Release 0.

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}

#include "MiniFB_cpp.h"
#endif

#endif
