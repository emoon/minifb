#pragma once

#include <MiniFB_internal.h>

//-------------------------------------
typedef struct {
    struct android_app  *app;
    uint32_t            mouse_button_state;
    bool                pending_hover_exit;
} SWindowData_Android;
