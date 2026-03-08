#pragma once

#include <stddef.h>
#include <MiniFB.h>
#include "WindowData.h"

#define kCall(func, ...)    if (window_data && window_data->func) window_data->func((struct mfb_window *) window_data, __VA_ARGS__);
#define kUnused(var)        (void) var;

typedef struct mfb_timer {
    int64_t     start_ticks;
    int64_t     last_delta_ticks;
    uint64_t    accumulated_ticks;
} mfb_timer;

#if defined(__cplusplus)
extern "C" {
#endif
    extern void mfb_log(mfb_log_level level, const char *message, ...);

    extern short int g_keycodes[MFB_MAX_KEYS];
    void keyboard_default(struct mfb_window *window, mfb_key key, mfb_key_mod mod, bool is_pressed);

    bool calculate_buffer_layout(uint32_t width, uint32_t height, uint32_t *stride_out, size_t *total_bytes_out);
    bool mfb_validate_viewport(const SWindowData *window_data,
                               unsigned offset_x, unsigned offset_y,
                               unsigned width, unsigned height,
                               const char *backend_name);
    void calc_dst_factor(SWindowData *window_data, uint32_t width, uint32_t height);
    void resize_dst(SWindowData *window_data, uint32_t width, uint32_t height);
    void set_target_fps_aux();

#if defined(__cplusplus)
}
#endif
