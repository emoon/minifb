#pragma once

#include "MiniFB_types.h"
#include "MiniFB_macros.h"

#ifdef __cplusplus
extern "C" {
#endif

// Create a window that is used to display the buffer sent into the mfb_update function, returns 0 if fails.
// If title is NULL or empty, a backend-default title ("minifb") is used.
struct mfb_window * mfb_open(const char *title, unsigned width, unsigned height);
struct mfb_window * mfb_open_ex(const char *title, unsigned width, unsigned height, unsigned flags);

// Update the display
// Input buffer is assumed to be a 32-bit buffer of the size given in the open call
// Will return a negative status if something went wrong or the user want to exit
// Also updates the window events
//
// Pixel format:
//   Each pixel is a packed uint32_t.  The expected byte order in memory depends on the
//   platform and is handled automatically by the MFB_RGB / MFB_ARGB macros:
//
//     Desktop / iOS / Web:  BGRA in memory  (B at lowest address)
//     Android (LE):         RGBA in memory  (R at lowest address)
//
//   Always use MFB_RGB(r,g,b) or MFB_ARGB(a,r,g,b) to construct pixel values; these
//   macros expand to the correct bit layout on every platform.
//
//   If you copy pixel data from an external source (image loader, GPU readback, etc.)
//   that always produces RGBA bytes (e.g. stb_image, libpng), you must swizzle R<->B
//   before passing the buffer to mfb_update_ex on non-Android platforms.
//
// On Wayland, mfb_update_ex may block until the compositor grants a frame callback.
// On Android, mfb_update_ex returns MFB_STATE_OK without rendering when ANativeWindow
//   is temporarily unavailable during lifecycle transitions (pause / surface lost).
mfb_update_state    mfb_update(struct mfb_window *window, void *buffer);

mfb_update_state    mfb_update_ex(struct mfb_window *window, void *buffer, unsigned width, unsigned height);

// Only updates the window events
mfb_update_state    mfb_update_events(struct mfb_window *window);

// Close the window
void                mfb_close(struct mfb_window *window);

// Set title
void                mfb_set_title(struct mfb_window *window, const char *title);

// Set user data
void                mfb_set_user_data(struct mfb_window *window, void *user_data);
void *              mfb_get_user_data(struct mfb_window *window);

// Set viewport in drawable coordinates (same units as mfb_get_window_width/height and resize callback)
bool                mfb_set_viewport(struct mfb_window *window, unsigned offset_x, unsigned offset_y, unsigned width, unsigned height);
// Let mfb calculate the best fit from your framebuffer original size (same drawable coordinate units)
bool                mfb_set_viewport_best_fit(struct mfb_window *window, unsigned old_width, unsigned old_height);

// DPI
// [Deprecated]: Probably a better name will be mfb_get_monitor_scale
MFB_DEPRECATED("mfb_get_moniter_dpi deprecated, use mfb_get_monitor_scale instead")
void                mfb_get_monitor_dpi(struct mfb_window *window, float *dpi_x, float *dpi_y);
// Use this instead.
// Returns monitor/content scale as multipliers (1.0 = 100%).
// If window is non-NULL, returns the scale of the monitor containing that window.
// If window is NULL, returns the primary monitor scale on platforms that support it
// (macOS, Windows, X11, iOS, Web, Android), or 1.0 otherwise (Wayland, DOS).
// Output pointers may be NULL.
void                mfb_get_monitor_scale(struct mfb_window *window, float *scale_x, float *scale_y);

// Show/hide cursor
void                mfb_show_cursor(struct mfb_window *window, bool show);

// Callbacks
void                mfb_set_active_callback(struct mfb_window *window, mfb_active_func callback);
void                mfb_set_resize_callback(struct mfb_window *window, mfb_resize_func callback);
void                mfb_set_close_callback(struct mfb_window *window, mfb_close_func callback);
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
int                 mfb_get_mouse_x(struct mfb_window *window);             // Last mouse pos X (Android/iOS touch path may encode pointer id in upper bits)
int                 mfb_get_mouse_y(struct mfb_window *window);             // Last mouse pos Y (Android/iOS touch path may encode pointer id in upper bits)
// Decode a value that may contain an encoded touch pointer id.
// Android/iOS encode touch pointer id in upper bits of mouse X/Y getters.
// Desktop/Web/DOS: pos=combined, id=0.
// Mobile decoding preserves signed position values.
// Output pointers may be NULL.
void                mfb_decode_touch(int combined, int *pos, int *id);
int                 mfb_decode_touch_pos(int combined);               // Extract position from a packed touch value.
int                 mfb_decode_touch_id(int combined);                // Extract pointer id from a packed touch value.
float               mfb_get_mouse_scroll_x(struct mfb_window *window);      // Mouse wheel delta X from the most recent event pump (0.0f if none).
float               mfb_get_mouse_scroll_y(struct mfb_window *window);      // Mouse wheel delta Y from the most recent event pump (0.0f if none).
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
void                mfb_log(const mfb_log_info *info, const char *tag, const char *message, ...);

//-------------------------------------

// Returns the display cutout (notch/punch-hole) insets in pixels - the physical area of
// the screen obstructed by the camera notch or Dynamic Island.  Values are 0 on edges
// that have no physical cutout, or on devices/platforms without a cutout.
// Insets are margins from each edge (not a rectangle): no inset means 0, 0, 0, 0.
// Android: requires API 28+; returns false (all zeros) on older APIs.
// iOS: approximated from UIWindow.safeAreaInsets; bottom is always 0 (home indicator
//   is excluded since it is not a physical obstruction).
// Desktop platforms (macOS, Windows, Linux, Web): true with all zeros for a valid window.
// All output parameters are optional (may be NULL).
bool                mfb_get_display_cutout_insets(struct mfb_window *window, int *left, int *top, int *right, int *bottom);

// Returns the full safe-area insets in pixels - the union of the display cutout area
// AND the system bars (status bar, navigation bar / home indicator).  Useful to know
// how much of the screen edges are reserved by the OS, even when bars are transparent.
// Insets are margins from each edge (not a rectangle): no inset means 0, 0, 0, 0.
// Android API 30+: queries WindowInsets.Type.systemBars()|displayCutout().
// Android API 24-29: falls back to getSystemWindowInset{Top, Right, Bottom, Left}().
// iOS: reads UIWindow.safeAreaInsets (includes notch + status bar + home indicator).
// Desktop platforms (macOS, Windows, Linux, Web): true with all zeros for a valid window.
// All output parameters are optional (may be NULL).
bool                mfb_get_display_safe_insets(struct mfb_window *window, int *left, int *top, int *right, int *bottom);

//-------------------------------------

#ifdef __cplusplus
}

#if !defined(MINIFB_AVOID_CPP_HEADERS)
    #include "MiniFB_cpp.h"
#endif

#endif
