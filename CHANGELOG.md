# Changelog

All notable changes to this project are documented in this file.

## [Unreleased]

### Added

- **Cursor enter and leave**: `mfb_set_mouse_enter_callback` reports the cursor entering or leaving the window content area, and `mfb_is_mouse_inside` returns the same state. It fires only on a real crossing, and while a button is held the window keeps the pointer, so dragging out reports no leave until the button is released. Android needs a mouse, trackpad or hover capable stylus. iOS and DOS never fire it, but on DOS `mfb_is_mouse_inside` is true when a mouse driver is present.
- **Web `MFB_WF_RESIZABLE`**: the canvas follows its CSS layout box scaled by `devicePixelRatio`, so the page must give it a relative size. Without the flag the drawing buffer stays pinned to the framebuffer size, as before.
- **DOS mouse wheel**: the wheel is read from the mouse driver through the CuteMouse API, so `mfb_set_mouse_scroll_callback` works there without giving up the arrow keys.
- **X11 text input through xkbcommon**: dead keys and Compose sequences now come from the system Compose file, the same source the Wayland backend uses, instead of a built-in table that only knew five accents over Latin-1 vowels. The library is optional and found with `pkg-config`. `-DMINIFB_X11_USE_IME=ON` puts an X11 input method (XIM) in front of it, and without xkbcommon that input method is used on its own, as before.
- **Web text input through a hidden text field**: dead keys, input method composition and characters outside the Basic Multilingual Plane now reach `mfb_set_char_input_callback`.
- Two interactive tests, `tests/mouse_events.c` and `tests/keyboard_events.c`, that walk a person through the mouse and keyboard contracts and end with a summary meant to be compared between backends with `diff`.
- Two testing documents: `docs/testing-x11.md`, on exercising the X11 key naming fallback without hunting for an unusual server, and `docs/testing-dos.md`, on running the interactive tests under DOSBox-x.

### Changed

- **Frame pacing on Web and DOS**: `mfb_wait_sync` honours the target frame rate on both, as every other backend does. DOS returned as soon as it had pumped events and Web yielded to the browser once, so `mfb_set_target_fps` did nothing on either. Call `mfb_set_target_fps(0)` to get the old free running behaviour back.
- **Android external mouse buttons**: a mouse click reported the touch pointer id, always `0`, in the `button` argument. It now reports the real button, like the desktop backends. The device moves to the packed pointer id: a mouse or hovering stylus reports the new `MFB_POINTER_ID_MOUSE`, a finger a lower id. Touch is unchanged.
- **DOS keypad keys**: with Num Lock off the keypad reports Home, the arrows, Page Up and the rest, which is what a program written for DOS expects; with Num Lock on it reports the keypad keys. Define `MINIFB_DOS_KEYPAD_POSITIONAL` to always report the physical keypad key, as the other backends do.

### Fixed

The mouse contract is the same on every backend now. Each row is what an application used to receive:

| Backend | Divergence |
| --- | --- |
| X11 | Every wheel notch twice, because it arrives as a button press followed by a release |
| Windows | `MFB_MOUSE_BTN_5` and `MFB_MOUSE_BTN_6` for the side buttons, where the others report `MFB_MOUSE_BTN_4` and `MFB_MOUSE_BTN_5` |
| Web | Both wheel axes with the opposite sign; MiniFB counts up and left as positive |
| Android | The horizontal wheel axis with the opposite sign, for the same reason |
| Web, Wayland, Android, macOS | A scroll callback with both axes at zero, from a kinetic scroll ending or a trackpad gesture only changing phase |

The keyboard contract is the same everywhere too:

| Area | What changed |
| --- | --- |
| Modifiers | They are built from both sources, the keys held and what the platform reports. Releasing one Shift while the other was held reported no Shift, and AltGr reported no Alt on the layouts where the system does not count it as one |
| Losing the focus | Every key still held is released, one callback each, leaving only the locks in the modifiers. The real release goes to whatever window took the focus and never arrives, so an application that tracked state from callbacks kept the key held for ever. Only Wayland did this before |
| Gaining the focus | Windows and X11 rebuild the key buffer from the keyboard itself, so a key pressed while another window had the focus is in it. X11 also does this when the window opens |
| Callback order | The physical key is reported before the character it produces. Wayland had the two the other way round |
| Text callback | Control characters, DEL, the C1 range and lone surrogates no longer reach `mfb_set_char_input_callback`. On Windows, Enter, Tab and Backspace arrived there as `\r`, `\t` and `\b`. Those keys are still reported by the keyboard callback |

Two more that affect every backend:

- **The window title is UTF-8.** A title that is not valid UTF-8 is now reported once in the log and its non-ASCII bytes dropped, instead of mojibake on Windows, no title at all on macOS, and a protocol error that closes the connection on Wayland. Windows creates a Unicode window now, so a valid non-ASCII title shows correctly, and X11 also sets `_NET_WM_NAME` for current window managers.
- **Frame pacing could stop for good.** Elapsed time was measured with an unsigned subtraction, and the compensated reset at the end of a frame can leave the start of the next one in the future. One frame that ran long then measured as almost 2^64 ticks, and every frame after it skipped its wait.

Per backend:

| Backend | Fix |
| --- | --- |
| Windows | A mouse button released outside the window is delivered; dragging out used to leave it marked as held for ever |
| Windows | AltGr no longer leaks a left Control release that the application never saw pressed |
| Windows | Shift and the Windows key no longer stay held when the system swallows their key up, as it does after Win+V |
| Windows | Print Screen reports a press; Windows only sends the key up for it |
| Windows | Text outside ASCII arrives whole, because the window and its message loop are Unicode now |
| Windows | Closing a window with the cursor hidden no longer leaves the cursor hidden |
| Windows | A null backend window state could be dereferenced in `WM_MOUSEMOVE` and `WM_SIZE` |
| macOS | One wheel notch reports the same amount as elsewhere; `NSEvent` gives a fraction of a line per notch, a tenth of one on some devices. Trackpad deltas are unchanged |
| X11 | A key is named by where it is on the servers that number keys the evdev way, every current Xorg and Xwayland, so a Dvorak or AZERTY layout no longer moves the letters. Other servers keep naming a key after the keysym it produces, and `MINIFB_X11_DISABLE_EVDEV_KEYCODES` forces that fallback |
| X11 | Auto-repeat no longer arrives as a release followed by a press |
| X11 | Caps Lock and Num Lock report the state they leave behind, not the previous one |
| X11 | A grab, from the window menu, alt-tab or dragging the window, is no longer reported as a focus change |
| X11 | Entering the window at the end of another application's drag is reported; `mfb_is_mouse_inside` stayed false until the pointer left and came back |
| Wayland | Losing the pointer releases the held buttons before reporting the leave, not after |
| Web | The getters match the callback being delivered, because the state travels with each queued event |
| Web | The active callback follows the real keyboard focus, which is also what decides whether keys reach the window |
| Web | A key or mouse release with no press behind it is no longer delivered |
| Web | A modifier release the browser reports without a key code is recovered; on Windows it reports no code at all once both Shift keys are down |
| Web | AltGr arrives as one key, as on the Windows backend |
| Web | `IntlBackslash`, the key between the left Shift and Z, was missing from the key map |
| iOS | Multi-touch works; `UIView` defaults `multipleTouchEnabled` to `NO`, so UIKit delivered one contact at a time |
| DOS | The arrow keys work; extended Up and Down were always turned into wheel scroll, which now needs `MINIFB_DOS_WHEEL_FROM_ARROW_KEYS` |
| DOS | Extended keys report their own token instead of the base scancode they borrow: right Ctrl and Alt, the navigation block, the Windows keys, Menu, keypad Enter and divide, and Pause. They also typed the text of that base key, so Home produced a `7` |
| DOS | The key between the left Shift and Z reports `MFB_KB_KEY_WORLD_2` instead of `MFB_KB_KEY_UNKNOWN` |
| DOS | Num Lock reaches the modifiers, and releasing one Shift or Control no longer clears the bit while the other side is held |
| DOS | A scancode with no token produces no keyboard callback; its text, when it has any, is still delivered |
| DOS | The mouse position is reported in window units; VESA can pick a mode twice the size the application asked for |

Build:

- GCC builds are warning free again. Casting the result of `GetProcAddress` and `wglGetProcAddress` to a concrete signature trips `-Wcast-function-type`. The casts now go through `void (*)(void)`, as GLFW and SDL do.
- The DJGPP build names its executables so they fit the DOS 8.3 limit: `keyevent.exe`, `mouseevt.exe`, `inpevent.exe` and so on. The CMake target names do not change, and `docs/testing-dos.md` lists them all.

## [0.13.0]

### Added

- **Wayland client-side decorations**: when the compositor does not implement `xdg-decoration`, MiniFB now draws the window frame with libdecor instead of leaving the window bare. libdecor is opened with `dlopen` the first time a window needs it, so it is never a link-time dependency: a binary built on a machine that has libdecor still runs on one that does not. If the library, any symbol it needs, or the compositor support is missing, the window falls back to a plain undecorated toplevel and MiniFB says so in the log. CMake finds the headers with `pkg-config` and reports which case applies, including the common one where the runtime library is installed but the development package is not.

### Changed

- Wayland: the surface now declares its opaque region, and refreshes it whenever the surface changes size. The buffer format has no alpha channel, so the whole surface is opaque, but a compositor that is not told this may still blend it. Declaring it lets the compositor skip that work and discard whatever the window covers. GLFW and SDL do the same.

### Known issues

- Wayland on WSLg: maximizing a window that libdecor decorates leaves the drop shadow of the floating window drawn over the maximized one, at its previous size and position. MiniFB removes the shadow correctly and the compositor confirms it, so this is not specific to MiniFB: the same artifact appears with GLFW and was reported against FLTK in [microsoft/wslg#914](https://github.com/microsoft/wslg/issues/914). It does not happen on native Linux compositors. Resizing the window by hand clears it.

## [0.12.0]

### Added

- **Log level from the environment**: `MINIFB_LOG_LEVEL` sets the log threshold by name (`trace`, `debug`, `info`, `warning`, `error`) without touching the code. It deliberately wins over `mfb_set_log_level()`, so you can get more output from a program you cannot rebuild. An unknown value is reported as an error and then ignored, leaving the threshold where the program left it.
- **Wayland fallback testing**: `MINIFB_WAYLAND_FORCE_VERSIONS` lowers the version MiniFB binds for one or more protocol globals, and `MINIFB_WAYLAND_DISABLE_GLOBALS` hides globals as if the compositor never advertised them. Both work in every build and are read only while globals are bound, so a single machine can exercise fallback paths that would otherwise need another compositor.
- Added `docs/wayland-testing.md`: the interfaces each variable accepts, which versions are worth testing and what each one covers, the related variables from libwayland and xkbcommon, and what this approach cannot test.

### Changed

- Wayland mouse wheel: the continuous `wl_pointer.axis` value is now divided by the ratio Weston uses, which SDL and GLFW follow too. One notch reports `1.0`, like the other backends. This only affects compositors that fall back to the continuous value. Those that send `axis_value120` or `axis_discrete` already reported `1.0`.
- Every Wayland listener callback now carries a comment with its protocol interface and the version that introduced it, so the version-dependent paths can be found with grep.
- Wayland: `xdg_toplevel.configure` now logs its state array at DEBUG (`activated`, `suspended`, `maximized`, `resizing`, ...) instead of discarding it.
- Wayland: `wl_keyboard.repeat_info` now logs the advertised rate and delay at DEBUG. It also reports when client-side repeat is off, which happens when the compositor drives it instead.
- Wayland: MiniFB now says when a window will have no frame. It logs the negotiated decoration mode at DEBUG. A client-side answer to a server-side request also raises a warning. The warning for a missing decoration manager now states the same fact instead of describing the protocol.
- Wayland: a failed dispatch now reports what libwayland knows about the connection. A protocol error names the interface, the object and the error code. Any other failure reports the system error. Before, every case printed the same generic message.
- Rewrote `README.md` in plainer English and corrected stale details: `mfb_update` return values, ESC handling, the macOS Metal default, the X11 default on Linux, the Wayland dependencies, and Web monitor scale and cursor support. Added a CMake options table, and replaced the per-platform "Beta" labels with what each backend actually supports.

### Fixed

- Wayland: `wl_pointer.axis_discrete` no longer marks an axis as valid when the compositor reports a discrete step of `0`.
- Wayland: a protocol error is no longer reported as `EPIPE`. The dispatch helpers gave up as soon as the flush failed. They never read the error the compositor had queued before closing the socket. They now continue to the read, as libwayland does in the function they derive from.
- Wayland: key repeat now runs at the rate the compositor asks for. MiniFB computed the next deadline from the moment the previous repeat fired. Every interval then rounded up to the next poll. On KWin at 25 cps, the measured rate was 19.18 before the fix and 25.06 after.
- Wayland: a hidden window no longer uses a whole CPU core. The frame throttle's wait budget belonged to the frame callback, and nothing refreshed it once it expired. Every later update then skipped without waiting. An application with no other frame pacing had nothing left to pace it. The budget now belongs to each update call. On KWin, a minimized window with no target FPS dropped from 100% of a core to 3.8%, and the visible frame rate did not change.

## [0.11.0]

### Changed

- **Wayland backend modernization**: promoted Wayland to a first-class desktop backend alongside Windows, macOS, and X11, with reworked SHM presentation, event dispatch, frame pacing, scaling, input, and resource lifecycle handling.
- Updated the bundled Wayland protocol bindings to 1.49 and added viewporter-based fractional and per-surface HiDPI scaling.
- **CMake restructure**: the build now checks the prefixed option names internally, adds `MINIFB_BUILD_EXAMPLES`, and uses `cmake_dependent_option` where one option depends on another. It also fixes the exported package config, the generated version header, and the Emscripten, iOS and macOS build paths. The unprefixed names were already deprecated in 0.9.3 and keep working.

### Fixed

- Improved Wayland reliability and responsiveness during initial mapping, resize, minimize, multi-output scale changes, buffer reuse, and compositor-driven configure sequences.
- Completed Wayland keyboard, pointer, and scroll behavior, including compose/dead keys, key repeat, modifier and focus synchronization, stuck-input cleanup, and safe seat/global removal.
- Hardened Wayland protocol negotiation and version-dependent object cleanup across older and newer environments.
- Fixed macOS `flagsChanged` events produced by focus synchronization toggling alphanumeric keys.

## [0.10.1]

### Added

- **Window title API**: added `mfb_set_title` to change the window title after creation. Implemented on Windows, macOS, X11, and Wayland, with no-op stubs on iOS, Android, Web, and DOS.

### Changed

- Unified keycode-table initialization across Windows, macOS, X11, and Wayland with one-time setup and explicit reset to `MFB_KB_KEY_UNKNOWN`.
- Moved the shared `stretch_image` declaration into `src/MiniFB_internal.h`.

### Fixed

- Fixed X11 dead-key compose cancellation so the standalone accent is emitted before the following character, matching Windows and macOS behavior.
- Fixed X11 and Wayland keyboard handling to avoid updating key state or firing keyboard callbacks for untranslated keys.
- Fixed DOS release completeness by adding the missing `mfb_set_title` backend stub required by the public API.

## [0.10.0]

### Added

- **Logging API**: `mfb_set_logger`, `mfb_set_log_level`, `mfb_log`, `mfb_log_level`, `mfb_log_info`, and `MFB_LOG*` helper macros for runtime log control and source-location-aware diagnostics. Backend messages now route through the shared logger instead of ad-hoc `fprintf`/`NSLog`.
- **Display inset APIs**: `mfb_get_display_cutout_insets` and `mfb_get_display_safe_insets` for mobile-safe layouts (Android API 28+, iOS, desktop stubs return zeros).
- **Touch pointer decoding**: `mfb_decode_touch`, `mfb_decode_touch_pos`, and `mfb_decode_touch_id` to decode packed pointer id/position values from mobile mouse getters.
- **Monitor scale**: implemented `mfb_get_monitor_scale` for Web (`devicePixelRatio`) and Android.
- **Cursor control**: implemented `mfb_show_cursor` for Web.
- **X11 scale detection**: layered fallbacks (XSettings, Xresources, XRandR, physical DPI).
- **DOS viewport**: basic viewport support for the MS-DOS backend.
- **Android `mfb_update_events`**: event-only pump without rendering, matching other backends.
- **Android example**: new example project using Android Studio Narwhal (native2026).
- **New headers**: `MiniFB_macros.h` (deprecation/pixel/logging macros), `MiniFB_types.h` (callback and logging typedefs), `WindowData_Web.h`.
- **Internal helpers**: `calculate_buffer_layout` (overflow-safe buffer validation) and `mfb_validate_viewport` (unified viewport checks), used by all backends.

### Changed

- Standardized public enum naming to `MFB_*` prefixes across states, keys, modifiers, mouse buttons, and window flags.
- Unified `mfb_open_ex` behavior across backends: consistent flag handling, `NULL`/empty title defaults to `"minifb"`, mutually-exclusive fullscreen flags logged.
- Unified `mfb_set_viewport` behavior across backends with shared validation and consistent destination recalculation.
- Unified `mfb_get_monitor_scale` so `window == NULL` is accepted across backends, returning the primary monitor scale where available and `1.0` fallback otherwise.
- Unified mouse wheel reset (`mouse_wheel_x/y = 0`) on every update across all backends.
- Web backend: auto-creates missing canvas element; pumps events in `mfb_wait_sync`.
- Moved `accumulated_error_ticks` into the timer struct (was static).
- Replaced deprecated Android API `ALooper_pollAll` with `ALooper_pollOnce`.
- Callback parameter names unified (`is_active`, `is_pressed`, `delta_x`, `delta_y`).
- Renamed `tests/` to `examples/` and updated CMake/example project paths accordingly.
- Reorganized Android examples into `native2021`/`native2026` folders.
- Moved DOS tools to `tools/dos/`, Wayland protocol generator to `tools/wayland/`.
- Updated DJGPP GCC toolchain to 12.2.0.
- Normalized line endings with `.gitattributes`.

### Deprecated

- All non-prefixed enum constants (`STATE_*`, `KB_*`, `MOUSE_*`, `WF_*`) in favor of `MFB_*` equivalents. Old names remain as deprecated aliases with compiler warnings.

### Fixed

- Fixed `MFB_ARGB` macro on Android little-endian (had 3 parameters instead of 4).
- Fixed Web `mfb_update_ex` not updating `buffer_width`/`buffer_height`/`buffer_stride`.
- Fixed integer overflow potential in buffer size calculations across all backends.
- Fixed iOS: Metal safety, content scale, touch coordinates, window lookup, active/close event management, and safer cutout/safe-inset handling when no launch screen is configured.
- Fixed Android: API 32-34 display cutout handling; surface transition and rotation edge cases.
- Fixed macOS: improved robustness and replaced `NSLog` with `mfb_log`.
- Fixed Windows: double-click messages now map to regular mouse button press events.
- Fixed Windows: initial window sizing on high-DPI displays so the client area and viewport stay aligned.
- Fixed X11: initial normal-window placement now centers on a real monitor instead of the combined virtual desktop.
- Fixed C++ wrapper: callback stubs are released when windows are destroyed, preventing stale callback reuse after recreating windows.
- Fixed Web: initialization/teardown robustness when `document.body` is not yet available.
- Fixed Wayland `wl_surface_attach`: the viewport offset was passed as the buffer offset, so buffers are now attached at `0, 0`.
- Fixed Wayland seat and output handling: pointer and keyboard listeners are added only when the seat really provides the object, and a removed `wl_output` is compared with the current one before it is destroyed.
- Fixed MS-DOS keyboard: completed the scancode table (numpad keys, F11/F12), read the initial Caps Lock state from the BIOS, applied Caps Lock to letters only, and stopped an out-of-bounds write for keys mapped to `KB_KEY_UNKNOWN` (`-1`, seen as `0xFFFFFFFF` when indexing the key table).
- Fixed MS-DOS mouse: the driver presence check uses the documented `0xFFFF` reply, the pointer range follows the real VESA resolution instead of the requested window size, and the middle button fires its callback.
- Fixed MS-DOS input under DPMI by locking the ring-buffer code used from the interrupt handler (`_go32_dpmi_lock_code`).
- Fixed use-after-free during teardown: macOS clears the `MTKView` delegate before closing the window, the OpenGL path no longer closes the X11 display it does not own, and the DOS backend no longer frees `window_data` while the caller is still using it.

## [0.9.3]

### Changed

- Reworked CMake target setup to apply compile options, definitions, standards, and link options per-target instead of relying on global flags.
- Improved CMake package installation/export flow with generated `minifb-config.cmake`, `minifb-targets.cmake`, and `minifb-config-version.cmake`.
- Raised `cmake_minimum_required` from `3.10` to `3.16`.
- Removed obsolete manual iOS detection logic in `CMakeLists.txt` now covered by modern CMake.
- `MINIFB_BUILD_VERSION_INFO` is now disabled automatically on iOS, Android, and Emscripten builds.

### Deprecated

- Deprecated legacy CMake flags `USE_METAL_API`, `USE_INVERTED_Y_ON_MACOS`, `USE_WAYLAND_API`, and `USE_OPENGL_API` in favor of the `MINIFB_USE_*` equivalents (legacy names are still accepted).

### Fixed

- Fixed newline consistency in `include/MiniFB.h`.
- Adjusted deprecation macro cleanup placement in `include/MiniFB.h`.
- Corrected the CMake project version metadata for the `0.9.2` line in `CMakeLists.txt`.

## [0.9.2]

### Fixed

- Fixed an Emscripten build issue caused by a legacy linker setting used in strict mode.

### Notes

- 2026-03-11: The `v0.9.2` tag was re-pointed to include a CMake version number correction only (`CMakeLists.txt`), with no functional source code changes.
- Sorry about this. You must execute:

```shell
git fetch origin --tags --force
```

## [0.9.1]

### Removed

- Removed Tundra build support files (`tundra.lua` and `units.lua`).

### Changed

- Minor README cleanup and consistency updates.

### Fixed

- Fixed a minor compilation issue when building with MinGW.

## [0.9.0]

### Added

- First formal release with semantic versioning.

## [Pre-0.9.0]

### Notes

- The project has been available on GitHub since 2014.
- Changes before version 0.9.0 were not tracked with formal release versions.

[Unreleased]: https://github.com/emoon/minifb/compare/v0.13.0...HEAD
[0.13.0]: https://github.com/emoon/minifb/releases/tag/v0.13.0
[0.12.0]: https://github.com/emoon/minifb/releases/tag/v0.12.0
[0.11.0]: https://github.com/emoon/minifb/releases/tag/v0.11.0
[0.10.1]: https://github.com/emoon/minifb/releases/tag/v0.10.1
[0.10.0]: https://github.com/emoon/minifb/releases/tag/v0.10.0
[0.9.3]: https://github.com/emoon/minifb/compare/v0.9.2...v0.9.3
[0.9.2]: https://github.com/emoon/minifb/compare/v0.9.1...v0.9.2
[0.9.1]: https://github.com/emoon/minifb/compare/v0.9.0...v0.9.1
[0.9.0]: https://github.com/emoon/minifb/releases/tag/v0.9.0
