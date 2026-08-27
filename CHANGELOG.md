# Changelog

All notable changes to this project are documented in this file.

## [0.12.0]

### Added

- **Log level from the environment**: `MINIFB_LOG_LEVEL` sets the log threshold by name (`trace`, `debug`, `info`, `warning`, `error`) without touching the code. It deliberately wins over `mfb_set_log_level()`, so you can get more output from a program you cannot rebuild. An unknown value is reported as an error and then ignored, leaving the threshold where the program left it.
- **Wayland fallback testing**: `MINIFB_WAYLAND_FORCE_VERSIONS` lowers the version MiniFB binds for one or more protocol globals, and `MINIFB_WAYLAND_DISABLE_GLOBALS` hides globals as if the compositor never advertised them. Both work in every build and are read only while globals are bound, so a single machine can exercise fallback paths that would otherwise need another compositor.

### Changed

- Wayland mouse wheel: the continuous `wl_pointer.axis` value is now divided by the ratio Weston uses, which SDL and GLFW follow as well, so one wheel notch reports `1.0` like the other backends. This only affects compositors that fall back to the continuous value; those that send `axis_value120` or `axis_discrete` already reported `1.0`.
- Every Wayland listener callback now carries a comment with its protocol interface and the version that introduced it, so the version-dependent paths can be found with grep.
- Wayland: `xdg_toplevel.configure` now logs its state array at DEBUG (`activated`, `suspended`, `maximized`, `resizing`, ...) instead of discarding it.
- Wayland: `wl_keyboard.repeat_info` now logs the advertised rate and delay at DEBUG. It also reports when client-side repeat is off, which happens when the compositor drives it instead.
- Wayland: a failed dispatch now reports what libwayland knows about the connection. A protocol error names the interface, the object and the error code. Any other failure reports the system error. Before, every case printed the same generic message.
- Added `docs/wayland-testing.md`: the interfaces each variable accepts, which versions are worth testing and what each one covers, the related variables from libwayland and xkbcommon, and what this approach cannot test.
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
- **CMake restructure**: build options are now prefixed (`MINIFB_USE_WAYLAND_API`, `MINIFB_USE_OPENGL_API`, `MINIFB_USE_METAL_API`, `MINIFB_BUILD_EXAMPLES`, ...), with fixes to the exported package config, the generated version header, and the Emscripten, iOS, and macOS build paths.

### Deprecated

- Unprefixed CMake options (`USE_WAYLAND_API`, `USE_OPENGL_API`, `USE_METAL_API`, `USE_INVERTED_Y_ON_MACOS`). They still work but emit a deprecation warning; use their `MINIFB_*` equivalents.

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
