# Testing the X11 backend

## Why this exists

MiniFB names a physical key in one of two ways on X11, and which one it uses depends on the
server it is talking to.

Current X servers on Linux, Xorg and Xwayland alike, number a key the way the kernel does,
plus eight. That number says where the key is, so MiniFB can name it with the same table the
Wayland backend uses, and both report the same key for the same position. XQuartz, some VNC
servers and Xorg from before the evdev driver number keys their own way. There MiniFB falls
back to naming a key after the keysym it produces, which depends on the layout: the key
between left Shift and Z reports `MFB_KB_KEY_WORLD_1` instead of `MFB_KB_KEY_WORLD_2`, and on
a layout like AZERTY or Dvorak the letter keys move with the layout.

The fallback is hard to test. A normal Linux machine only offers the first path, so checking
the second one means finding an old or unusual server.

To solve this, MiniFB reads one environment variable while it builds the key table:

| Variable | What it does |
| --- | --- |
| `MINIFB_X11_DISABLE_EVDEV_KEYCODES` | Ignores the evdev key numbering, as if the server did not use it |

Any non-empty value turns it on. It works in every build, not only in debug builds, and
MiniFB reads it once per window, while the window opens.

## Quick start

```sh
# Name keys by position, which is what happens by default on Linux.
./keyboard_events_test

# Name keys after the keysym they produce, as on a server without evdev keycodes.
MINIFB_X11_DISABLE_EVDEV_KEYCODES=1 ./keyboard_events_test
```

Raise the log level to `MFB_LOG_DEBUG` to see which path MiniFB took. It logs one line from
`init_keycodes` saying whether it named keys by position or by keysym.

## What to compare

Run `keyboard_events_test` both ways and diff the summaries. Only one line should change:

```
  ISO key token            World_2     # default
  ISO key token            World_1     # with the variable set
```

Everything else has to match, including the text. The token names the physical key; the
characters come from the layout, so the ISO key still produces `<` and `>` either way.

## Text input

The text pipeline is chosen at build time instead, so there is no variable for it. See the
`MINIFB_X11_USE_IME` option in `CMakeLists.txt` and the keyboard section of
`docs/api/api-contract-notes.md`.
