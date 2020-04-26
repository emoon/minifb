#pragma once

#include "MiniFB.h"
#include "MiniFB_enums.h"

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
    void init_keycodes(void);
    void keyboard_default(struct mfb_window *window, mfb_key key, mfb_key_mod mod, bool isPressed);

#if defined(__cplusplus)
}
#endif
