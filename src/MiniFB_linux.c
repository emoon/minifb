#if defined(__linux__)

#include <time.h>
#include <MiniFB.h>

extern double   g_timer_frequency;
extern double   g_timer_resolution;

#define kClock      CLOCK_MONOTONIC
//#define kClock      CLOCK_REALTIME

uint64_t 
mfb_timer_tick() {
    struct timespec time;

    if (clock_gettime(kClock, &time) != 0) {
        return 0.0;
    }

    return time.tv_sec * 1e+9 + time.tv_nsec;
}

void 
mfb_timer_init() {
    struct timespec res;

    if (clock_getres(kClock, &res) != 0) {
        g_timer_frequency = 1e+9;
    }
    else {
        g_timer_frequency = res.tv_sec + res.tv_nsec * 1e+9;
    }
    g_timer_resolution = 1.0 / g_timer_frequency;
}

#endif

