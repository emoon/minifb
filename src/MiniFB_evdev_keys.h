#pragma once

#include <stdbool.h>

//-------------------------------------
// Fills the shared key table from the kernel's evdev key numbers, which name a key by where
// it is instead of by what it prints. Wayland reports those numbers as they come; an X
// server adds eight to them and passes 8 as the offset.
//
// Returns false where evdev does not exist, and the caller then has to name keys some other
// way.
//-------------------------------------
bool mfb_init_evdev_keycodes(unsigned keycode_offset);
