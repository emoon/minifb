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

// Monitor info returned by mfb_get_monitor_info and mfb_get_window_monitor.
// Logical coordinates use the OS coordinate system (points on macOS, device-independent
// pixels on Windows with DPI awareness, pixels on X11/Wayland).
// Physical coordinates are raw device pixels.
// position (logical_x, logical_y) is relative to the virtual desktop origin.
// physical_x/y are intentionally omitted: the virtual desktop layout is always
// defined in logical units and mixing scales across monitors makes physical
// desktop coordinates ill-defined.
typedef struct mfb_monitor_info {
    int      logical_x, logical_y;            // position in virtual desktop (OS logical units)
    unsigned logical_width,  logical_height;  // size in OS logical units
    unsigned physical_width, physical_height; // size in physical/device pixels
    float    scale_x, scale_y;               // content scale: physical / logical
    bool     is_primary;
    char     name[128];                       // display name, truncated to 127 chars if longer
} mfb_monitor_info;

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

// Monitor enumeration
// Index 0 is always the primary monitor.
// On single-display backends (Web, iOS, Android, DOS) always returns 1.
int                 mfb_get_num_monitors(void);
// Fills out_info with the properties of the monitor at the given index.
// Returns false if index is out of range or out_info is NULL.
bool                mfb_get_monitor_info(unsigned index, mfb_monitor_info *out_info);
// Returns a pointer to the info of the monitor that contains the given window.
// When the window straddles two monitors, returns the one with the largest overlap.
// Returns NULL if window is NULL or the monitor cannot be determined.
// The pointer is valid until the next call to any mfb_*monitor* function on the
// same window, or until the window is destroyed. Copy the struct to keep the data.
mfb_monitor_info *  mfb_get_window_monitor(struct mfb_window *window);

// Open a window on a specific monitor, centered on it.
// monitor_index 0 = primary monitor. Falls back to primary if out of range.
struct mfb_window * mfb_open_on_monitor(const char *title, unsigned width, unsigned height,
                                         unsigned monitor_index);
// Extended version: same as mfb_open_on_monitor plus mfb_window_flags.
// MFB_WF_SIZE_LOGICAL  — width/height are OS logical units (points / CSS px).
// MFB_WF_SIZE_PHYSICAL — width/height are physical device pixels.
// Passing both flags simultaneously is an error: returns NULL and logs a message.
// If neither flag is set the size is in the backend's current native units
// (same behavior as mfb_open_ex — kept for backward compatibility).
struct mfb_window * mfb_open_on_monitor_ex(const char *title, unsigned width, unsigned height,
                                            unsigned flags, unsigned monitor_index);

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

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}

#if !defined(MINIFB_AVOID_CPP_HEADERS)
    #include "MiniFB_cpp.h"
#endif

#endif

// don't want to bleed our deprecation macro
#undef __MFB_DEPRECATED
