#pragma once

#include <wayland-client.h>

#include "WindowData.h"
#include "WindowData_Way.h"

//-------------------------------------
extern const struct wl_keyboard_listener g_wayland_keyboard_listener;

//-------------------------------------
void
wayland_clear_keyboard_focus_state(SWindowData *window_data, SWindowData_Way *window_data_specific);
