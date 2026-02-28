#include <MiniFB.h>
#include "MiniFB_internal.h"
#include <stdlib.h>

//-------------------------------------
double      g_timer_frequency;
double      g_timer_resolution;
double      g_time_for_frame = 1.0 / 60.0;
bool        g_use_hardware_sync = false;

//-------------------------------------
extern uint64_t mfb_timer_tick(void);
extern void     mfb_timer_init(void);

// Sets the target frame rate (FPS) and calculates the required time per frame
//-------------------------------------
void
mfb_set_target_fps(uint32_t fps) {
    if (fps <= 0) {
        g_time_for_frame  = 0;
    }
    else {
        g_time_for_frame = 1.0 / fps;
    }

    set_target_fps_aux();
}

// Returns the currently configured target FPS
//-------------------------------------
unsigned
mfb_get_target_fps() {
    if (g_time_for_frame == 0) {
        return 0;
    }

    return (unsigned) (1.0 / g_time_for_frame);
}

// Creates and initializes a new timer. First call also initializes the global timing system
//-------------------------------------
struct mfb_timer *
mfb_timer_create() {
    static int  once = 1;   // Not thread safe
    mfb_timer   *tmr;

    if (once) {
        once = 0;
        mfb_timer_init();
    }

    tmr = malloc(sizeof(mfb_timer));
    mfb_timer_reset(tmr);

    return tmr;
}

// Frees the memory of a timer
//-------------------------------------
void
mfb_timer_destroy(struct mfb_timer *tmr) {
    if (tmr != NULL) {
        free(tmr);
    }
}

// Resets a timer, setting its time to zero and updating its starting point
//-------------------------------------
void
mfb_timer_reset(struct mfb_timer *tmr) {
    if (tmr == NULL)
        return;

    tmr->start_ticks       = mfb_timer_tick();
    tmr->last_delta_ticks  = tmr->start_ticks;
    tmr->accumulated_ticks = 0;
}

// Returns the total elapsed time since the timer was created or reset (in seconds)
//-------------------------------------
double
mfb_timer_now(struct mfb_timer *tmr) {
    uint64_t    current_ticks;

    if (tmr == NULL)
        return 0.0;

    current_ticks           = mfb_timer_tick();
    tmr->accumulated_ticks += (current_ticks - tmr->start_ticks);
    tmr->start_ticks        = current_ticks;

    return tmr->accumulated_ticks * g_timer_resolution;
}

// Returns the elapsed time since the last call to this function (delta time in seconds)
//-------------------------------------
double
mfb_timer_delta(struct mfb_timer *tmr) {
    int64_t     current_ticks;
    uint64_t    delta_ticks;

    if (tmr == NULL)
        return 0.0;

    current_ticks         = mfb_timer_tick();
    delta_ticks           = (current_ticks - tmr->last_delta_ticks);
    tmr->last_delta_ticks = current_ticks;

    return delta_ticks * g_timer_resolution;
}

// Returns the system timer frequency (ticks per second)
//-------------------------------------
double
mfb_timer_get_frequency() {
    return g_timer_frequency;
}

// Returns the timer resolution (seconds per tick)
//-------------------------------------
double
mfb_timer_get_resolution() {
    return g_timer_resolution;
}

// Returns the target time per frame (in seconds)
//-------------------------------------
double
mfb_timer_get_time_per_frame() {
    return g_time_for_frame;
}

// Returns the target ticks per frame (calculated from time per frame)
//-------------------------------------
int64_t
mfb_timer_get_ticks_per_frame() {
    return (int64_t) (g_time_for_frame * g_timer_frequency);
}

// Resets a timer with accumulated error compensation for more stable frame timing
//-------------------------------------
void
mfb_timer_compensated_reset(struct mfb_timer *tmr) {
    static const double ERROR_THRESHOLD    = 0.01; // % threshold
    static const double MAX_CORRECTION     = 0.50; // % max correction
    static const double MAX_FRAME_TIME     = 2.0;  // 5x target frame time = fallback threshold
    static int64_t accumulated_error_ticks = 0;

    if (g_time_for_frame == 0) {
        mfb_timer_reset(tmr);
        return;
    }

    if (tmr == NULL) {
        return;
    }

    // Work entirely with ticks for maximum precision
    int64_t  target_ticks_per_frame = mfb_timer_get_ticks_per_frame();
    int64_t  threshold_ticks        = (int64_t) (target_ticks_per_frame * ERROR_THRESHOLD);
    int64_t  max_frame_ticks        = (int64_t) (target_ticks_per_frame * MAX_FRAME_TIME);
    uint64_t current_ticks          = mfb_timer_tick();

    // Calculate actual frame time in ticks
    int64_t actual_frame_ticks = current_ticks - tmr->last_delta_ticks;

    // If frame time is excessive (breakpoint, pause, etc.), fallback to normal reset
    if (actual_frame_ticks > max_frame_ticks) {
        accumulated_error_ticks = 0;  // Clear accumulated error
        mfb_timer_reset(tmr);         // Use normal reset
        return;
    }

    int64_t error_ticks = actual_frame_ticks - target_ticks_per_frame;

    // Accumulate error
    accumulated_error_ticks += error_ticks;

    // Apply correction if error exceeds threshold
    int64_t correction_ticks = 0;
    if (llabs(accumulated_error_ticks) > threshold_ticks) {
        correction_ticks = (int64_t) (accumulated_error_ticks * MAX_CORRECTION);
        accumulated_error_ticks -= correction_ticks;
    }

    // Reset with compensation
    tmr->start_ticks       = current_ticks - correction_ticks;
    tmr->last_delta_ticks  = tmr->start_ticks;
    tmr->accumulated_ticks = 0;
}
