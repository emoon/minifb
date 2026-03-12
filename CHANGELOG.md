# Changelog

All notable changes to this project are documented in this file.

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
