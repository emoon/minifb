#pragma once

#include <MiniFB_internal.h>

//-------------------------------------
typedef struct {
    struct android_app  *app;
    struct mfb_timer    *timer;
} SWindowData_Android;
