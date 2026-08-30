#pragma once

#ifdef MINIFB_HAS_LIBDECOR

#include "WindowData.h"
#include "WindowData_Way.h"

//-------------------------------------
// Draws the window frame client-side, for compositors that do not implement
// xdg-decoration. libdecor is opened with dlopen the first time this is called,
// so a build made where it exists still runs where it is missing.
//
// libdecor creates the xdg_surface and the xdg_toplevel itself, so this
// replaces the xdg path instead of extending it. Returning false before the
// surface has been decorated lets the caller fall back to a plain undecorated
// toplevel. Once the surface has a role there is no way back, and the caller
// detects that case by checking libdecor_frame.
//-------------------------------------
bool
wayland_libdecor_create_toplevel(SWindowData *window_data, SWindowData_Way *window_data_specific,
                                 unsigned effective_flags, const char *window_title,
                                 const char *app_id, unsigned width, unsigned height);

//-------------------------------------
// Drops the frame and the context, in that order. The frame owns an xdg_surface
// built from the window's wl_surface, so this must run before that surface is
// destroyed. Safe to call on a window that never used libdecor.
//-------------------------------------
void
wayland_libdecor_release(SWindowData_Way *window_data_specific);

//-------------------------------------
// Dispatches the events libdecor has pending, without blocking. This covers
// the display's default queue and any event source the plugin has of its own.
// Returns 0 when libdecor does not own this window, or a negative value on
// failure. Do not call it between a wl_display_prepare_read and its matching
// read or cancel, because libdecor reads the display itself.
//-------------------------------------
int
wayland_libdecor_dispatch_pending(SWindowData_Way *window_data_specific);

//-------------------------------------
// Sets the frame title. Returns false when libdecor does not own this window,
// so the caller goes through xdg_toplevel instead.
//-------------------------------------
bool
wayland_libdecor_set_title(SWindowData_Way *window_data_specific, const char *title);

#endif
