#pragma once

#include "MiniFB_enums.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// cross-platform deprecation macro, try to use the clean [[deprecated]] if it's avalible, if not, use compiler-specific fallbacks

// C++ [[deprecated]] attribute
#if !defined(__MFB_DEPRECATED) && defined(__has_cpp_attribute)
    #if __has_cpp_attribute(deprecated)
        #define __MFB_DEPRECATED(msg) [[deprecated(msg)]]
    #endif
#endif
// C23 [[deprecated]] attribute
#if !defined(__MFB_DEPRECATED) && defined(__has_c_attribute)
    #if __has_c_attribute(deprecated)
        #define __MFB_DEPRECATED(msg) [[deprecated(msg)]]
    #endif
#endif
// gcc/clang __attribute__ method
#if !defined(__MFB_DEPRECATED) && (defined(__GNUC__) || defined(__clang__))
    #define __MFB_DEPRECATED(msg) __attribute__((deprecated(msg)))
#endif
// msvc __declspec method
#if !defined(__MFB_DEPRECATED) && defined(_MSC_VER)
    #define __MFB_DEPRECATED(msg) __declspec(deprecated(msg))
#endif
// if we can't use any of those, just don't bother
#if !defined(__MFB_DEPRECATED)
    #define __MFB_DEPRECATED(msg)
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef __ANDROID__
    #define MFB_RGB(r, g, b)        (((uint32_t) r) << 16) | (((uint32_t) g) << 8) | ((uint32_t) b)
    #define MFB_ARGB(a, r, g, b)    (((uint32_t) a) << 24) | (((uint32_t) r) << 16) | (((uint32_t) g) << 8) | ((uint32_t) b)
#else
    #ifdef HOST_WORDS_BIGENDIAN
    #define MFB_RGB(r, g, b)     (((uint32_t) r) << 16) | (((uint32_t) g) << 8) | ((uint32_t) b)
    #define MFB_ARGB(a, r, g, b) (((uint32_t) a) << 24) | (((uint32_t) r) << 16) | (((uint32_t) g) << 8) | ((uint32_t) b)
    #else
    #define MFB_ARGB(r, g, b)    (((uint32_t) a) << 24) | (((uint32_t) b) << 16) | (((uint32_t) g) << 8) | ((uint32_t) r)
    #define MFB_RGB(r, g, b)     (((uint32_t) b) << 16) | (((uint32_t) g) << 8) | ((uint32_t) r)
    #endif
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
extern "C" {
#endif

// Create a window that is used to display the buffer sent into the mfb_update function, returns 0 if fails
struct mfb_window * mfb_open(const char *title, unsigned width, unsigned height);
struct mfb_window * mfb_open_ex(const char *title, unsigned width, unsigned height, unsigned flags);

// Update the display
// Input buffer is assumed to be a 32-bit buffer of the size given in the open call
// Will return a negative status if something went wrong or the user want to exit
// Also updates the window events
mfb_update_state    mfb_update(struct mfb_window *window, void *buffer);

mfb_update_state    mfb_update_ex(struct mfb_window *window, void *buffer, unsigned width, unsigned height);

// Only updates the window events
mfb_update_state    mfb_update_events(struct mfb_window *window);

// Close the window
void                mfb_close(struct mfb_window *window);

// Set user data
void                mfb_set_user_data(struct mfb_window *window, void *user_data);
void *              mfb_get_user_data(struct mfb_window *window);

// Set viewport (useful when resize)
bool                mfb_set_viewport(struct mfb_window *window, unsigned offset_x, unsigned offset_y, unsigned width, unsigned height);
// Let mfb to calculate the best fit from your framebuffer original size
bool                mfb_set_viewport_best_fit(struct mfb_window *window, unsigned old_width, unsigned old_height);

// DPI
// [Deprecated]: Probably a better name will be mfb_get_monitor_scale
__MFB_DEPRECATED("mfb_get_moniter_dpi deprecated, use mfb_get_monitor_scale instead")
void                mfb_get_monitor_dpi(struct mfb_window *window, float *dpi_x, float *dpi_y);
// Use this instead
void                mfb_get_monitor_scale(struct mfb_window *window, float *scale_x, float *scale_y);

// Show/hide cursor
void                mfb_show_cursor(struct mfb_window *window, bool show);

// Callbacks
void                mfb_set_active_callback(struct mfb_window *window, mfb_active_func callback);
void                mfb_set_resize_callback(struct mfb_window *window, mfb_resize_func callback);
void                mfb_set_close_callback(struct mfb_window* window, mfb_close_func callback);
void                mfb_set_keyboard_callback(struct mfb_window *window, mfb_keyboard_func callback);
void                mfb_set_char_input_callback(struct mfb_window *window, mfb_char_input_func callback);
void                mfb_set_mouse_button_callback(struct mfb_window *window, mfb_mouse_button_func callback);
void                mfb_set_mouse_move_callback(struct mfb_window *window, mfb_mouse_move_func callback);
void                mfb_set_mouse_scroll_callback(struct mfb_window *window, mfb_mouse_scroll_func callback);

// Getters
const char *        mfb_get_key_name(mfb_key key);

bool                mfb_is_window_active(struct mfb_window *window);
unsigned            mfb_get_window_width(struct mfb_window *window);
unsigned            mfb_get_window_height(struct mfb_window *window);
void                mfb_get_window_size(struct mfb_window *window, unsigned *width, unsigned *height);
unsigned            mfb_get_drawable_offset_x(struct mfb_window *window);
unsigned            mfb_get_drawable_offset_y(struct mfb_window *window);
unsigned            mfb_get_drawable_width(struct mfb_window *window);
unsigned            mfb_get_drawable_height(struct mfb_window *window);
void                mfb_get_drawable_bounds(struct mfb_window *window, unsigned *offset_x, unsigned *offset_y, unsigned *width, unsigned *height);
int                 mfb_get_mouse_x(struct mfb_window *window);             // Last mouse pos X
int                 mfb_get_mouse_y(struct mfb_window *window);             // Last mouse pos Y
float               mfb_get_mouse_scroll_x(struct mfb_window *window);      // Last mouse wheel delta X.
float               mfb_get_mouse_scroll_y(struct mfb_window *window);      // Last mouse wheel delta Y.
const uint8_t *     mfb_get_mouse_button_buffer(struct mfb_window *window); // One byte for every button. Press (1), Release 0. (up to 8 buttons)
const uint8_t *     mfb_get_key_buffer(struct mfb_window *window);          // One byte for every key. Press (1), Release 0.

// FPS
void                mfb_set_target_fps(uint32_t fps);
unsigned            mfb_get_target_fps(void);
bool                mfb_wait_sync(struct mfb_window *window);

// Timer
struct mfb_timer *  mfb_timer_create(void);
void                mfb_timer_destroy(struct mfb_timer *tmr);
void                mfb_timer_reset(struct mfb_timer *tmr);
void                mfb_timer_compensated_reset(struct mfb_timer *tmr);
double              mfb_timer_now(struct mfb_timer *tmr);
double              mfb_timer_delta(struct mfb_timer *tmr);
double              mfb_timer_get_frequency(void);
double              mfb_timer_get_resolution(void);

// Logger
void                mfb_set_logger(mfb_log_func user_logger);
void                mfb_set_log_level(mfb_log_level level);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}

#if !defined(MINIFB_AVOID_CPP_HEADERS)
    #include "MiniFB_cpp.h"
#endif

#endif

// don't want to bleed our deprecation macro
#undef __MFB_DEPRECATED
