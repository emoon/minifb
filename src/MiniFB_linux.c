#if defined(__linux__) || defined(linux)

#ifndef _XOPEN_SOURCE
    #define _XOPEN_SOURCE 700
#endif

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

    return (uint64_t) time.tv_sec * 1000000000ULL + (uint64_t) time.tv_nsec;
}

void
mfb_timer_init() {
    g_timer_frequency = 1e+9;
    g_timer_resolution = 1.0 / g_timer_frequency;
}

#endif
