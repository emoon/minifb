# Changelog

All notable changes to this project are documented in this file.

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
- Fixed Wayland: dynamic resize and resource reallocation paths.
- Fixed MS-DOS: multiple rendering and input handling issues.
- Fixed multiple lifecycle and event-loop edge cases across all backends.

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
