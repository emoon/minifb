# MiniFB

MiniFB (Mini FrameBuffer) is a small cross-platform library that makes it easy to render (32-bit) pixels in a window.

## Quick Start

An example is the best way to show how it works:

```c
int main() {
    struct mfb_window *window = mfb_open_ex("my display", 800, 600, MFB_WF_RESIZABLE);
    if (window == NULL)
        return 0;

    uint32_t *buffer = malloc(800 * 600 * 4);

    mfb_update_state state;
    do {
        // TODO: add some fancy rendering to the buffer of size 800 * 600

        state = mfb_update_ex(window, buffer, 800, 600);

        if (state != MFB_STATE_OK)
            break;

    } while(mfb_wait_sync(window));

    free(buffer);
    buffer = NULL;
    window = NULL;

    return 0;
}
```

### How it works

1. First the application creates a **window** calling **mfb_open** or **mfb_open_ex**.
2. Next, it's the application's responsibility to allocate a buffer to work with.
3. Next, call **mfb_update** or **mfb_update_ex** to copy the buffer to the window and display it. If this function returns a value lower than 0, the window has been destroyed internally and cannot be used anymore.
4. Last the code waits to synchronize with the monitor calling **mfb_wait_sync**.

**Note:** By default, if ESC key is pressed, **mfb_update** / **mfb_update_ex** will return -1 (and the window will have been destroyed internally).

See <https://github.com/emoon/minifb/blob/master/examples/noise.c> for a complete example.

## Supported Platforms

| Platform | Backends | Status |
|----------|----------|--------|
| **Windows** | GDI, OpenGL | Fully supported |
| **macOS** | Cocoa, Metal | Fully supported |
| **Linux/Unix** | X11, Wayland | Fully supported (X11, Wayland) |
| **iOS** | Metal | Beta |
| **Android** | Native | Beta |
| **Web** | WASM | Beta |
| **DOS** | DJGPP | Beta |

MiniFB has been tested on Windows, macOS, Linux, iOS, Android, web, and DOSBox-x. Compatibility may vary depending on your setup. Currently, the library does not perform any data conversion if a proper 32-bit display cannot be created.

## Features

- Window creation and management
- Event callbacks (keyboard, mouse, window lifecycle)
- Direct window state queries
- Per-window custom data
- Built-in timers and FPS control
- C and C++ interfaces
- Cursor control

## API Reference

### Window Management

```c
// Create and manage windows
struct mfb_window * mfb_open(const char *title, unsigned width, unsigned height);
struct mfb_window * mfb_open_ex(const char *title, unsigned width, unsigned height, unsigned flags);
void                mfb_close(struct mfb_window *window);
void                mfb_set_title(struct mfb_window *window, const char *title);

// Update and synchronization
mfb_update_state    mfb_update(struct mfb_window *window, void *buffer);
mfb_update_state    mfb_update_ex(struct mfb_window *window, void *buffer, unsigned width, unsigned height);
mfb_update_state    mfb_update_events(struct mfb_window *window);
bool                mfb_wait_sync(struct mfb_window *window);

// Viewport control
// Coordinates/sizes are in drawable coordinates (same units as mfb_get_window_width/height and resize callback).
bool                mfb_set_viewport(struct mfb_window *window, unsigned offset_x, unsigned offset_y, unsigned width, unsigned height);
bool                mfb_set_viewport_best_fit(struct mfb_window *window, unsigned old_width, unsigned old_height);
```

`mfb_set_viewport()` returns `false` if:
- `width == 0` or `height == 0`
- viewport bounds exceed the current window drawable size

`mfb_open()` and `mfb_open_ex()` return `NULL` if:
- `width == 0` or `height == 0`
- `width * 4` would overflow the internal framebuffer stride

If `title` is `NULL` or empty, MiniFB uses `"minifb"` as the effective window/canvas title.

If both `MFB_WF_FULLSCREEN` and `MFB_WF_FULLSCREEN_DESKTOP` are provided, `MFB_WF_FULLSCREEN` takes precedence.

`mfb_update_ex()` returns `MFB_STATE_INVALID_BUFFER` if:
- `buffer == NULL`
- `width == 0` or `height == 0`
- `width * 4` would overflow internal stride calculations

`mfb_update_ex()` runtime behavior is backend-specific:
- Wayland waits for compositor frame callback inside `mfb_update_ex()` (can block).
- Android may return `MFB_STATE_OK` without presenting when `ANativeWindow` is temporarily unavailable during lifecycle transitions.

Open-time readiness is backend-specific:
- Wayland waits for the initial configure handshake before returning from `mfb_open_ex()`.
- Android may return a window handle before `ANativeWindow` is ready (rendering starts once the native window becomes available).

`mfb_open_ex()` flag support by backend:

| Backend | RESIZABLE | BORDERLESS | ALWAYS_ON_TOP | FULLSCREEN | FULLSCREEN_DESKTOP |
|---------|-----------|------------|---------------|------------|--------------------|
| Windows | Yes | Yes | Yes | Yes | Yes |
| X11 | Yes | Yes* | Yes* | Yes* | Yes* |
| Wayland | Yes | Yes | No (ignored, warning) | Yes | Yes (maximized) |
| macOS | Yes | Yes | Yes | Yes | Yes (zoom/maximize) |
| Web | No (ignored, warning) | No (ignored, warning) | No (ignored, warning) | Yes** | Yes** |
| DOS | No (ignored, warning) | No (ignored, warning) | No (ignored, warning) | No (ignored, warning) | No (ignored, warning) |
| Android | No (ignored, warning) | No (ignored, warning) | No (ignored, warning) | No (ignored, warning) | No (ignored, warning) |
| iOS | No (ignored, warning) | No (ignored, warning) | No (ignored, warning) | No (ignored, warning) | No (ignored, warning) |

\* Best effort via window-manager hints/properties; behavior depends on compositor/WM support.

\** Browser-managed fullscreen; typically requires a user gesture before entering fullscreen.

### Event Callbacks

Register callbacks to handle window events:

```c
// Callback types
void                mfb_set_active_callback(struct mfb_window *window, mfb_active_func callback);
void                mfb_set_resize_callback(struct mfb_window *window, mfb_resize_func callback);
void                mfb_set_close_callback(struct mfb_window *window, mfb_close_func callback);
void                mfb_set_keyboard_callback(struct mfb_window *window, mfb_keyboard_func callback);
void                mfb_set_char_input_callback(struct mfb_window *window, mfb_char_input_func callback);
void                mfb_set_mouse_button_callback(struct mfb_window *window, mfb_mouse_button_func callback);
void                mfb_set_mouse_move_callback(struct mfb_window *window, mfb_mouse_move_func callback);
void                mfb_set_mouse_scroll_callback(struct mfb_window *window, mfb_mouse_scroll_func callback);
```

#### Callback Signature Examples

```c
void active(struct mfb_window *window, bool is_active) {
    // Called when window gains/loses focus
}

void resize(struct mfb_window *window, int width, int height) {
    // Called when window is resized (width/height use the same drawable units as mfb_set_viewport)
    // Optionally adjust viewport:
    // mfb_set_viewport(window, x, y, width, height);
}

bool close(struct mfb_window *window) {
    // Called when close is requested
    return true;    // true => confirm close, false => cancel
}

void keyboard(struct mfb_window *window, mfb_key key, mfb_key_mod mod, bool is_pressed) {
    if (key == KB_KEY_ESCAPE) {
        mfb_close(window);
    }
}

void char_input(struct mfb_window *window, unsigned int char_code) {
    // Unicode character input
}

void mouse_btn(struct mfb_window *window, mfb_mouse_button button, mfb_key_mod mod, bool is_pressed) {
    // Mouse button events
}

void mouse_move(struct mfb_window *window, int x, int y) {
    // Mouse movement (note: fired frequently)
}

void mouse_scroll(struct mfb_window *window, mfb_key_mod mod, float delta_x, float delta_y) {
    // Mouse wheel/scroll events
}
```

#### C++ Callback Interface

If you are using C++, you can set callbacks to class methods or lambda expressions:

```cpp
// Using object and pointer to member
mfb_set_active_callback(window, &myObject, &MyClass::onActive);

// Using std::bind
mfb_set_active_callback(std::bind(&MyClass::onActive, &myObject, _1, _2), window);

// Using lambda
mfb_set_active_callback([](struct mfb_window *window, bool is_active) {
    // Handle event
}, window);
```

### Window State Queries

Query window and input state directly (alternative to callbacks):

```c
// Window state
bool                mfb_is_window_active(struct mfb_window *window);
unsigned            mfb_get_window_width(struct mfb_window *window);
unsigned            mfb_get_window_height(struct mfb_window *window);
void                mfb_get_window_size(struct mfb_window *window, unsigned *width, unsigned *height);

// Key utilities
const char *        mfb_get_key_name(mfb_key key);

// Drawable area (considering viewport scaling/DPI)
unsigned            mfb_get_drawable_offset_x(struct mfb_window *window);
unsigned            mfb_get_drawable_offset_y(struct mfb_window *window);
unsigned            mfb_get_drawable_width(struct mfb_window *window);
unsigned            mfb_get_drawable_height(struct mfb_window *window);
void                mfb_get_drawable_bounds(struct mfb_window *window, unsigned *offset_x, unsigned *offset_y, unsigned *width, unsigned *height);

// Input state
int                 mfb_get_mouse_x(struct mfb_window *window);
int                 mfb_get_mouse_y(struct mfb_window *window);
void                mfb_decode_touch(int combined, int *pos, int *id);     // Decode packed mobile touch pos/id
int                 mfb_decode_touch_pos(int combined);                     // Extract position from a packed touch value
int                 mfb_decode_touch_id(int combined);                      // Extract pointer id from a packed touch value
float               mfb_get_mouse_scroll_x(struct mfb_window *window);      // Mouse wheel delta X from the most recent event pump (0.0f if none)
float               mfb_get_mouse_scroll_y(struct mfb_window *window);      // Mouse wheel delta Y from the most recent event pump (0.0f if none)
const uint8_t *     mfb_get_mouse_button_buffer(struct mfb_window *window); // 1=pressed, 0=released (8 buttons)
const uint8_t *     mfb_get_key_buffer(struct mfb_window *window);          // 1=pressed, 0=released
```

On Android/iOS touch paths, `mfb_get_mouse_x()` and `mfb_get_mouse_y()` include an encoded touch pointer id in the upper bits.
Use `mfb_decode_touch()` to decode both at once, or `mfb_decode_touch_pos()` / `mfb_decode_touch_id()` individually. On desktop/Web/DOS, `id` is always `0`.
For touch callbacks, the pointer id is also exposed as `button` in `mfb_mouse_button_func` (`MFB_MOUSE_BTN_0`..`MFB_MOUSE_BTN_7`).
On Android/iOS touch move callbacks, `mfb_mouse_move_func` receives packed `x/y` values (same encoding as getters).
On Android, external `HOVER_MOVE` also uses packed `x/y`; if Android does not provide a valid pointer id, MiniFB uses fallback id `15`.
`mfb_get_mouse_scroll_x/y()` are pump-local values: MiniFB resets them to `0.0f` before each backend event pump, then writes the delta if a scroll event is received during that pump.

### Per-Window Data

Attach and retrieve custom data per window:

```c
void                mfb_set_user_data(struct mfb_window *window, void *user_data);
void *              mfb_get_user_data(struct mfb_window *window);
```

### Timers

Create and manage timers independently:

```c
struct mfb_timer *  mfb_timer_create(void);
void                mfb_timer_destroy(struct mfb_timer *tmr);
void                mfb_timer_reset(struct mfb_timer *tmr);
void                mfb_timer_compensated_reset(struct mfb_timer *tmr);
double              mfb_timer_now(struct mfb_timer *tmr);
double              mfb_timer_delta(struct mfb_timer *tmr);
double              mfb_timer_get_frequency(void);
double              mfb_timer_get_resolution(void);
```

### Frame Rate Control

Control target FPS and frame synchronization:

```c
void                mfb_set_target_fps(uint32_t fps);         // Default: 60 fps
unsigned            mfb_get_target_fps(void);

bool                mfb_wait_sync(struct mfb_window *window); // Frame sync point
```

**Note:** Hardware-accelerated syncing (OpenGL / Metal, where supported) will use vertical sync. Other platforms use software pacing.

### Logging

MiniFB ships with a simple logger that you can redirect or disable:

```c
// Set a custom logger; pass NULL to restore the built-in logger
void mfb_set_logger(mfb_log_func user_logger);

// Control verbosity threshold (inclusive)
void mfb_set_log_level(mfb_log_level level);
```

- Levels (low → high): `MFB_LOG_TRACE`, `MFB_LOG_DEBUG`, `MFB_LOG_INFO`, `MFB_LOG_WARNING`, `MFB_LOG_ERROR`.
- Defaults: in `_DEBUG` builds the threshold is `MFB_LOG_DEBUG`; in release builds `MFB_LOG_INFO`.
- Messages with a level **below** the threshold are discarded; equal or higher are emitted.
- Custom loggers receive the message already formatted (`level` + `message`).
- The built-in logger writes `TRACE`/`DEBUG`/`INFO` to `stdout` and `WARNING`/`ERROR` to `stderr` as `[MiniFB (LEVEL)] message`.

### Cursor Control

```c
void                mfb_show_cursor(struct mfb_window *window, bool show);
```

**Note:** Cursor hiding is supported on Windows, macOS, X11, and Wayland only.

### Monitor Information

```c
void                mfb_get_monitor_scale(struct mfb_window *window, float *scale_x, float *scale_y);
void                mfb_get_monitor_dpi(struct mfb_window *window, float *dpi_x, float *dpi_y); // [Deprecated]
```

`mfb_get_monitor_scale()`:

- Returns scale multipliers (`1.0` = 100%).
- If `window == NULL`, outputs still receive a safe fallback (`1.0`) when their pointers are non-`NULL`.
- Some backends provide real scale values (for example Retina/HiDPI); others currently return fixed `1.0`.
- On X11, scale detection is implemented (Xresources/XRandR/fallbacks), but in many desktop setups the value is effectively startup-time and may not update dynamically after changing global scale while the app is running.

If you define layout in logical units and need drawable-coordinate values for `mfb_set_viewport()`, convert explicitly:

```c
float sx = 1.0f, sy = 1.0f;
mfb_get_monitor_scale(window, &sx, &sy);
unsigned margin_x_viewport = (unsigned) lroundf(margin_logical_x * sx);
unsigned margin_y_viewport = (unsigned) lroundf(margin_logical_y * sy);
```

### Display Insets

Two C functions let you query display insets from native code:

```c
// Physical cutout/notch area only.
bool mfb_get_display_cutout_insets(struct mfb_window *window,
                                   int *left, int *top, int *right, int *bottom);

// Full safe area: cutout + system UI reserved regions.
bool mfb_get_display_safe_insets(struct mfb_window *window,
                                 int *left, int *top, int *right, int *bottom);
```

Insets are **edge margins in pixels**, not a rectangle:

- `left`, `top`, `right`, `bottom` are distances from each window edge.
- No reserved area means `0, 0, 0, 0`.
- If you need a safe rectangle, derive it from window size:
  - `safe_x = left`
  - `safe_y = top`
  - `safe_w = window_w - left - right`
  - `safe_h = window_h - top - bottom`

Return value contract (all backends):

- `true`: query succeeded, output values are valid (possibly all zeros).
- `false`: query unavailable/invalid at that moment; outputs are set to zeros.

Behavior by backend:

| Backend | `mfb_get_display_cutout_insets` | `mfb_get_display_safe_insets` |
|---------|----------------------------------|--------------------------------|
| Android | Physical cutout only (`DisplayCutout`, API 28+). Returns `true` with zeros when there is no cutout. Returns `false` if unavailable (e.g. API < 28 or query failure). | Full safe insets. API 30+: `WindowInsets.getInsets(systemBars|displayCutout)`. API 24-29: `getSystemWindowInset*()` fallback. |
| iOS | Approximated from `UIWindow.safeAreaInsets` for physical cutout intent. Bottom is kept `0` (home indicator is not a physical cutout). | Uses `UIWindow.safeAreaInsets` directly (includes notch/Dynamic Island + status bar + home indicator). |
| Desktop/Web/DOS | Returns `true` with zeros for a valid window (no platform cutout/safe-inset data exposed). | Returns `true` with zeros for a valid window. |

For any backend, passing `window == NULL` returns `false` and zero outputs.

Example usage (safe layout):

```c
void on_resize(struct mfb_window *window, int width, int height) {
    int left = 0, top = 0, right = 0, bottom = 0;
    if (!mfb_get_display_safe_insets(window, &left, &top, &right, &bottom)) {
        return;
    }

    int safe_x = left;
    int safe_y = top;
    int safe_w = width  - left - right;
    int safe_h = height - top  - bottom;

    // Clamp defensive, in case platform values arrive before resize settles.
    if (safe_w < 0) safe_w = 0;
    if (safe_h < 0) safe_h = 0;

    // Place important UI inside [safe_x, safe_y, safe_w, safe_h].
}
```

## Integration

### Adding MiniFB to Your Project

Add this **repository as a submodule** in your dependencies folder:

```sh
git submodule add https://github.com/emoon/minifb.git dependencies/minifb
```

Then in your `CMakeLists.txt` file:

```cmake
add_subdirectory(dependencies/minifb)

# Link MiniFB to your project:
target_link_libraries(${PROJECT_NAME} minifb)
```

## Build Instructions

The build system is **CMake**.

## Versioning

MiniFB existed for many years without an official release version. Starting from this codebase, the project now uses SemVer and declares **v0.9.0** as the official baseline version.

Builds expose public version/build metadata for C/C++ consumers via `minifb_version.h` (generated at configure time and installed with the public headers). This includes:

- `MINIFB_VERSION_STRING` / major-minor-patch macros
- packed `MINIFB_VERSION_NUMERIC` and extraction helpers
- optional Git-derived metadata (`MINIFB_COMMIT_COUNT`, `MINIFB_COMMITS_SINCE_TAG`, `MINIFB_GIT_SHA`, `MINIFB_GIT_DIRTY`)

When building from a source archive without `.git` metadata, defaults are used (`unknown` SHA, counters `0`) and the build still succeeds.

Some projects use date-based versions when retrofitting versioning. MiniFB now uses SemVer to keep compatibility expectations clearer for users and downstream integrations.

### Windows

If you use **CMake**, a Visual Studio project will be generated.

Furthermore you can also use **MinGW** instead of Visual Studio.

#### OpenGL API backend (Windows)

By default, the OpenGL backend is used instead of Windows GDI because it is faster. To maintain compatibility with older computers, an OpenGL 1.5 context is created (no shaders needed).

To enable or disable OpenGL just use a CMake flag:

```sh
cmake .. -DMINIFB_USE_OPENGL_API=ON
# or
cmake .. -DMINIFB_USE_OPENGL_API=OFF
```
`USE_OPENGL_API` is still accepted for compatibility, but deprecated.

### X11 (FreeBSD, Linux, *nix)

#### Dependencies for X11 on Ubuntu/Debian

To compile MiniFB with X11 backend on Ubuntu/Debian, install the following packages:

```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    pkg-config \
    libx11-dev \
    libxkbcommon-dev \
    libgl1-mesa-dev \
    libxrandr-dev
```

- **build-essential**: Compiler toolchain (gcc, g++, make)
- **cmake**: Build system
- **pkg-config**: Helper tool for compiling applications and libraries
- **libx11-dev**: X11 core libraries and headers
- **libxkbcommon-dev**: Keyboard handling library
- **libgl1-mesa-dev**: OpenGL libraries (required if using OpenGL backend, which is default)
- **libxrandr-dev** *(optional)*: Enables XRandR-based monitor scale/DPI queries on X11 (`mfb_get_monitor_scale` fallback path and diagnostics)

If you prefer to use X11 without OpenGL (XImage rendering), you can omit `libgl1-mesa-dev`.
If you do not need XRandR-assisted scale/DPI detection, you can omit `libxrandr-dev`.

Equivalent packages for other distros:

- Fedora: `gcc`, `cmake`, `pkg-config`, `libX11-devel`, `libxkbcommon-devel`, `mesa-libGL-devel`
- Arch: `base-devel`, `cmake`, `pkgconf`, `libx11`, `libxkbcommon`, `mesa`
- openSUSE: `gcc`, `cmake`, `pkg-config`, `libX11-devel`, `libxkbcommon-devel`, `Mesa-libGL-devel`

#### Building with CMake

If you use **CMake**, just disable the flag:

```sh
mkdir build-x11
cd build-x11
cmake .. -DMINIFB_USE_WAYLAND_API=OFF
```
`USE_WAYLAND_API` is still accepted for compatibility, but deprecated.

#### OpenGL API backend (X11)

By default, the OpenGL backend is used instead of X11 XImages because it is faster. To maintain compatibility with older computers, an OpenGL 1.5 context is created (no shaders needed).

To enable or disable OpenGL just use a CMake flag:

```sh
cmake .. -DMINIFB_USE_OPENGL_API=ON -DMINIFB_USE_WAYLAND_API=OFF
# or
cmake .. -DMINIFB_USE_OPENGL_API=OFF -DMINIFB_USE_WAYLAND_API=OFF
```
`USE_OPENGL_API` and `USE_WAYLAND_API` are still accepted for compatibility, but deprecated.

### Wayland (Linux)

Depends on gcc and wayland-client and wayland-cursor. Built using the wayland-gcc variants.
The Wayland backend supports the same core MiniFB API surface as the other primary desktop backends (Windows, macOS, X11).

#### Dependencies for Wayland on Ubuntu/Debian

To compile MiniFB with Wayland backend on Ubuntu/Debian, install the following packages:

```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    pkg-config \
    libwayland-dev \
    libxkbcommon-dev \
    wayland-protocols
```

- **build-essential**: Compiler toolchain (gcc, g++, make)
- **cmake**: Build system
- **pkg-config**: Helper tool for compiling applications and libraries
- **libwayland-dev**: Wayland client libraries and headers
- **libxkbcommon-dev**: Keyboard handling library
- **wayland-protocols**: Wayland protocol definitions

Equivalent packages for other distros:

- Fedora: `gcc`, `cmake`, `pkg-config`, `wayland-devel`, `libxkbcommon-devel`, `wayland-protocols-devel`
- Arch: `base-devel`, `cmake`, `pkgconf`, `wayland`, `libxkbcommon`, `wayland-protocols`
- openSUSE: `gcc`, `cmake`, `pkg-config`, `wayland-devel`, `libxkbcommon-devel`, `wayland-protocols-devel`

#### Wayland Protocol Compatibility

Different Linux distributions and versions may ship with different versions of Wayland and its protocols. MiniFB includes pre-generated protocol headers and code that work with most common setups. However, if you encounter compatibility issues or want to ensure optimal compatibility with your specific Wayland version, you can regenerate the protocol files using your system's Wayland version.

To regenerate Wayland protocol files for your system you must run first the protocol generation script:

```sh
chmod +x ./tools/wayland/generate-protocols.sh
./tools/wayland/generate-protocols.sh
```

This script will generate protocol headers and code that are specifically compatible with your installed Wayland version, potentially resolving any version mismatch issues.

If you use **CMake**, just enable the flag:

```sh
mkdir build-wayland
cd build-wayland
cmake .. -DMINIFB_USE_WAYLAND_API=ON
```
`USE_WAYLAND_API` is still accepted for compatibility, but deprecated.

### macOS

Cocoa and clang are assumed to be installed on the system (downloading the latest Xcode and installing the command line tools should do the trick).

Note that macOS Mojave+ does not support the Cocoa framework as expected. For that reason, you can switch to the Metal API.
To enable it with CMake, use the `MINIFB_USE_METAL_API` option.

If you use **CMake**, just enable the flag:

```sh
mkdir build-macos-metal
cd build-macos-metal
cmake .. -DMINIFB_USE_METAL_API=ON
```
`USE_METAL_API` is still accepted for compatibility, but deprecated.

Or, if you don't want to use the Metal API:

```sh
mkdir build-macos-cocoa
cd build-macos-cocoa
cmake .. -DMINIFB_USE_METAL_API=OFF
```

#### Coordinate system

On macOS, the default mouse coordinate system is (0, 0) → (left, bottom). But since we want to create a multiplatform library, we invert the coordinates so that (0, 0) → (left, top), like on the other platforms.

In any case, if you want to get the default coordinate system you can use the CMake flag: `MINIFB_USE_INVERTED_Y_ON_MACOS=ON`

```sh
mkdir build-macos-inverted-y
cd build-macos-inverted-y
cmake .. -DMINIFB_USE_INVERTED_Y_ON_MACOS=ON
```
`USE_INVERTED_Y_ON_MACOS` is still accepted for compatibility, but deprecated.

**Note**: In the future, we may use a global option so that all platforms behave in the same way. Probably: -DUSE_INVERTED_Y

### iOS (beta)

It works with and without a `UIWindow`.
If you create the window/view hierarchy through Storyboard, set the `UIViewController` to `iOSViewController` and the root `UIView` to `iOSView`.

**Launch screen / storyboard requirement**:

For App Store distribution, Apple requires a launch storyboard (legacy static launch images are deprecated). Without a launch storyboard, iOS can start in a compatibility layout and you may see top/bottom black bands or an incorrect initial drawable size.

That is why there are now two iOS example targets:

- `noise`: uses `examples/ios/Info.plist` + `examples/ios/LaunchScreen.storyboard` (recommended, App Store-ready).
- `noise_no_storyboard`: uses `examples/ios/Info.no_storyboard.plist` without launch storyboard (useful for legacy/manual setups and behavior comparison).

Apple references:
- https://developer.apple.com/news/?id=03042020b
- https://developer.apple.com/documentation/xcode/specifying-your-apps-launch-screen
- https://developer.apple.com/videos/play/wwdc2019/401/

**Issues**:

- To run on a physical device, you need to set a valid 'Signing Team' and 'Bundle Identifier'.
- iOS Simulator supports Metal on modern Xcode versions, but final validation should still be done on real devices.

**Limitations**:

- No keyboard or char-input callbacks (iOS backend uses touch events instead)
- Single window only (flags to `mfb_open_ex()` are ignored)
- `mfb_show_cursor()` is a no-op (no cursor concept on touch devices)
- No dedicated multitouch API; touches are mapped to mouse callbacks (`MFB_MOUSE_BTN_0`..`MFB_MOUSE_BTN_7`)
- Mouse events represent touch events (coordinates track the last processed touch event)
- Touch pointer id is packed into upper bits of `mfb_get_mouse_x()` / `mfb_get_mouse_y()` values; decode with `mfb_decode_touch()` / `mfb_decode_touch_pos()` / `mfb_decode_touch_id()`
- No mouse wheel/scroll callback support on iOS

`mfb_set_active_callback()` is triggered from iOS app lifecycle notifications
(active/inactive transitions).
`mfb_set_close_callback()` is called as a termination notification only; iOS does not allow canceling app termination.

`mfb_set_target_fps()` / `mfb_get_target_fps()` are supported on iOS for software pacing via `mfb_wait_sync()`.
If your app is driven by `CADisplayLink` (like the example), frame pacing is display-driven by iOS.

Core rendering, viewport, timers, and user data management work normally.

**Example**:

```objective-c
- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
    g_width  = [UIScreen mainScreen].bounds.size.width;
    g_height = [UIScreen mainScreen].bounds.size.height;

    g_window = mfb_open("noise", g_width, g_height);
    if (g_window == NULL) {
        return NO;
    }

    g_buffer = malloc(g_width * g_height * 4);

    return YES;
}

- (void)applicationDidBecomeActive:(UIApplication *)application {
    mDisplayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(OnUpdateFrame)];
    [mDisplayLink addToRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
}

- (void)applicationWillTerminate:(UIApplication *)application {
    [mDisplayLink invalidate];
    mfb_close(g_window);
}

- (void) OnUpdateFrame {
    if(g_buffer != NULL) {
        // Do your wonderful rendering stuff
    }

    mfb_update_state state = mfb_update_ex(g_window, g_buffer, g_width, g_height);
    if (state != MFB_STATE_OK) {
        free(g_buffer);
        g_window = NULL;
        g_buffer = NULL;
        g_width  = 0;
        g_height = 0;
    }
}
```

**CMake**:

```sh
mkdir build-ios
cd build-ios
cmake -G Xcode -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 ..
```

Then choose the Xcode scheme you want to run:

- `noise` (with launch storyboard, recommended)
- `noise_no_storyboard` (without launch storyboard)

### Android (beta)

Take a look at the example in examples/android. You need **Android Studio** to build and run it.

**Limitations**:

- No general keyboard/char-input callback support yet
- Single window only (flags to `mfb_open_ex()` are ignored)
- `mfb_show_cursor()` is a no-op (no cursor concept on touch devices)
- No dedicated multitouch API; touches are mapped to mouse callbacks (`MFB_MOUSE_BTN_0`..`MFB_MOUSE_BTN_7`)
- Mouse events represent touch events (last processed touch position)
- Touch pointer id is packed into upper bits of `mfb_get_mouse_x()` / `mfb_get_mouse_y()` values; decode with `mfb_decode_touch()` / `mfb_decode_touch_pos()` / `mfb_decode_touch_id()`
- `mfb_get_monitor_scale()` reports Android density scale (same value for X/Y, from `AConfiguration_getDensity()` with `160dpi = 1.0`)

**Note**: On Android, pressing `BACK` should close the app by default. In some emulators, right-click may be mapped to `BACK`. For debugging this case, the Android example CMake option `MINIFB_ANDROID_CAPTURE_RIGHT_CLICK_AS_ESC` can be enabled to map `BACK` to `ESC` instead (default: `OFF`).

`mfb_set_target_fps()` / `mfb_get_target_fps()` are supported on Android for software pacing via `mfb_wait_sync()`.

All other MiniFB functions work normally, including timers, viewports, and user data management.

#### Pixel format on Android

MiniFB uses a **32-bit pixel buffer** on all platforms, but the byte order in memory
differs between Android and desktop/iOS:

| Platform | Byte order in memory | Equivalent uint32_t (LE) |
|----------|----------------------|--------------------------|
| Desktop (Windows, Linux, macOS) | B · G · R · X | `0x00RRGGBB` |
| iOS | B · G · R · A | `0x00RRGGBB` |
| **Android** | **R · G · B · X** | **`0x00BBGGRR`** |

**You do not need to think about this** as long as you construct pixels with the
`MFB_RGB` / `MFB_ARGB` macros, they expand to the correct bit layout automatically
on every platform:

```c
buffer[i] = MFB_RGB(255, 0, 0);   // always displays red, on every platform
```

**Where it matters: external pixel data.**
If you load an image with a library that always produces RGBA bytes in memory
(e.g. `stb_image`, `libpng`, browser canvas), and you pass that data directly
to `mfb_update_ex`, the colors will be correct on Android but **red and blue will
be swapped on desktop/iOS** (and vice-versa if you adapt for desktop).

```c
// stb_image / libpng give RGBA bytes in memory:
//   byte[0]=R  byte[1]=G  byte[2]=B  byte[3]=A

// On Android this is exactly what ANativeWindow expects, pass as-is.
// On desktop/iOS you must swap R <-> B before calling mfb_update_ex.
```

**Why can't Android just accept the same format as desktop?**
`ANativeWindow` (the Android NDK surface API) does not expose a BGRA format in its
public interface, only `WINDOW_FORMAT_RGBX_8888` (RGBA bytes) and `WINDOW_FORMAT_RGB_565`
are guaranteed on all devices. Doing a full-buffer swizzle per frame inside the library
would add CPU cost for every rendered frame. The current design avoids that cost by
adjusting the macro at compile time instead.

#### Display cutout / Notch (API 32-34)

Android's handling of the display cutout (notch, punch-hole camera) changed across API levels
and can cause a framebuffer-size mismatch if not handled explicitly:

| API level | Default behaviour                               | Result                        |
|-----------|-------------------------------------------------|-------------------------------|
| ≤ 31      | Legacy fullscreen flags handle everything       | Works out of the box          |
| 32-34     | System reserves space for the cutout by default | **Content shifted / clipped** |
| ≥ 35      | Edge-to-edge is forced by the OS                | Works out of the box          |

Two approaches are provided in the example (`examples/android/native2026`); pick whichever fits
your project.

##### Option A - Manifest + theme (no Java code)

Add a theme to `res/values/styles.xml`:

```xml
<resources>
    <style name="FullscreenNative" parent="@android:style/Theme.NoTitleBar.Fullscreen">
        <!-- Allows the window to draw into the cutout area (API 31+). -->
        <item name="android:windowLayoutInDisplayCutoutMode">always</item>
    </style>
</resources>
```

Then reference it in `AndroidManifest.xml`:

```xml
<activity
    android:name="android.app.NativeActivity"
    android:theme="@style/FullscreenNative"
    ...>
```

**Pros**: zero Java code, takes effect before the native thread starts.
**Cons**: limited runtime control; no way to query inset values from C.

##### Option B - Java subclass (recommended)

Subclass `NativeActivity` in `MiniFBActivity.java` and override `onCreate` /
`onWindowFocusChanged` to call `setupFullscreen()`, which:

- Sets `LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS` (API 31+) or `SHORT_EDGES` (API 28-30).
- Hides system bars via `WindowInsetsController` (API 30+) or the legacy
  `setSystemUiVisibility` flags (API 24-29).
- Re-applies on focus changes (bars can reappear after an edge-swipe gesture).

In `AndroidManifest.xml` replace the activity class name:

```xml
<activity
    android:name="com.example.noise.MiniFBActivity"
    android:theme="@style/FullscreenNative"
    ...>
```

The theme is kept as an early fallback; the Java code overrides it once the Activity starts.

**Pros**: robust, handles all API levels, re-applies after gesture-triggered bar visibility.
**Cons**: requires one Java source file.

Both options can coexist (the theme fires first, the Java code reinforces it).

#### Querying Insets from C

The display inset APIs are backend-agnostic:
`mfb_get_display_cutout_insets()` and `mfb_get_display_safe_insets()`.

See [Display Insets](#display-insets) in the API reference for exact semantics,
return contract, and backend behavior details.

### Web (WASM)

Download and install [Emscripten](https://emscripten.org/). When configuring your CMake build, specify the Emscripten toolchain file. Then proceed to build as usual.

#### Building and running the examples (WASM)

```sh
cmake -DCMAKE_TOOLCHAIN_FILE=/path/to/emsdk/<version>/emscripten/cmake/Modules/Platform/Emscripten.cmake -B build-web .
cmake --build build-web
```

On Windows, you can't use Visual Studio's default CMake generator because Emscripten uses its own toolchain based on a modified version of Clang. Instead, you need to generate MinGW-compatible makefiles. If you have MinGW installed:

```sh
cmake -DCMAKE_TOOLCHAIN_FILE=C:\Path\to\emsdk\<version>\upstream\emscripten\cmake\Modules\Platform\Emscripten.cmake -G "MinGW Makefiles" -B build-web .
cmake --build build-web
```

> **Note**: On Windows, you will need a build tool other than Visual Studio. [Ninja](https://ninja-build.org/) is the best and easiest option. Simply download it, put the `ninja.exe` executable somewhere in your path, and make it available on the command line via your `PATH` environment variable. Then invoke the first command above with the addition of `-G Ninja` at the end.

Then open the file `build-web/index.html` in your browser to view the example index.

The examples are build using the Emscripten flag `-sSINGLE_FILE`, which will coalesce the `.js` and `.wasm` files into a single `.js` file. If you build your own apps without the `-sSINGLE_FILE` flag, you can not simply open the `.html` file in the browser from disk. Instead, you need an HTTP server to serve the build output. The simplest solution for that is Python's `http.server` module:

```sh
python3 -m http.server --directory build-web
```

You can then open the index at [http://localhost:8000](http://localhost:8000) in your browser.

#### Integrating a MiniFB app in a website

To build an executable target for the web, you need to add a linker option specifying its module name, e.g.:

```cmake
target_link_options(my_app PRIVATE "-sEXPORT_NAME=my_app")
```

The Emscripten toolchain will then build a `my_app.wasm` and `my_app.js` file containing your app's WASM code and JavaScript glue code to load the WASM file and run it. To load and run your app, you need to:

1. Call the `<my_module_name>()` in JavaScript.
2. Optionally create a `<canvas>` element whose `id` matches the effective MiniFB title.
   If it does not exist, the backend will create one and append it to the document, logging a warning.

Example app:

```c
int main() {
    struct mfb_window *window = mfb_open("my_app", 320, 240);
    if (window == NULL)
        return 0;

    uint32_t *buffer = malloc(320 * 240 * 4);

    mfb_update_state state;
    do {
        // TODO: add some fancy rendering to the buffer of size 320 * 240

        state = mfb_update_ex(window, buffer, 320, 240);

        if (state != MFB_STATE_OK)
            break;

    } while(mfb_wait_sync(window));

    free(buffer);
    buffer = NULL;
    window = NULL;

    return 0;
}
```

Assuming the build will generate `my_app.wasm` and `my_app.js`, the simplest `.html` file to load and run the app would look like this:

```html
<html>
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset='utf-8'>
    <meta http-equiv='X-UA-Compatible' content='IE=edge'>
    <meta name='viewport' content='width=device-width, initial-scale=1'>
    <!-- Load the app's .js file -->
    <script src="./my_app.js"></script>
</head>
<body>
<div>
    <canvas id="my_app" style="background: #000;"></canvas>
</div>
<script>
    // Call the app's main() function
    my_app();
</script>
</body>
</html>
```

#### Limitations & caveats

The web backend has the following backend-specific behavior:

- In `mfb_open_ex()`, only fullscreen flags are currently interpreted (`MFB_WF_FULLSCREEN`, `MFB_WF_FULLSCREEN_DESKTOP`); other window flags are ignored
- `mfb_get_monitor_dpi()` / `mfb_get_monitor_scale()` report fixed values (`1.0` scale)
- `mfb_set_target_fps()` / `mfb_get_target_fps()` store/query the target value, but do not currently control browser frame pacing (the browser event loop / RAF timing drives pacing)
- `mfb_show_cursor()` tracks requested visibility, but hiding the browser cursor is not implemented yet

Core rendering, events, viewport and timers are supported.

When calling `mfb_open()` or `mfb_open_ex()`, Web uses the effective title as canvas id.
If `title` is `NULL` or empty, the effective title is `"minifb"`, so the backend looks for `<canvas id="minifb">`.
If a matching canvas is not found, the backend creates one automatically and appends it to the document, and logs a warning.
The functions modify the `width` and `height` attributes of the selected/created `<canvas>`. If not already set, they also modify CSS `width` and `height`.

Setting the CSS width and height of the canvas allows you to up-scale the framebuffer arbitrarily:

```js
// Request a 320x240 window
mfb_open("my_app", 320, 240);

// Up-scale 2x via CSS
<canvas id="my_app" style="width: 640px; height: 480px">
````

If not already set, the backend will also set a handful of CSS styles on the canvas that are good defaults for pixel graphics.

- `image-rendering: pixelated`
- `user-select: none`
- `border: none`
- `outline-style: none`;

### MS-DOS (DJGPP)

Use the `tools/dos/download-dos-tools.sh` file to download all the tools needed to compile, run and debug MiniFB DOS applications. The Bash script will download the following tools:

- [DJGPP](https://www.delorie.com/djgpp/), a GCC fork targeting 32-bit protected mode DOS.
- [GDB 7.1a](https://github.com/badlogic/gdb-7.1a), a GDB fork that can remotely debug 32-bit COFF executables via TCP, running in e.g. DOSBox-x, VirtualBox, or a real machine.
- [DOSBox-x](https://github.com/badlogic/dosbox-x/), a fork of the popular DOS emulator with some modifications to enable remote debugging via GDB.

The tools are downloaded to the `tools/dos/` folder. The folder also contains a DOSBox-x configuration file `dosbox-x.conf` preconfigured for debugging. The `toolchain-djgpp.cmake` file is a CMake toolchain file for DJGPP.

You can optionally run the script with the argument `--with-vs-code`. If you have [Visual Studio Code](https://code.visualstudio.com/) installed, the script will install extensions needed for C/C++ development and debugging, and create a `.vscode` folder in the repository root containing launch configurations, tasks, and various other settings for DOS development in VS Code.

#### Building and running the examples (DOS)

```sh
cmake -DCMAKE_TOOLCHAIN_FILE=./tools/dos/toolchain-djgpp.cmake -B build-dos .
cmake --build build-dos
```

or from the build-dos directory:

```sh
cmake -DCMAKE_TOOLCHAIN_FILE=../tools/dos/toolchain-djgpp.cmake ..
cmake --build .
```

> **Note**: On Windows, you will need a build tool other than Visual Studio. [Ninja](https://ninja-build.org/) is the best and easiest option. Simply download it, put the `ninja.exe` executable somewhere, and make it available on the command line via your `PATH` environment variable. Then invoke the first command above with the addition of `-G Ninja` at the end.

This will generate DOS 32-bit `.exe` files in the `build-dos/` folder which you can run with DOSBox-x like this:

```sh
./tools/dos/dosbox-x/dosbox-x -fastlaunch -exit -conf ./tools/dos/dosbox-x.conf build-dos/<executable-file>
```

Note that the DOS backend cannot support multi-window applications. The examples `multiple-windows.c` and `hidpi.c` will thus not run correctly.

The `dos` example target (`examples/dos/debug_dos.c`) is a GDB-stub debugging sample. In a `Debug` build it calls `gdb_start()` and waits for a debugger connection. If you want a regular visual test, run `noise` or `input_events` instead.

#### Compiling your own MiniFB app for DOS

Copy the folder `examples/dos/` from the MiniFB repository to your project and run the `tools/dos/download-dos-tools.sh` file as described above. Pull in MiniFB via CMake as described above.

Then, when configuring your CMake build, specify the DJGPP toolchain file:

```sh
cmake -DCMAKE_TOOLCHAIN_FILE=./tools/dos/toolchain-djgpp.cmake ... rest of your configure parameters ...
```

The build will then generate DOS 32-bit protected mode executables and use the MiniFB DOS backend. You can run the executables as is in DOSBox-x or FreeDOS, or a Windows version that can run DOS applications.

Running the executables in vanilla MS-DOS requires a DPMI server. Download [CWSDPMI](https://www.ibiblio.org/pub/micro/pc-stuff/freedos/files/distributions/1.2/repos/pkg-html/cwsdpmi.html), extract the ZIP file, and place the `CWSDPMI.EXE` file found in the `BIN/` folder next to your application's executable.

#### Debugging your MiniFB app in DOSBox-x

The MiniFB DOS backend comes with a [GDB stub](https://sourceware.org/gdb/onlinedocs/gdb/Remote-Stub.html) in [`examples/dos/gdbstub.h`](examples/dos/gdbstub.h) that you can incorporate into your application to enable remote debugging your app through GDB. Run the `tools/dos/download-dos-tools.sh` script as described above to get GDB and DOSBox-x versions capable of remote debugging. Then, in the source file that contains your `main()` function, include the `gdbstub.h` file and call the `gdb_start()` and `gdb_checkpoint()` functions like this:

```c
#define GDB_IMPLEMENTATION
#include "gdbstub.h"

int main(void) {
    gdb_start();

    ... setup code ...

    do {
        ... main loop ...
        gdb_checkpoint();
    } while (mfb_wait_sync(window));
}
```

Configure your CMake build with `-DCMAKE_BUILD_TYPE=Debug` to generate debug binaries and build your application.

Run your application with the downloaded DOSBox-x:

```sh
./tools/dos/dosbox-x/dosbox-x -fastlaunch -exit -conf ./tools/dos/dosbox-x.conf path/to/your/executable.exe
```

DOSBox-x will start up, your application will wait for GDB to connect (`gdb_start()`).

Run GDB, load the debugging information from the executable and connect to your app running and waiting in DOSBox-x:

```sh
./tools/dos/gdb/gdb
(gdb) file path/to/your/executable.exe
(gdb) target remote localhost:5123
```

GDB will show your app being halted on the `gdb_start()` line. You can now set breakpoints, step, continue, inspect local variables and so on.

If your app is executing and you press `CTRL+C` to interrupt it, you will end up inside `gdb_checkpoint()`. You can then set breakpoints, or step out to inspect your program state.

Alternatively, you can use VS Code to debug via a graphical user interface. Run the `download-dos-tools.sh` script with the `--with-vs-code` flag. This will install C/C++/CMake VS Code extensions and copy the `tools/dos/.vscode` folder to the project root. Open the project root folder in VS Code, select the `djgpp` [CMake kit](https://vector-of-bool.github.io/docs/vscode-cmake-tools/kits.html), select the `Debug` [CMake variant](https://vector-of-bool.github.io/docs/vscode-cmake-tools/getting_started.html#selecting-a-variant), and the [CMake launch target](https://vector-of-bool.github.io/docs/vscode-cmake-tools/debugging.html#selecting-a-launch-target), then run the `DOS debug target` launch configuration.

You can use both the CLI and GUI method for debugging the MiniFB examples as well. See the example [examples/dos/debug_dos.c](examples/dos/debug_dos.c) for usage of the GDB stub.

## Platform-Specific Limitations

Some MiniFB features are not available on all platforms. Here's a summary of what's supported:

### Feature Support Matrix

| Feature | Windows | macOS | Linux X11 | Wayland | iOS | Android | Web | DOS |
|---------|---------|-------|-----------|---------|-----|---------|-----|-----|
| Window creation | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes |
| mfb_update | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes |
| Keyboard input | Yes | Yes | Yes | Yes | No | Limited | Yes | Limited |
| Mouse input | Yes | Yes | Yes | Yes | Touch | Touch | Yes | Limited |
| Multi-window | Yes | Yes | Yes | Yes | No | No | Yes | No |
| Viewport | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes |
| Cursor hiding | Yes | Yes | Yes | Yes | No-op | No-op | Yes | No-op |
| Monitor DPI / scale | Yes | Yes | Yes* | Yes | Yes | Yes | Yes | Fixed |
| Target FPS | Yes | Yes | Yes | Yes | Yes** | Yes | Limited*** | Limited*** |
| Hardware sync | OpenGL | Metal | OpenGL | - | Metal | - | Browser-driven | - |

`**` On iOS, this applies when using `mfb_wait_sync()`. If your app loop is driven by `CADisplayLink`, pacing is already tied to display refresh.

`***` Web/DOS currently store/report the target FPS value, but frame pacing is not controlled by it.

`*` X11 monitor scale is available, but often behaves as an initial value only; runtime global-scale changes may not be reflected until restart (environment-dependent, especially under XWayland).

For detailed caveats and behavior differences, see each platform section above (iOS, Android, Web, DOS).
