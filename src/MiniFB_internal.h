#pragma once

#include <MiniFB.h>
#include "WindowData.h"

#define kCall(func, ...)    if(window_data && window_data->func) window_data->func((struct mfb_window *) window_data, __VA_ARGS__);
#define kUnused(var)        (void) var;

typedef struct mfb_timer {
    int64_t     start_time;
    int64_t     delta_counter;
    uint64_t    time;
} mfb_timer;

#if defined(__cplusplus)
extern "C" {
#endif
    extern short int g_keycodes[512];
    void keyboard_default(struct mfb_window *window, mfb_key key, mfb_key_mod mod, bool isPressed);

    void calc_dst_factor(SWindowData *window_data, uint32_t width, uint32_t height);
    void resize_dst(SWindowData *window_data, uint32_t width, uint32_t height);
    void set_target_fps_aux();

#if defined(__cplusplus)
}
#endif
