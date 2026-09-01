# MiniFB

MiniFB (Mini FrameBuffer) is a small cross-platform C library. You give it a buffer of 32-bit pixels, it copies that buffer to a window, and it reports keyboard and mouse events back to you. Your code draws everything: MiniFB never draws for you. It only needs the system libraries of each platform.

## Quick Start

The shortest useful program looks like this:

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

1. Create a window with `mfb_open()` or `mfb_open_ex()`.
2. Allocate the buffer yourself: one 32-bit value per pixel, `width * height` pixels. MiniFB never allocates or frees it.
3. Draw into the buffer, then call `mfb_update()` or `mfb_update_ex()` to copy it to the window.
4. Call `mfb_wait_sync()` to wait until it is time to draw the next frame.

`MFB_STATE_OK` means the frame was accepted. `MFB_STATE_EXIT` means the window was closed and destroyed internally: leave the loop and do not use the handle again. The other values report an error for that frame (invalid window, invalid buffer, backend failure) and leave the window alive.

**Note:** By default, releasing the ESC key closes the window, so the next update returns `MFB_STATE_EXIT`. Install your own keyboard callback if you do not want that.

See `examples/noise.c` for a complete example.

## Supported Platforms

| Platform | Backends | Status |
|----------|----------|--------|
| **Windows** | GDI, OpenGL | Full API |
| **macOS** | Cocoa, Metal | Full API |
| **Linux/Unix** | X11, Wayland | Full API |
| **iOS** | Metal | Reduced API: single window, touch input |
| **Android** | Native | Reduced API: single window, touch input |
| **Web** | WASM | Partial: some calls are accepted but not applied |
| **DOS** | DJGPP | Reduced API: single window, partial keyboard |

Build instructions and platform notes: [Windows](#windows), [X11](#x11-freebsd-linux-nix), [Wayland](#wayland-linux), [macOS](#macos), [iOS](#ios), [Android](#android), [Web](#web-wasm), [MS-DOS](#ms-dos-djgpp). [Feature support by platform](#feature-support-by-platform) lists what each backend implements.

MiniFB has been tested on Windows, macOS, Linux, iOS, Android, browsers and DOSBox-x. Results depend on your setup. The library does not convert pixel data: if the system cannot provide a proper 32-bit display, MiniFB does not work around it.

## Features

- Window creation and management
- Event callbacks (keyboard, mouse, window lifecycle)
- Window and input state queries, as an alternative to callbacks
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

`mfb_update_ex()` behaves differently on two backends:
- Wayland waits for the compositor frame callback inside `mfb_update_ex()` (can block). A minimized or hidden window may stop receiving that callback. Each call then waits briefly and returns `MFB_STATE_OK` without presenting.
- Android may return `MFB_STATE_OK` without presenting when `ANativeWindow` is temporarily unavailable during lifecycle transitions.

What is ready when `mfb_open_ex()` returns also depends on the backend:
- Wayland waits for the initial configure handshake before returning from `mfb_open_ex()`.
- Android may return a window handle before `ANativeWindow` is ready (rendering starts once the native window becomes available).

`mfb_open_ex()` flag support by backend:

| Backend | RESIZABLE | BORDERLESS | ALWAYS_ON_TOP | FULLSCREEN | FULLSCREEN_DESKTOP |
|---------|-----------|------------|---------------|------------|--------------------|
| Windows | Yes | Yes | Yes | Yes | Yes |
| X11 | Yes | Yes* | Yes* | Yes* | Yes* |
| Wayland | Yes | Yes | No (ignored, warning) | Yes | Yes (maximized) |
| macOS | Yes | Yes | Yes | Yes | Yes (zoom/maximize) |
| Web | Yes*** | No (ignored, warning) | No (ignored, warning) | Yes** | Yes** |
| DOS | No (ignored, warning) | No (ignored, warning) | No (ignored, warning) | No (ignored, warning) | No (ignored, warning) |
| Android | No (ignored, warning) | No (ignored, warning) | No (ignored, warning) | No (ignored, warning) | No (ignored, warning) |
| iOS | No (ignored, warning) | No (ignored, warning) | No (ignored, warning) | No (ignored, warning) | No (ignored, warning) |

\* Best effort via window-manager hints/properties; behavior depends on compositor/WM support.

\** Browser-managed fullscreen; typically requires a user gesture before entering fullscreen.

\*** The canvas follows its CSS layout box scaled by `devicePixelRatio`, so the page has to give it a relative size. Without the flag the drawing buffer stays pinned to the framebuffer size.

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
void                mfb_set_mouse_enter_callback(struct mfb_window *window, mfb_mouse_enter_func callback);
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

void mouse_enter(struct mfb_window *window, bool is_inside) {
    // Cursor entered or left the window content area
}
```

#### C++ Callback Interface

In C++ you can also point a callback at a class method or a lambda:

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

Read window and input state directly, instead of using callbacks:

```c
// Window state
bool                mfb_is_window_active(struct mfb_window *window);
bool                mfb_is_mouse_inside(struct mfb_window *window);
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

On Android and iOS, touch positions carry the pointer id in their upper bits:

- `mfb_get_mouse_x()`, `mfb_get_mouse_y()` and the `x`/`y` given to `mfb_mouse_move_func` are packed values. Decode them with `mfb_decode_touch()`, or one at a time with `mfb_decode_touch_pos()` / `mfb_decode_touch_id()`. On desktop, Web and DOS the id is always `0`.
- The pointer id also arrives as the `button` argument of `mfb_mouse_button_func` (`MFB_MOUSE_BTN_0`..`MFB_MOUSE_BTN_7`).
- An external mouse on Android is the exception: it reports the real button, like the desktop backends. The device goes in the packed pointer id instead. A mouse or hovering stylus reports `MFB_POINTER_ID_MOUSE`, and fingers take the ids Android hands out, starting at `0`, so `mfb_decode_touch_id(mfb_get_mouse_x(window)) == MFB_POINTER_ID_MOUSE` tells them apart.

`mfb_get_mouse_scroll_x()` and `mfb_get_mouse_scroll_y()` only hold the delta of the last event pump: MiniFB sets them to `0.0f` before pumping events, then writes the delta if a scroll event arrives during that pump.

With more than one window on Windows or macOS the pump drains the whole thread queue, so a scroll belonging to another window can be delivered there and cleared before that window reads it. `mfb_mouse_scroll_func` is not affected and is the reliable source.

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

**Note:** Where MiniFB renders through OpenGL or Metal, `mfb_wait_sync()` relies on vertical sync. The other backends pace frames in software.

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

#### Setting the level from the environment

`MINIFB_LOG_LEVEL` sets the threshold without touching the code:

```sh
MINIFB_LOG_LEVEL=trace ./my_program
```

Use a level name, not a number: `trace`, `debug`, `info`, `warning` or `error`. Case does not matter. Names keep working if the `mfb_log_level` enum is ever reordered, which numbers would not.

**The variable wins over `mfb_set_log_level()`.** This is on purpose. The point of the variable is to get more output from a program you cannot rebuild, and a hardcoded call in that program would otherwise block you. If you need the program to stay in control, do not set the variable.

An unknown value is reported as an error and then ignored, so the threshold stays where the program left it. Nothing changes without a message.

Since the built-in logger writes low levels to `stdout` and high levels to `stderr`, the two streams can interleave out of order when you pipe them into one file. Redirect them separately, or send both to the same place with `2>&1` and accept the ordering.

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
- On X11 the scale comes from Xresources, XRandR or a fallback. In most desktops it is read at startup, so changing the global scale while the program runs may have no effect until you restart it.

If your layout is defined in logical units, convert it to drawable coordinates before calling `mfb_set_viewport()`:

```c
float sx = 1.0f, sy = 1.0f;
mfb_get_monitor_scale(window, &sx, &sy);
unsigned margin_x_viewport = (unsigned) lroundf(margin_logical_x * sx);
unsigned margin_y_viewport = (unsigned) lroundf(margin_logical_y * sy);
```

### Display Insets

Two functions report the display insets from C:

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

    // Clamp: platform values can arrive before the resize settles.
    if (safe_w < 0) safe_w = 0;
    if (safe_h < 0) safe_h = 0;

    // Place important UI inside [safe_x, safe_y, safe_w, safe_h].
}
```

## Adding MiniFB to Your Project

Add the repository as a submodule in your dependencies folder:

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

MiniFB is built with CMake:

```sh
cmake -B build .
cmake --build build
```

The sections below describe what each platform needs and which backend it uses by default.

### CMake Options

| Option | Default | Applies to | What it does |
|--------|---------|------------|--------------|
| `MINIFB_USE_OPENGL_API` | `ON` | Windows, X11 | Render through OpenGL 1.5 instead of GDI or XImage |
| `MINIFB_USE_WAYLAND_API` | `OFF` | Linux | Use the Wayland backend instead of X11 |
| `MINIFB_USE_METAL_API` | `ON` | macOS | Render through Metal instead of Cocoa |
| `MINIFB_USE_INVERTED_Y_ON_MACOS` | `OFF` | macOS | Keep the native macOS mouse origin at the bottom-left |
| `MINIFB_BUILD_EXAMPLES` | `ON` | all | Build the example programs |
| `MINIFB_BUILD_VERSION_INFO` | `ON` | desktop | Build the version info utility (always off on iOS, Android and Emscripten) |

The old names without the `MINIFB_` prefix (`USE_OPENGL_API`, `USE_WAYLAND_API`, `USE_METAL_API`, `USE_INVERTED_Y_ON_MACOS`) still work, but they are deprecated and print a warning when CMake configures the project.

### Windows

CMake generates a Visual Studio project by default. MinGW works as well.

#### OpenGL API backend (Windows)

MiniFB renders through OpenGL instead of GDI by default, because it is faster. The context is OpenGL 1.5, so it needs no shaders and still works on old machines.

One CMake flag turns it on or off:

```sh
cmake .. -DMINIFB_USE_OPENGL_API=ON
# or
cmake .. -DMINIFB_USE_OPENGL_API=OFF
```

### X11 (FreeBSD, Linux, *nix)

#### Dependencies for X11 on Ubuntu/Debian

To build the X11 backend on Ubuntu/Debian, install these packages:

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

Omit `libgl1-mesa-dev` if you want X11 without OpenGL (XImage rendering), and `libxrandr-dev` if you do not need XRandR-assisted scale/DPI detection.

Equivalent packages for other distros:

- Fedora: `gcc`, `cmake`, `pkg-config`, `libX11-devel`, `libxkbcommon-devel`, `mesa-libGL-devel`
- Arch: `base-devel`, `cmake`, `pkgconf`, `libx11`, `libxkbcommon`, `mesa`
- openSUSE: `gcc`, `cmake`, `pkg-config`, `libX11-devel`, `libxkbcommon-devel`, `Mesa-libGL-devel`

#### Building with CMake

X11 is the default backend on Linux, so no flag is needed. To be explicit:

```sh
mkdir build-x11
cd build-x11
cmake .. -DMINIFB_USE_WAYLAND_API=OFF
```

#### OpenGL API backend (X11)

MiniFB renders through OpenGL instead of XImages by default, because it is faster. The context is OpenGL 1.5, so it needs no shaders and still works on old machines.

One CMake flag turns it on or off:

```sh
cmake .. -DMINIFB_USE_OPENGL_API=ON -DMINIFB_USE_WAYLAND_API=OFF
# or
cmake .. -DMINIFB_USE_OPENGL_API=OFF -DMINIFB_USE_WAYLAND_API=OFF
```

### Wayland (Linux)

The Wayland backend needs `wayland-client`, `wayland-cursor` and `xkbcommon`, which CMake locates with `pkg-config`. It covers the same core API as the other desktop backends (Windows, macOS, X11).

#### Dependencies for Wayland on Ubuntu/Debian

To build the Wayland backend on Ubuntu/Debian, install these packages:

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
- **libdecor-0-dev** (optional): Client-side window decorations. See [Window Decorations](#window-decorations)

Equivalent packages for other distros:

- Fedora: `gcc`, `cmake`, `pkg-config`, `wayland-devel`, `libxkbcommon-devel`, `wayland-protocols-devel`
- Arch: `base-devel`, `cmake`, `pkgconf`, `wayland`, `libxkbcommon`, `wayland-protocols`
- openSUSE: `gcc`, `cmake`, `pkg-config`, `wayland-devel`, `libxkbcommon-devel`, `wayland-protocols-devel`

#### Wayland Protocol Compatibility

Distributions ship different versions of Wayland and its protocols. MiniFB includes pre-generated protocol headers and code that work with most setups. If you hit a version mismatch, or you simply want the files to match your system, regenerate them:

```sh
chmod +x ./tools/wayland/generate-protocols.sh
./tools/wayland/generate-protocols.sh
```

The script writes headers and code for the Wayland version installed on your machine.

Then enable the Wayland backend:

```sh
mkdir build-wayland
cd build-wayland
cmake .. -DMINIFB_USE_WAYLAND_API=ON
```

#### Window Decorations

Wayland has no window frames of its own. Who draws the title bar and the borders depends on the compositor:

- If the compositor implements `xdg-decoration`, MiniFB asks it to draw the frame. KDE and most wlroots compositors do this.
- If it does not, MiniFB draws the frame itself with [libdecor](https://gitlab.freedesktop.org/libdecor/libdecor). GNOME needs this path.
- If libdecor is not available either, the window opens without a frame. It still works: you can move and resize it with the compositor's keyboard shortcuts.

libdecor is optional and is never linked. MiniFB opens it with `dlopen` the first time a window needs it, so the same binary runs on machines that do not have it. To build with support, install the development package:

```bash
sudo apt-get install -y libdecor-0-dev     # Fedora: libdecor-devel, Arch: libdecor
```

CMake reports what it found while configuring:

```text
-- libdecor 0.2.2 found, client-side decorations will be loaded at run time
```

libdecor draws the frame through plugins that it also loads at run time (`libdecor-gtk`, `libdecor-cairo`). If the development package is installed but no plugin is, libdecor prints a warning of its own and the window ends up without a frame.

##### Known issue on WSLg

On WSLg, maximizing a window that libdecor decorates leaves the drop shadow of the floating window drawn on top of the maximized one, at the size and position the window had before.

This is not specific to MiniFB. The same artifact appears with GLFW, and it was reported against FLTK in [microsoft/wslg#914](https://github.com/microsoft/wslg/issues/914), open since 2022. MiniFB removes the shadow when the window is maximized, and the compositor confirms it by reporting that the surface left every output, but WSLg keeps drawing it. Native Linux compositors do not show the problem. Resizing the window by hand clears it.

#### Wayland Testing and Diagnostics

Which code path the Wayland backend takes depends on the compositor it runs against. MiniFB binds each protocol global at the lowest version the compositor, libwayland and the build headers all support. If a global is missing, or its version is too low for some event, MiniFB falls back to other code.

One machine only ever gives you one of those combinations. Two environment variables let you test the others without changing compositor:

| Variable | What it does | Example |
| --- | --- | --- |
| `MINIFB_WAYLAND_FORCE_VERSIONS` | Lowers the version used for one or more interfaces | `wl_seat=4,wl_output=1` |
| `MINIFB_WAYLAND_DISABLE_GLOBALS` | Hides globals, as if the compositor never offered them | `wp_viewporter` |

Both work in every build, not only in debug builds. MiniFB reads them once, while it binds globals, so they cost nothing after the window opens. Every override is written to the log, and a value that cannot be applied is reported and ignored rather than dropped in silence.

They combine with libwayland's own `WAYLAND_DEBUG=client`, which prints all protocol traffic to stderr:

```sh
MINIFB_WAYLAND_FORCE_VERSIONS="wl_seat=4" WAYLAND_DEBUG=client ./my_program 2> trace.txt
```

Together these answer the two halves of one question: the trace shows what the compositor sent, and your program's output shows what MiniFB made of it.

See [docs/wayland-testing.md](docs/wayland-testing.md) for the interfaces each variable accepts, the versions actually worth testing and what each one covers, the other useful variables from libwayland and xkbcommon, and what this approach cannot test.

### macOS

You need Xcode and its command line tools, which provide clang and the Cocoa framework.

On macOS Mojave and later, the Cocoa framework no longer behaves as MiniFB expects, so the library renders through Metal by default:

```sh
mkdir build-macos-metal
cd build-macos-metal
cmake .. -DMINIFB_USE_METAL_API=ON
```

To use Cocoa instead:

```sh
mkdir build-macos-cocoa
cd build-macos-cocoa
cmake .. -DMINIFB_USE_METAL_API=OFF
```

#### Coordinate system

macOS places the mouse origin (0, 0) at (left, bottom). MiniFB flips it to (left, top) so that every platform behaves the same way.

To keep the native macOS origin, use the CMake flag `MINIFB_USE_INVERTED_Y_ON_MACOS=ON`:

```sh
mkdir build-macos-inverted-y
cd build-macos-inverted-y
cmake .. -DMINIFB_USE_INVERTED_Y_ON_MACOS=ON
```

**Note**: a global option affecting every platform (probably `-DUSE_INVERTED_Y`) may replace this one later.

### iOS

It works with and without a `UIWindow`. If you create the window/view hierarchy through Storyboard, set the `UIViewController` to `iOSViewController` and the root `UIView` to `iOSView`.

**Launch screen / storyboard requirement**:

For App Store distribution, Apple requires a launch storyboard (legacy static launch images are deprecated). Without a launch storyboard, iOS can start in a compatibility layout and you may see top/bottom black bands or an incorrect initial drawable size.

That is why there are two iOS example targets:

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

iOS calls `mfb_set_active_callback()` from the app lifecycle notifications (active/inactive transitions). It calls `mfb_set_close_callback()` only as a termination notice: iOS does not let an app cancel its own termination.

`mfb_set_target_fps()` and `mfb_get_target_fps()` work on iOS for software pacing through `mfb_wait_sync()`. If your app runs on `CADisplayLink` (like the example), iOS paces the frames instead.

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

### Android

See the example in `examples/android`. You need **Android Studio** to build and run it.

**Limitations**:

- No general keyboard/char-input callback support yet
- Single window only (flags to `mfb_open_ex()` are ignored)
- `mfb_show_cursor()` is a no-op (no cursor concept on touch devices)
- No dedicated multitouch API; touches are mapped to mouse callbacks (`MFB_MOUSE_BTN_0`..`MFB_MOUSE_BTN_7`)
- Mouse events represent touch events (last processed touch position)
- Touch pointer id is packed into upper bits of `mfb_get_mouse_x()` / `mfb_get_mouse_y()` values; decode with `mfb_decode_touch()` / `mfb_decode_touch_pos()` / `mfb_decode_touch_id()`
- `mfb_get_monitor_scale()` reports Android density scale (same value for X/Y, from `AConfiguration_getDensity()` with `160dpi = 1.0`)

**Note**: pressing `BACK` closes the app by default. Some emulators map right-click to `BACK`. To debug that case, enable the Android example CMake option `MINIFB_ANDROID_CAPTURE_RIGHT_CLICK_AS_ESC` (default `OFF`), which maps `BACK` to `ESC` instead.

`mfb_set_target_fps()` and `mfb_get_target_fps()` work on Android for software pacing through `mfb_wait_sync()`.

All other MiniFB functions work normally, including timers, viewports, and user data management.

#### Pixel format on Android

MiniFB uses a **32-bit pixel buffer** on all platforms, but the byte order in memory differs between Android and desktop/iOS:

| Platform | Byte order in memory | Equivalent uint32_t (LE) |
|----------|----------------------|--------------------------|
| Desktop (Windows, Linux, macOS) | B · G · R · X | `0x00RRGGBB` |
| iOS | B · G · R · A | `0x00RRGGBB` |
| **Android** | **R · G · B · X** | **`0x00BBGGRR`** |

**You do not need to think about this** if you build pixels with the `MFB_RGB` / `MFB_ARGB` macros: they expand to the correct layout on every platform:

```c
buffer[i] = MFB_RGB(255, 0, 0);   // always displays red, on every platform
```

**Where it matters: external pixel data.** If you load an image with a library that always produces RGBA bytes in memory (e.g. `stb_image`, `libpng`, browser canvas), and you pass that data directly to `mfb_update_ex`, the colors will be correct on Android but **red and blue will be swapped on desktop/iOS** (and vice-versa if you adapt for desktop).

```c
// stb_image / libpng give RGBA bytes in memory:
//   byte[0]=R  byte[1]=G  byte[2]=B  byte[3]=A

// On Android this is exactly what ANativeWindow expects, pass as-is.
// On desktop/iOS you must swap R <-> B before calling mfb_update_ex.
```

**Why can't Android just accept the same format as desktop?** `ANativeWindow` (the Android NDK surface API) does not expose a BGRA format in its public interface, only `WINDOW_FORMAT_RGBX_8888` (RGBA bytes) and `WINDOW_FORMAT_RGB_565` are guaranteed on all devices. Swizzling the whole buffer inside the library would cost CPU time on every frame. MiniFB avoids that by adjusting the macros at compile time.

#### Display cutout / Notch (API 32-34)

Android's handling of the display cutout (notch, punch-hole camera) changed across API levels and can cause a framebuffer-size mismatch if not handled explicitly:

| API level | Default behaviour                               | Result                        |
|-----------|-------------------------------------------------|-------------------------------|
| ≤ 31      | Legacy fullscreen flags handle everything       | Works out of the box          |
| 32-34     | System reserves space for the cutout by default | **Content shifted / clipped** |
| ≥ 35      | Edge-to-edge is forced by the OS                | Works out of the box          |

The example (`examples/android/native2026`) shows two approaches; pick the one that fits your project.

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

- **Pros**: zero Java code, takes effect before the native thread starts.
- **Cons**: limited runtime control; no way to query inset values from C.

##### Option B - Java subclass (recommended)

Subclass `NativeActivity` in `MiniFBActivity.java` and override `onCreate` / `onWindowFocusChanged` to call `setupFullscreen()`, which:

- Sets `LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS` (API 31+) or `SHORT_EDGES` (API 28-30).
- Hides system bars via `WindowInsetsController` (API 30+) or the legacy `setSystemUiVisibility` flags (API 24-29).
- Re-applies on focus changes (bars can reappear after an edge-swipe gesture).

In `AndroidManifest.xml` replace the activity class name:

```xml
<activity
    android:name="com.example.noise.MiniFBActivity"
    android:theme="@style/FullscreenNative"
    ...>
```

The theme is kept as an early fallback; the Java code overrides it once the Activity starts.

- **Pros**: robust, handles all API levels, re-applies after gesture-triggered bar visibility.
- **Cons**: requires one Java source file.

Both options can coexist (the theme fires first, the Java code reinforces it).

#### Querying Insets from C

The display inset APIs are backend-agnostic: `mfb_get_display_cutout_insets()` and `mfb_get_display_safe_insets()`.

See [Display Insets](#display-insets) in the API reference for exact semantics, return contract, and backend behavior details.

### Web (WASM)

Download and install [Emscripten](https://emscripten.org/), then point CMake at the Emscripten toolchain file and build as usual.

#### Building and running the examples (WASM)

```sh
cmake -DCMAKE_TOOLCHAIN_FILE=/path/to/emsdk/<version>/emscripten/cmake/Modules/Platform/Emscripten.cmake -B build-web .
cmake --build build-web
```

On Windows you cannot use the default Visual Studio generator, because Emscripten brings its own toolchain based on a modified Clang. Generate MinGW makefiles instead:

```sh
cmake -DCMAKE_TOOLCHAIN_FILE=C:\Path\to\emsdk\<version>\upstream\emscripten\cmake\Modules\Platform\Emscripten.cmake -G "MinGW Makefiles" -B build-web .
cmake --build build-web
```

> **Note**: On Windows, you will need a build tool other than Visual Studio. [Ninja](https://ninja-build.org/) is the best and easiest option. Simply download it, put the `ninja.exe` executable somewhere in your path, and make it available on the command line via your `PATH` environment variable. Then invoke the first command above with the addition of `-G Ninja` at the end.

Then open the file `build-web/index.html` in your browser to view the example index.

The examples are built with the Emscripten flag `-sSINGLE_FILE`, which merges the `.js` and `.wasm` output into a single `.js` file. Without that flag you cannot open the `.html` file from disk: the build output has to be served over HTTP. The simplest way is Python's `http.server` module:

```sh
python3 -m http.server --directory build-web
```

You can then open the index at [http://localhost:8000](http://localhost:8000) in your browser.

#### Integrating a MiniFB app in a website

To build an executable target for the web, you need to add a linker option specifying its module name, e.g.:

```cmake
target_link_options(my_app PRIVATE "-sEXPORT_NAME=my_app")
```

The Emscripten toolchain then builds `my_app.wasm` plus a `my_app.js` file with the glue code that loads the WASM file and runs it. To load and run your app:

1. Call the `<my_module_name>()` in JavaScript.
2. Optionally create a `<canvas>` element whose `id` matches the effective MiniFB title. If it does not exist, the backend will create one and append it to the document, logging a warning.

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

Assuming the build generates `my_app.wasm` and `my_app.js`, the simplest `.html` file to load and run the app looks like this:

```html
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

#### Canvas size and resizing

The canvas has two independent sizes. `canvas.width` and `canvas.height` are the
drawing buffer in real pixels. The CSS `width` and `height` are the layout box the
browser paints it into. MiniFB uses one or the other depending on the window flags.

**Without `MFB_WF_RESIZABLE`** the backend forces the drawing buffer to the
framebuffer size on every update. Page CSS cannot change it, and the window never
resizes. This is the default and gives a pixel exact canvas.

**With `MFB_WF_RESIZABLE`** the drawing buffer follows the CSS layout box,
multiplied by `devicePixelRatio` so it stays sharp on HiDPI screens. A
`ResizeObserver` reports every change through the resize callback. Give the canvas
a relative CSS size, otherwise the layout box never changes and no resize is ever
reported:

```html
<!-- resizes with the window -->
<canvas id="my_app" style="width: 90vw; height: 70vh"></canvas>

<!-- never resizes: a fixed size in px is the same as not being resizable -->
<canvas id="my_app" style="width: 640px; height: 480px"></canvas>
```

MiniFB stretches the framebuffer to fill the whole canvas, exactly as the X11 and
Windows backends do. So a resizable window has to deal with aspect ratio, and there
are two ways:

1. **Reallocate the framebuffer** in the resize callback, to the size the callback
   reports. The buffer then always matches the canvas and nothing is ever
   stretched. Any CSS size works. `examples/noise.c` and `examples/timer.c` do
   this.
2. **Keep a fixed framebuffer** and give the canvas the same aspect ratio, so the
   stretch is uniform:

   ```html
   <canvas id="my_app" style="width: min(90vw, 93vh); aspect-ratio: 4 / 3"></canvas>
   ```

   The `93vh` is derived, not arbitrary. At 4:3 the height is 75% of the width, so
   capping the height at `70vh` caps the width at `70 / 0.75 = 93vh`. The `min()`
   applies whichever limit is tighter, so the canvas never overflows a wide short
   viewport or a narrow tall one.

   As an alternative, call `mfb_set_viewport_best_fit()` to letterbox the
   framebuffer inside a canvas of any shape.

#### Limitations & caveats

The web backend behaves differently in these areas:

- In `mfb_open_ex()`, only `MFB_WF_RESIZABLE` and the fullscreen flags (`MFB_WF_FULLSCREEN`, `MFB_WF_FULLSCREEN_DESKTOP`) are interpreted; `MFB_WF_BORDERLESS` and `MFB_WF_ALWAYS_ON_TOP` are ignored
- A resize clears the canvas, so a resize handled from `mfb_update_events()` leaves it blank until the application paints again (explained below)
- `mfb_set_target_fps()` / `mfb_get_target_fps()` store/query the target value, but do not currently control browser frame pacing (the browser event loop / RAF timing drives pacing)

Resizing writes `canvas.width`, and the browser clears the canvas whenever that
attribute is written. `mfb_update()` resizes before it paints, so the new frame
hides the clear. `mfb_update_events()` has no framebuffer to paint with, so the
canvas stays blank until the application paints again. `examples/input_events.c`
shows this: it stops painting when the canvas loses focus, so resizing the browser
window then blanks the canvas until you click it. Other backends behave the same
way, because a window that does not repaint after a resize also shows invalid
content.

Core rendering, events, viewport and timers are supported. `mfb_get_monitor_scale()` returns `window.devicePixelRatio`, and `mfb_show_cursor()` hides the canvas cursor with `cursor: none`.

When calling `mfb_open()` or `mfb_open_ex()`, Web uses the effective title as canvas id. If `title` is `NULL` or empty, the effective title is `"minifb"`, so the backend looks for `<canvas id="minifb">`. If a matching canvas is not found, the backend creates one automatically and appends it to the document, and logs a warning.

The functions modify the `width` and `height` attributes of the selected/created `<canvas>`. If not already set, they also modify CSS `width` and `height`.

The CSS width and height of the canvas scale the framebuffer to any size. For example, to show a 320x240 window at double size:

```c
mfb_open("my_app", 320, 240);
```

```html
<canvas id="my_app" style="width: 640px; height: 480px">
```

If they are not set already, the backend also applies a few CSS defaults that suit pixel graphics:

- `image-rendering: pixelated`
- `user-select: none`
- `border: none`
- `outline-style: none`;

### MS-DOS (DJGPP)

Run `tools/dos/download-dos-tools.sh` to download everything needed to compile, run and debug MiniFB DOS applications:

- [DJGPP](https://www.delorie.com/djgpp/), a GCC fork targeting 32-bit protected mode DOS.
- [GDB 7.1a](https://github.com/badlogic/gdb-7.1a), a GDB fork that can remotely debug 32-bit COFF executables via TCP, running in e.g. DOSBox-x, VirtualBox, or a real machine.
- [DOSBox-x](https://github.com/badlogic/dosbox-x/), a fork of the popular DOS emulator with some modifications to enable remote debugging via GDB.

The tools are downloaded to the `tools/dos/` folder. The folder also contains a DOSBox-x configuration file `dosbox-x.conf` preconfigured for debugging. The `toolchain-djgpp.cmake` file is a CMake toolchain file for DJGPP.

Run the script with `--with-vs-code` if you use [Visual Studio Code](https://code.visualstudio.com/): it installs the extensions needed for C/C++ development and debugging, and creates a `.vscode` folder in the repository root with launch configurations, tasks and other settings for DOS development.

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

The DOS backend cannot support multi-window applications, so the examples `multiple_windows.c` and `hidpi.c` do not run correctly.

It also does not tell extended (`E0`-prefixed) scancodes apart from their base ones: the keypad and cursor-block keys that share a scancode report the same `mfb_key`, and right Ctrl and Alt report as the left ones.

Some DOS mouse drivers emulate a wheel by injecting extended Up/Down keys. Reading those as scroll costs the arrow keys, which send the same scancodes, so it is off by default. Define `MINIFB_DOS_WHEEL_FROM_ARROW_KEYS` when building MiniFB to turn it on.

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

The MiniFB DOS backend comes with a [GDB stub](https://sourceware.org/gdb/onlinedocs/gdb/Remote-Stub.html) in [`examples/dos/gdbstub.h`](examples/dos/gdbstub.h) that you can incorporate into your application to enable remote debugging your app through GDB.

Run the `tools/dos/download-dos-tools.sh` script as described above to get GDB and DOSBox-x versions capable of remote debugging. Then, in the source file that contains your `main()` function, include the `gdbstub.h` file and call the `gdb_start()` and `gdb_checkpoint()` functions like this:

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

DOSBox-x starts up and your application waits inside `gdb_start()` for GDB to connect.

Run GDB, load the debugging information from the executable and connect to your app running and waiting in DOSBox-x:

```sh
./tools/dos/gdb/gdb
(gdb) file path/to/your/executable.exe
(gdb) target remote localhost:5123
```

GDB will show your app being halted on the `gdb_start()` line. You can now set breakpoints, step, continue, inspect local variables and so on.

If your app is executing and you press `CTRL+C` to interrupt it, you will end up inside `gdb_checkpoint()`. You can then set breakpoints, or step out to inspect your program state.

You can also debug from VS Code, with a graphical interface. Run the `download-dos-tools.sh` script with the `--with-vs-code` flag: it installs the C/C++/CMake extensions and copies the `tools/dos/.vscode` folder to the project root.

Then open the project root folder in VS Code and:

1. Select the `djgpp` [CMake kit](https://vector-of-bool.github.io/docs/vscode-cmake-tools/kits.html).
2. Select the `Debug` [CMake variant](https://vector-of-bool.github.io/docs/vscode-cmake-tools/getting_started.html#selecting-a-variant).
3. Select the [CMake launch target](https://vector-of-bool.github.io/docs/vscode-cmake-tools/debugging.html#selecting-a-launch-target).
4. Run the `DOS debug target` launch configuration.

You can use both the CLI and GUI method for debugging the MiniFB examples as well. See the example [examples/dos/debug_dos.c](examples/dos/debug_dos.c) for usage of the GDB stub.

## Feature Support by Platform

Not every feature exists on every platform. This table summarizes what each backend does:

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

`*` X11 reports a monitor scale, but usually only the value read at startup. A global scale change while the program runs may not be visible until restart (it depends on the environment, especially under XWayland).

`**` On iOS this applies when you call `mfb_wait_sync()`. If your loop runs on `CADisplayLink`, pacing already follows the display refresh.

`***` Web and DOS store and report the target FPS, but do not use it to pace frames.

For the details behind each entry, see the platform sections above.

## Versioning

MiniFB had no official release version for many years. This codebase adopts SemVer and takes **v0.9.0** as the baseline version.

The build generates `minifb_version.h` at configure time and installs it with the public headers. It gives C and C++ code:

- `MINIFB_VERSION_STRING` and the major/minor/patch macros
- the packed `MINIFB_VERSION_NUMERIC` and its extraction helpers
- Git metadata when available: `MINIFB_COMMIT_COUNT`, `MINIFB_COMMITS_SINCE_TAG`, `MINIFB_GIT_SHA`, `MINIFB_GIT_DIRTY`

Building from a source archive without `.git` still works: the SHA becomes `unknown` and the counters stay at `0`.
