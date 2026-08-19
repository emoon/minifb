#ifndef _XOPEN_SOURCE
    #define _XOPEN_SOURCE 700  // for mkstemp, ftruncate, usleep
#endif

#include "MiniFB_timespec.h"

//-------------------------------------
struct timespec
ts_add(const struct timespec a, const struct timespec b) {
    struct timespec out;

    out.tv_sec  = a.tv_sec  + b.tv_sec;
    out.tv_nsec = a.tv_nsec + b.tv_nsec;

    if (out.tv_nsec >= 1000000000L) {
        out.tv_sec += 1;
        out.tv_nsec -= 1000000000L;
    }

    return out;
}

//-------------------------------------
struct timespec
ts_sub_sat(const struct timespec a, const struct timespec b) {
    struct timespec out;

    out.tv_sec  = a.tv_sec  - b.tv_sec;
    out.tv_nsec = a.tv_nsec - b.tv_nsec;

    if (out.tv_nsec < 0) {
        out.tv_sec -= 1;
        out.tv_nsec += 1000000000L;
    }

    if (out.tv_sec < 0) {
        out.tv_sec = 0;
        out.tv_nsec = 0;
    }

    return out;
}

//-------------------------------------
bool
ts_is_less(const struct timespec a, const struct timespec b) {
    if (a.tv_sec != b.tv_sec) {
        return a.tv_sec < b.tv_sec;
    }

    return a.tv_nsec < b.tv_nsec;
}

//-------------------------------------
bool
ts_is_zero(const struct timespec value) {
    return value.tv_sec == 0 && value.tv_nsec == 0;
}

//-------------------------------------
struct timespec
ms_to_ts(double ms) {
    struct timespec out = { 0, 0 };

    if (ms <= 0.0) {
        return out;
    }

    out.tv_sec = (time_t) (ms / 1000.0);
    out.tv_nsec = (long) ((ms - ((double) out.tv_sec * 1000.0)) * 1000000.0);

    if (out.tv_nsec >= 1000000000L) {
        out.tv_sec += out.tv_nsec / 1000000000L;
        out.tv_nsec %= 1000000000L;
    }

    return out;
}
