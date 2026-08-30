#pragma once

#include "WindowData.h"
#include "WindowData_Way.h"

//-------------------------------------
// Adapts slot 0 to the size the compositor chose and attaches it, which is what
// makes the surface visible. Every shell path calls this from its first
// configure, the only point where the final size is known.
//-------------------------------------
void
wayland_attach_initial_buffer(SWindowData *window_data, SWindowData_Way *window_data_specific);

//-------------------------------------
// Declares the whole surface opaque. The buffer format has no alpha channel,
// but a compositor that is not told this may still blend the surface with what
// is behind it. Every shell path must call this whenever the surface changes size,
// because the region is given in surface-local coordinates.
//-------------------------------------
void
wayland_update_opaque_region(SWindowData *window_data, SWindowData_Way *window_data_specific);
