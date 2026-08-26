# Testing the Wayland backend

## Why this exists

MiniFB does not know in advance what a compositor supports. For every protocol global it binds, it picks the lowest of three version numbers:

- the version the compositor advertises,
- the version the linked libwayland provides,
- the version of the headers MiniFB was built with.

If a global is missing, or if the chosen version is too low for some event, MiniFB runs a fallback path instead.

These fallback paths are hard to test. One machine gives you one combination and nothing else. To test another one you would need a different compositor, or a build against older headers.

To solve this, MiniFB reads two environment variables while it binds globals. They let you make one compositor look older or more limited than it is:

| Variable | What it does |
| --- | --- |
| `MINIFB_WAYLAND_FORCE_VERSIONS` | Lowers the version used for one or more interfaces |
| `MINIFB_WAYLAND_DISABLE_GLOBALS` | Hides globals, as if the compositor never offered them |

Both work in every build, not only in debug builds. MiniFB reads them only while it handles `wl_registry.global`, so they cost nothing after the window opens.

## Quick start

```sh
# Use wl_seat version 4 instead of whatever the compositor offers.
MINIFB_WAYLAND_FORCE_VERSIONS="wl_seat=4" ./my_program

# Pretend the compositor has no viewporter.
MINIFB_WAYLAND_DISABLE_GLOBALS="wp_viewporter" ./my_program

# Same, plus a full log of the Wayland traffic.
MINIFB_WAYLAND_FORCE_VERSIONS="wl_seat=4" WAYLAND_DEBUG=client ./my_program 2> trace.txt
```

## `MINIFB_WAYLAND_FORCE_VERSIONS`

The value is a list of `interface=version` pairs, separated by commas. Spaces around the entries and around `=` are ignored.

```sh
MINIFB_WAYLAND_FORCE_VERSIONS="wl_seat=4, wl_output=1" ./my_program
```

### It can only lower the version

You cannot ask for a version higher than the compositor, libwayland, or the headers support. That would be a protocol error. So MiniFB treats your number as an upper limit, not as a target. If the number is already too high to matter, MiniFB says so and ignores it:

```text
WaylandMiniFB: MINIFB_WAYLAND_FORCE_VERSIONS caps wl_seat from v6 to v4.
WaylandMiniFB: MINIFB_WAYLAND_FORCE_VERSIONS asks for wl_output v9 but v3 is already the maximum available; ignored.
```

### Bad input

For any global that MiniFB tries to bind, these mistakes are reported in the log and then ignored: a version of `0`, a missing `=version`, or a value that is not a number. Nothing is dropped without a message.

One case produces no message: an entry for a global the compositor never advertises. MiniFB never gets to that interface, so it cannot notice the entry. Check your spelling if a setting seems to do nothing.

### Which interfaces you can set

You can only name the eight globals that MiniFB binds from the registry. Every other interface is created from one of these, and it takes the version of the object that created it. So you reach those indirectly.

On five of the eight, a version has a real effect. The last column is the number of distinct behaviours, explained in the next section:

| Global you can set | Version also applies to | Behaviours |
| --- | --- | --- |
| `wl_seat` | `wl_pointer`, `wl_keyboard` | 8 |
| `wl_compositor` | `wl_surface` | 4 |
| `wl_output` | - | 4 |
| `xdg_wm_base` | `xdg_surface`, `xdg_toplevel` | 3 |
| `wl_shm` | `wl_shm_pool`, `wl_buffer` | 2 |

On the other three, MiniFB never checks the version, so setting one changes nothing. Hide them with `MINIFB_WAYLAND_DISABLE_GLOBALS` instead:

| Global | Version also applies to |
| --- | --- |
| `zxdg_decoration_manager_v1` | `zxdg_toplevel_decoration_v1` |
| `wp_fractional_scale_manager_v1` | `wp_fractional_scale_v1` |
| `wp_viewporter` | `wp_viewport` |

Note that you cannot lower `wl_pointer` on its own. It comes from `wl_seat.get_pointer`, so it always has the same version as `wl_seat`, and so does `wl_keyboard`.

## How many versions are worth testing

Fewer than you might expect. You do not need to try every version number. You only need the versions where MiniFB's own code changes what it does. Call these the cut points.

For example, `wl_seat` v5, v6 and v7 all run the same code in MiniFB. Testing all three tests the same path three times.

Each range between two cut points is one behaviour class. The tables below list them.

### Where cut points come from

Two places, and you can find both with grep.

First, the events MiniFB handles. Every handler has a comment with its interface and the version that introduced it:

```sh
grep -n "// Protocol:" src/wayland/*.c
```

Second, the requests and events that MiniFB checks at run time:

```sh
grep -nE '>= (WL|XDG|ZXDG|WP)_[A-Z0-9_]+_SINCE_VERSION' src/wayland/*.c
```

In the tables below, the left column is the value you pass, and each row keeps everything from the rows above it. "or none" means you can also leave the variable unset, as long as your compositor reaches that version.

### `wl_seat`: 8 classes

| Set it to | What MiniFB uses |
| --- | --- |
| `1` | core pointer and keyboard events only; no frame grouping, no key repeat |
| `2` | `+ wl_seat.name` |
| `3` | `+ wl_pointer.release`, `wl_keyboard.release`; before this MiniFB destroys them |
| `4` | `+ wl_keyboard.repeat_info`; MiniFB starts repeating held keys |
| `5` to `7` | `+ wl_pointer.frame`, `axis_source`, `axis_stop`, `axis_discrete`, `wl_seat.release` |
| `8` | `+ axis_value120`, and the compositor stops sending `axis_discrete` |
| `9` | `+ axis_relative_direction` |
| `10` or none | + compositor-driven key repeat (`wl_keyboard.key_state` repeated) |

Note that v8 is a switch, not an addition. `axis_discrete` is deprecated from v8 on, so a compositor sends either `axis_discrete` or `axis_value120`, never both. This is why mouse wheel handling needs only three tests: `wl_seat=4` for the old path, `wl_seat=7` for `axis_discrete`, and no setting at all for `axis_value120`.

The v9 handler is empty today, so v8 and v9 behave the same. The class is listed anyway, so it is already there if that handler ever does something.

### `wl_compositor`: 4 classes

| Set it to | What MiniFB uses |
| --- | --- |
| `1` or `2` | `wl_surface.damage` only, which repaints the whole surface |
| `3` | `+ wl_surface.set_buffer_scale` |
| `4` or `5` | `+ wl_surface.damage_buffer`, so only the changed area is repainted |
| `6` or none | `+ preferred_buffer_scale` and `preferred_buffer_transform`; scale no longer comes from `enter` and `leave` |

`wl_surface.offset` arrives in v5, but MiniFB never calls it, so v5 is not a cut point. This is a good example of the rule: cut points come from what MiniFB uses, not from what the protocol adds.

### `wl_output`: 4 classes

| Set it to | What MiniFB uses |
| --- | --- |
| `1` | `geometry` and `mode` only; the integer scale stays at 1 |
| `2` | `+ done` and `scale`, so monitor scale tracking works |
| `3` | `+ wl_output.release`; before this MiniFB destroys the output |
| `4` or none | `+ name` and `description` |

### `xdg_wm_base`: 3 classes

| Set it to | What MiniFB uses |
| --- | --- |
| `1` to `3` | `xdg_toplevel.configure` and `close` only |
| `4` | `+ configure_bounds` |
| `5` or none | `+ wm_capabilities` |

### `wl_shm`: 2 classes

| Set it to | What MiniFB uses |
| --- | --- |
| `1` | MiniFB destroys `wl_shm` on teardown |
| `2` or none | `+ wl_shm.release`, used instead of destroying it |

### The other three: 1 class each

`zxdg_decoration_manager_v1`, `wp_fractional_scale_manager_v1` and `wp_viewporter` have no cut points, so they count as one class each. See "Which interfaces you can set" above.

### Totals

That is 24 behaviour classes across the eight globals.

A run with no settings at all covers the highest class of each global, which is eight of the 24. That leaves 16 runs with a version setting, plus 9 runs with a hidden global: one per global, and one more for `wp_viewporter` and `wp_fractional_scale_manager_v1` together. See "When settings interact" below.

These numbers are the best case. They assume your compositor offers all eight globals at their highest version. Check the log first, because `MINIFB_WAYLAND_FORCE_VERSIONS` can only lower a version, never raise it. If your compositor stops at `wl_seat` v7, then classes 8, 9 and 10 are out of reach on that machine: you need a newer compositor, a newer libwayland, or newer headers.

The same applies to a global your compositor does not offer at all. You can test what happens without it, but not what happens with it.

The full cross product would be about 196,000 runs, but you do not need it. Most of these settings do not affect each other, so you can test them one at a time. There is one exception, described below.

The list also stays this size. A new Wayland version adds a class only when MiniFB starts using something new from it, and then it adds exactly one. The size of the matrix depends on MiniFB's code, not on how fast Wayland grows.

## `MINIFB_WAYLAND_DISABLE_GLOBALS`

This hides globals completely, as if the compositor never advertised them. The value is a list of interface names, separated by commas.

```sh
MINIFB_WAYLAND_DISABLE_GLOBALS="zxdg_decoration_manager_v1,wp_viewporter" ./my_program
```

MiniFB logs every global it hides:

```text
WaylandMiniFB: MINIFB_WAYLAND_DISABLE_GLOBALS hides wp_viewporter (registry id 3).
```

### Optional globals

Five globals are optional. If you hide one, the window still opens, MiniFB logs a warning, and it runs a fallback path:

| Hidden global | Result |
| --- | --- |
| `zxdg_decoration_manager_v1` | no control over server-side decorations |
| `wp_fractional_scale_manager_v1` | integer surface scaling only |
| `wp_viewporter` | integer surface scaling only |
| `wl_seat` | no keyboard and no pointer input |
| `wl_output` | no monitor scale tracking; scale stays at 1 |

### Required globals

The other three are required. Hiding one is still a useful test: `mfb_open_ex()` must fail with a clear message instead of crashing or hanging.

| Hidden global | `mfb_open_ex()` fails with |
| --- | --- |
| `wl_shm` | `compositor does not expose a supported shared memory format` |
| `wl_compositor` | `Wayland compositor interface is unavailable` |
| `xdg_wm_base` | `xdg_wm_base is unavailable; cannot create a toplevel surface` |

## When settings interact

Almost all of these settings are independent, so you can change one at a time. Surface scale is the exception. MiniFB decides the scale with a chain of three options and uses the first one that works:

1. `wp_viewporter` and `wp_fractional_scale_manager_v1` are both present, and the compositor sent a preferred fractional scale: use fractional scaling.
2. Otherwise, if `wl_compositor` is v6 or higher: use `wl_surface.preferred_buffer_scale`.
3. Otherwise: use the highest `wl_output.scale` among the outputs the surface is on. This needs `wl_output` v2 or higher.

Turning off step 1 changes which of steps 2 and 3 runs. So these settings have to be combined:

- `wl_compositor` set to `2`, set to `5`, or left alone
- `wl_output` set to `1`, or left alone
- viewporter and fractional scale both present, or not both

That is 12 runs, and they cover the whole chain.

The last item has two states rather than four because step 1 needs both globals. Hiding one gives the same scale as hiding both. Even so, hide each one separately once, to check that MiniFB logs the right warning.

## Reading the Wayland traffic

`WAYLAND_DEBUG=client` is a libwayland feature. It prints every request and every event to stderr. Together with the two MiniFB variables, this is how you check a change from end to end: the trace shows what the compositor sent, and your program's own output shows what MiniFB made of it.

```sh
MINIFB_WAYLAND_FORCE_VERSIONS="wl_seat=4" WAYLAND_DEBUG=client ./my_program 2> trace.txt
```

`WAYLAND_DEBUG` takes a comma-separated list, and accepts `client`, `server` and `1`. For a client program, `1` means the same as `client`. Use `WAYLAND_DEBUG=client,server` when you run a nested compositor and your program in the same terminal, so you see both sides of the conversation.

In the trace, lines that start with `->` are requests sent by the client. Everything else is an event from the compositor.

Useful checks:

```sh
# Did the version setting work?
grep 'bind(.*"wl_seat"' trace.txt

# Are the events it should have removed really gone?
grep -c 'wl_pointer@[0-9]*\.frame()' trace.txt
grep -c 'axis_discrete' trace.txt

# Follow one interface.
grep 'wl_pointer@' trace.txt
```

Always run the first check before you trust the result. If the trace still contains the events you meant to remove, the variable did not take effect, and your program's output tells you nothing about the path you wanted to test.

### Keep the trace free of colour codes

libwayland colours the trace when stderr is a terminal. Writing to a file turns that off, so the greps above normally work. But `FORCE_COLOR` turns colour back on even when you redirect, and some tools and shell configurations set it. The trace then contains ANSI escape codes, and the greps stop matching without any sign that something is wrong.

Set `NO_COLOR=1` to be sure:

```sh
NO_COLOR=1 WAYLAND_DEBUG=client ./my_program 2> trace.txt
```

To read the trace live instead, do the opposite:

```sh
FORCE_COLOR=1 WAYLAND_DEBUG=client ./my_program 2>&1 | less -R
```

## Other environment variables

### From libwayland

These are all of them. libwayland reads no others.

| Variable | What it does |
| --- | --- |
| `WAYLAND_DEBUG` | Prints the protocol traffic, as described above |
| `NO_COLOR` | Turns off colour in that output |
| `FORCE_COLOR` | Turns colour on, even when the output is not a terminal |
| `WAYLAND_DISPLAY` | Which compositor socket to connect to. Point it at a nested or headless compositor to test against one |
| `XDG_RUNTIME_DIR` | Where that socket lives. MiniFB also creates its shared memory file here when `memfd_create` is not available |
| `WAYLAND_SOCKET` | An already connected file descriptor, passed in by a parent process. Not useful for testing |

### From xkbcommon

xkbcommon turns raw key codes into key symbols and text. It handles layouts, modifiers and dead keys. A Wayland compositor sends the client a keymap, and MiniFB uses xkbcommon to compile it and to work out which character a key produces.

| Variable | What it does |
| --- | --- |
| `XKB_LOG_LEVEL` | How much xkbcommon logs. Useful when a key produces the wrong character, or none |
| `XKB_LOG_VERBOSITY` | How much detail each message carries, from 0 to 10 |

You may also see `XKB_DEFAULT_LAYOUT`, `XKB_DEFAULT_MODEL`, `XKB_DEFAULT_RULES`, `XKB_DEFAULT_VARIANT` and `XKB_DEFAULT_OPTIONS`. They do nothing here. They only apply when a program builds a keymap from rule names, and under Wayland the compositor does that. The client receives a finished keymap over a file descriptor, so setting these on your program changes nothing. Set them on the compositor instead.

## What these variables cannot test

They change the version MiniFB negotiates at run time. They do not change how MiniFB was compiled.

The listener structs are built with `#if defined(..._SINCE_VERSION)` guards, which the preprocessor resolves at build time. If you set `wl_seat` to 4, the compositor stops sending `wl_pointer.frame`, but `.frame = pointer_frame` is still in the listener struct.

So these variables cover the "older compositor" case, which is the one that happens in practice. They do not cover the "built against an older libwayland" case. For that you still need to build with older headers.
