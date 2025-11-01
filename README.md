# MiniFB

MiniFB (Mini FrameBuffer) is a small cross platform library that makes it easy to render (32-bit) pixels in a window.

An example is the best way to show how it works:

```c
struct mfb_window *window = mfb_open_ex("my display", 800, 600, WF_RESIZABLE);
if (!window)
    return 0;

buffer = (uint32_t *) malloc(800 * 600 * 4);

do {
    int state;

    // TODO: add some fancy rendering to the buffer of size 800 * 600

    state = mfb_update_ex(window, buffer, 800, 600);

    if (state < 0) {
        window = NULL;
        break;
    }
} while(mfb_wait_sync(window));
```

1. First the application creates a **window** calling **mfb_open** or **mfb_open_ex**.
2. Next it's the application responsibility to allocate a buffer to work with.
3. Next calling **mfb_update** or **mfb_update_ex** the buffer will be copied over to the window and displayed. If this function return a value lower than 0 the window will have been destroyed internally and cannot be used anymore.
4. Last the code waits to synchronize with the monitor calling **mfb_wait_sync**.

Note that, by default, if ESC key is pressed **mfb_update** / **mfb_update_ex** will return -1 (and the window will have been destroyed internally).

See <https://github.com/emoon/minifb/blob/master/tests/noise.c> for a complete example.

**Supported Platforms**:

- Windows
- MacOS X
- X11 (FreeBSD, Linux, *nix)
- Wayland (Linux) [there are some issues]
- iOS (beta)
- Android (beta)
- Web (WASM) (beta)
- DOS (DJGPP) (beta)

MiniFB has been tested on Windows, Mac OS X, Linux, iOS, Android, web, and DOSBox-x but may of course have trouble depending on your setup. Currently the code will not do any converting of data if not a proper 32-bit display can be created.

**Features**:

- Window creation
- Callbacks to window events
- Get information from windows
- Add per window data
- Timers and target FPS
- C and C++ interface

## Callbacks to window events

You can _add callbacks to the windows_:

```c
void active(struct mfb_window *window, bool isActive) {
    ...
}

void resize(struct mfb_window *window, int width, int height) {
    ...
    // Optionally you can also change the viewport size
    mfb_set_viewport(window, x, y, width, height);
    // or let mfb caclculate the best fit from your original framebuffer size
    mfb_set_viewport_best_fit(window, old_width, old_height);

}

bool close(struct mfb_window *window) {
    ...
    return true;    // true => confirm close
                    // false => don't close
}

void keyboard(struct mfb_window *window, mfb_key key, mfb_key_mod mod, bool isPressed) {
    ...
    // Remember to close the window in some way
    if(key == KB_KEY_ESCAPE) {
        mfb_close(window);
    }
}

void char_input(struct mfb_window *window, unsigned int charCode) {
    ...
}

void mouse_btn(struct mfb_window *window, mfb_mouse_button button, mfb_key_mod mod, bool isPressed) {
    ...
}

// Use wisely this event. It can be sent too often
void mouse_move(struct mfb_window *window, int x, int y) {
    ...
}

// Mouse wheel
void mouse_scroll(struct mfb_window *window, mfb_key_mod mod, float deltaX, float deltaY) {
    ...
}


int main(int argc, char argv[]) {

    struct mfb_window *window = mfb_open_ex("my display", 800, 600, WF_RESIZABLE);
    if (!window)
        return 0;

    mfb_set_active_callback(window, active);
    mfb_set_resize_callback(window, resize);
    mfb_set_close_callback(window, close);
    mfb_set_keyboard_callback(window, keyboard);
    mfb_set_char_input_callback(window, char_input);
    mfb_set_mouse_button_callback(window, mouse_btn);
    mfb_set_mouse_move_callback(window, mouse_move);
    mfb_set_mouse_scroll_callback(window, mouse_scroll);

    ...
}
```

### C++ event interface

If you are using C++ you can set the callbacks to a class, or use lambda expressions:

```cpp
struct Events {
    void active(struct mfb_window *window, bool isActive) {
        ...
    }
    ...
}

int main(int argc, char argv[]) {
    Events e;

    // Using object and pointer to member
    mfb_set_active_callback(window, &e, &Events::active);

    // Using std::bind
    mfb_set_active_callback(std::bind(&Events::active, &e, _1, _2), window);

    // Using a lambda
    mfb_set_active_callback([](struct mfb_window *window, bool isActive) {
        ...
    }, window);

    ...
}

```

## Get information from windows (direct interface)

If you don't want to use callbacks, you can _get information about the window events directly_:

```c
bool                mfb_is_window_active(struct mfb_window *window);

unsigned            mfb_get_window_width(struct mfb_window *window);
unsigned            mfb_get_window_height(struct mfb_window *window);

int                 mfb_get_mouse_x(struct mfb_window *window);             // Last mouse pos X
int                 mfb_get_mouse_y(struct mfb_window *window);             // Last mouse pos Y

float               mfb_get_mouse_scroll_x(struct mfb_window *window);      // Mouse wheel X as a sum. When you call this function it resets.
float               mfb_get_mouse_scroll_y(struct mfb_window *window);      // Mouse wheel Y as a sum. When you call this function it resets.

const uint8_t *     mfb_get_mouse_button_buffer(struct mfb_window *window); // One byte for every button. Press (1), Release 0. (up to 8 buttons)

const uint8_t *     mfb_get_key_buffer(struct mfb_window *window);          // One byte for every key. Press (1), Release 0.
```

## Add per window data

Additionally you can _set per window data and recover it_:

```c
mfb_set_user_data(window, (void *) myData);
...
myData = (someCast *) mfb_get_user_data(window);
```

## Timers and target FPS

You can create timers for your own purposes.

```c
struct mfb_timer *  mfb_timer_create();
void                mfb_timer_destroy(struct mfb_timer *tmr);

void                mfb_timer_reset(struct mfb_timer *tmr);
double              mfb_timer_now(struct mfb_timer *tmr);
double              mfb_timer_delta(struct mfb_timer *tmr);

double              mfb_timer_get_frequency();
double              mfb_timer_get_resolution();
```

Furthermore you can set (and get) a target fps for the application. The default is 60 frames per second.

```c
void                mfb_set_target_fps(uint32_t fps);
unsigned            mfb_get_target_fps();
```

This avoid the problem of update too fast the window collapsing the redrawing in fast processors.

Note: OpenGL and iOS have hardware support for syncing. Other systems will use software syncing. Including MacOS Metal.

In order to be able to use it you need to call the function:

```c
bool                mfb_wait_sync(struct mfb_window *window);
```

Note that if you have several windows running on the same thread it makes no sense to wait them all...

## Cursor Control

### Hiding the cursor

Yow can Show/Hide the mouse cursor while it hovers over your window.

```c
void mfb_show_cursor(struct mfb_window *window, bool show);
```

_Note: **AS OF RIGHT NOW** this is only implemented for Windows and X11_

## Build instructions

The current build system is **CMake**.

Initially MiniFB used tundra [https://github.com/deplinenoise/tundra](https://github.com/deplinenoise/tundra) as build system and it was required to build the code (but now is not maintained).

In any case, not many changes should be needed if you want to use MiniFB directly in your own code.

### MacOS X

Cocoa and clang is assumed to be installed on the system (downloading latest XCode + installing the command line tools should do the trick).

Note that MacOS X Mojave+ does not support Cocoa framework as expected. For that reason you can switch to Metal API.
To enable it just compile defining the preprocessor macro USE_METAL_API.

If you use **CMake** just enable the flag:

```sh
mkdir build
cd build
cmake .. -DUSE_METAL_API=ON
```

or if you don't want to use Metal API:

```sh
mkdir build
cd build
cmake .. -DUSE_METAL_API=OFF
```

#### Coordinate system

On MacOS, the default mouse coordinate system is (0, 0) -> (left, bottom). But as we want to create a multiplatform library we inverted the coordinates in such a way that now (0, 0) -> (left, top), like in the other platforms.

In any case, if you want to get the default coordinate system you can use the CMake flag: USE_INVERTED_Y_ON_MACOS=ON

```sh
mkdir build
cd build
cmake .. -DUSE_INVERTED_Y_ON_MACOS=ON
```

**Note**: In the future, we may use a global option so that all platforms behave in the same way. Probably: -DUSE_INVERTED_Y

if you use **tundra**:

```sh
tundra2 macosx-clang-debug
```

and you should be able to run the noise example (t2-output/macosx-clang-debug-default/noise).

### iOS (beta)

It works with and without an UIWindow created.
If you want to create the UIWindow through an Story Board, remember to set the UIViewController as iOSViewController and the UIView as iOSView.

**Issues**:

- It seems that you have to manually set 'tvOS Deployment Target' to less than 13.
- It seems that you have to manually set 'Launch Screen File' in project > executable > general to be able to get the real device height.
- You need to manually set the 'Signing Team' and 'Bundle Identifier'.
- No multitouch is available yet.
- As this version uses Metal API it cannot be run in the emulator.

**Functions**:

Some of the MiniFB functions don't make sense on mobile.
The available functions for iOS are:

```c
struct mfb_window * mfb_open(const char *title, unsigned width, unsigned height);
struct mfb_window * mfb_open_ex(const char *title, unsigned width, unsigned height, unsigned flags);    // flags ignored

mfb_update_state    mfb_update(struct mfb_window *window, void *buffer);

void                mfb_close(struct mfb_window *window);

void                mfb_set_user_data(struct mfb_window *window, void *user_data);
void *              mfb_get_user_data(struct mfb_window *window);

bool                mfb_set_viewport(struct mfb_window *window, unsigned offset_x, unsigned offset_y, unsigned width, unsigned height);

bool                mfb_set_viewport_best_fit(struct mfb_window *window, unsigned old_width, unsigned old_height);

void                mfb_set_mouse_button_callback(struct mfb_window *window, mfb_mouse_button_func callback);
void                mfb_set_mouse_move_callback(struct mfb_window *window, mfb_mouse_move_func callback);
void                mfb_set_resize_callback(struct mfb_window *window, mfb_resize_func callback);
void                mfb_set_close_callback(struct mfb_window *window, mfb_close_func callback);

unsigned            mfb_get_window_width(struct mfb_window *window);
unsigned            mfb_get_window_height(struct mfb_window *window);
int                 mfb_get_mouse_x(struct mfb_window *window);             // Last mouse pos X
int                 mfb_get_mouse_y(struct mfb_window *window);             // Last mouse pos Y
const uint8_t *     mfb_get_mouse_button_buffer(struct mfb_window *window); // One byte for every button. Press (1), Release 0. (up to 8 buttons)

void                mfb_get_monitor_scale(struct mfb_window *window, float *scale_x, float *scale_y)
// [Deprecated] Use mfb_get_monitor_scale instead
void                mfb_get_monitor_dpi(struct mfb_window *window, float *dpi_x, float *dpi_y)
```

Timers are also available.

```c
struct mfb_timer *  mfb_timer_create(void);
void                mfb_timer_destroy(struct mfb_timer *tmr);
void                mfb_timer_reset(struct mfb_timer *tmr);
double              mfb_timer_now(struct mfb_timer *tmr);
double              mfb_timer_delta(struct mfb_timer *tmr);
double              mfb_timer_get_frequency(void);
double              mfb_timer_get_resolution(void);
```

For now, no multitouch is available.

**Example**:

```objective-c
- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
    if(g_window == 0x0) {
        g_width  = [UIScreen mainScreen].bounds.size.width;
        g_height = [UIScreen mainScreen].bounds.size.height;
        g_window = mfb_open("noise", g_width, g_height);
        if(g_window != 0x0) {
            g_buffer = malloc(g_width * g_height * 4);
        }
    }

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
    if(g_buffer != 0x0) {
        // Do your wonderful rendering stuff
    }

    mfb_update_state state = mfb_update(g_window, g_buffer);
    if (state != STATE_OK) {
        free(g_buffer);
        g_buffer  = 0x0;
        g_width   = 0;
        g_height  = 0;
    }
}
```

**CMake**:

```sh
mkdir build
cd build
cmake -G Xcode -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 ..
```

### Android (beta)

Take a look at the example in tests/android. You need **Android Studio** to build and run it.

**Functions**:

Some of the MiniFB functions don't make sense on mobile.
The available functions for Android are:

```c
struct mfb_window * mfb_open(const char *title, unsigned width, unsigned height);
struct mfb_window * mfb_open_ex(const char *title, unsigned width, unsigned height, unsigned flags);    // flags ignored

mfb_update_state    mfb_update(struct mfb_window *window, void *buffer);

void                mfb_close(struct mfb_window *window);

void                mfb_set_user_data(struct mfb_window *window, void *user_data);
void *              mfb_get_user_data(struct mfb_window *window);

bool                mfb_set_viewport(struct mfb_window *window, unsigned offset_x, unsigned offset_y, unsigned width, unsigned height);

bool                mfb_set_viewport_best_fit(struct mfb_window *window, unsigned old_width, unsigned old_height);

void                mfb_set_active_callback(struct mfb_window *window, mfb_active_func callback);
void                mfb_set_mouse_button_callback(struct mfb_window *window, mfb_mouse_button_func callback);
void                mfb_set_mouse_move_callback(struct mfb_window *window, mfb_mouse_move_func callback);
void                mfb_set_resize_callback(struct mfb_window *window, mfb_resize_func callback);
void                mfb_set_close_callback(struct mfb_window *window, mfb_close_func callback);

bool                mfb_is_window_active(struct mfb_window *window);
unsigned            mfb_get_window_width(struct mfb_window *window);
unsigned            mfb_get_window_height(struct mfb_window *window);
int                 mfb_get_mouse_x(struct mfb_window *window);             // Last mouse pos X
int                 mfb_get_mouse_y(struct mfb_window *window);             // Last mouse pos Y
const uint8_t *     mfb_get_mouse_button_buffer(struct mfb_window *window); // One byte for every button. Press (1), Release 0. (up to 8 buttons)
```

Timers are also available.

```c
struct mfb_timer *  mfb_timer_create(void);
void                mfb_timer_destroy(struct mfb_timer *tmr);
void                mfb_timer_reset(struct mfb_timer *tmr);
double              mfb_timer_now(struct mfb_timer *tmr);
double              mfb_timer_delta(struct mfb_timer *tmr);
double              mfb_timer_get_frequency(void);
double              mfb_timer_get_resolution(void);
```

### Windows

If you use **CMake** the Visual Studio project will be generated (2015, 2017 and 2019 have been tested).

Furthermore you can also use **MinGW** instead of Visual Studio.

if you use **tundra**:

Visual Studio (ver 2012 express has been tested) tools needed (using the vcvars32.bat (for 32-bit) will set up the enviroment) to build run:

```sh
tundra2 win32-msvc-debug
```

and you should be able to run noise in t2-output/win32-msvc-debug-default/noise.exe

#### OpenGL API backend (Windows)

Now, by default, OpenGL backend is used, instead of Windows GDI, because it is faster. To maintain compatibility with old computers an OpenGL 1.5 context is created (no shaders needed).

To enable or disable OpenGL just use a CMake flag:

```sh
cmake .. -DUSE_OPENGL_API=ON
# or
cmake .. -DUSE_OPENGL_API=OFF
```

### X11 (FreeBSD, Linux, *nix)

gcc and x11-dev libs needs to be installed.

If you use **CMake** just disable the flag:

```sh
mkdir build
cd build
cmake .. -DUSE_WAYLAND_API=OFF
```

If you use **tundra**:

To build the code run:

```sh
tundra2 x11-gcc-debug
```

and you should be able to run t2-output/x11-gcc-debug-default/noise

#### OpenGL API backend (X11)

Now, by default, OpenGL backend is used instead of X11 XImages because it is faster. To maintain compatibility with old computers an OpenGL 1.5 context is created (no shaders needed).

To enable or disable OpenGL just use a CMake flag:

```sh
cmake .. -DUSE_OPENGL_API=ON -DUSE_WAYLAND_API=OFF
# or
cmake .. -DUSE_OPENGL_API=OFF -DUSE_WAYLAND_API=OFF
```

### Wayland (Linux)

Depends on gcc and wayland-client and wayland-cursor. Built using the wayland-gcc variants.

**Wayland Protocol Compatibility**:

Different Linux distributions and versions may ship with different versions of Wayland and its protocols. MiniFB includes pre-generated protocol headers and code that work with most common setups. However, if you encounter compatibility issues or want to ensure optimal compatibility with your specific Wayland version, you can regenerate the protocol files using your system's Wayland version.

To regenerate Wayland protocol files for your system:

```sh
# First, ensure you have the required packages installed:
# - Ubuntu/Debian: sudo apt install libwayland-dev wayland-protocols
# - Fedora: sudo dnf install wayland-devel wayland-protocols-devel
# - Arch: sudo pacman -S wayland wayland-protocols
# - openSUSE: sudo zypper install wayland-devel wayland-protocols-devel

# Then run the protocol generation script:
./scripts/generate-protocols.sh
```

This script will generate protocol headers and code that are specifically compatible with your installed Wayland version, potentially resolving any version mismatch issues.

If you use **CMake** just enable the flag:

```sh
mkdir build
cd build
cmake .. -DUSE_WAYLAND_API=ON
```

### Web (WASM)

Download and install [Emscripten](https://emscripten.org/). When configuring your CMake build, specify the Emscripten toolchain file. Then proceed to build as usual.

#### Building and running the examples (WASM)

```sh
cmake -DCMAKE_TOOLCHAIN_FILE=/path/to/emsdk/<version>/emscripten/cmake/Modules/Platform/Emscripten.cmake -S . -B build
cmake --build build
```

> **Note**: On Windows, you will need a build tool other than Visual Studio. [Ninja](https://ninja-build.org/) is the best and easiest option. Simply download it, put the `ninja.exe` executable somewhere, and make it available on the command line via your `PATH` environment variable. Then invoke the first command above with the addition of `-G Ninja` at the end.

Then open the file `build/index.html` in your browser to view the example index.

The examples are build using the Emscripten flag `-sSINGLE_FILE`, which will coalesce the `.js` and `.wasm` files into a single `.js` file. If you build your own apps without the `-sSINGLE_FILE` flag, you can not simply open the `.html` file in the browser from disk. Instead, you need an HTTP server to serve the build output. The simplest solution for that is Python's `http.server` module:

```sh
python3 -m http.server build/
```

You can then open the index at [http://localhost:8000](http://localhost:8000) in your browser.

#### Integrating a MiniFB app in a website

To build an executable target for the web, you need to add a linker option specifying its module name, e.g.:

```cmake
target_link_options(my_app PRIVATE "-sEXPORT_NAME=my_app")
```

The Emscripten toolchain will then build a `my_app.wasm` and `my_app.js` file containing your app's WASM code and JavaScript glue code to load the WASM file and run it. To load and run your app, you need to:

1. Create a `<canvas>` element with an `id` attribute matching the `title` you specify when calling `mfb_open_window()` or `mfb_open_window_ex()`.
2. Call the `<my_module_name>()` in JavaScript.

Example app:

```c
int main() {
    struct mfb_window *window = mfb_open_ex("my_app", 320, 240);
    if (!window)
        return 0;
    uint32_t *buffer = (uint32_t *) malloc(g_width * g_height * 4);
    mfb_update_state state;
    do {
        state = mfb_update_ex(window, buffer, 320, 200);
        if (state != STATE_OK) {
            break;
        }
    } while(mfb_wait_sync(window));
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
    <canvas id="Noise Test" style="background: #000;"></canvas>
</div>
<script>
    // Call the app's main() function
    noise();
</script>
</body>
</html>
```

#### Limitations & caveats

The web backend currently does not support the following MiniFB features:

- The flags to `mfb_open_ex()` are ignored
- `mfb_set_viewport()` (no-op)
- `mfb_set_viewport_best_fit()` (no-op)
- `mfb_get_monitor_dpi()` (reports a fixed value)
- `mfb_get_monitor_scale()` (reports a fixed value)
- `mfb_set_target_fps()` (no-op)
- `mfb_get_target_fps()` (no-op)

Everything else is supported.

When calling `mfb_open()` or `mfb_open_ex()`, the specified title must match the `id` attribute of a `<canvas>` element in the DOM. The functions will modify the `width` and `height` attribute of the `<canvas>` element. If not already set, then the functions will also modify the CSS style `width` and `height` attributes of the canvas.

Setting the CSS width and height of the canvas allows you to up-scale the framebuffer arbitrarily:

```js
// Request a 320x240 window
mfb_open("my_app", 320, 240);

// Up-scale 2x via CSS
<canvas id="my_app" style="width: 640px; height: 480px">
````

If not already set, the backend will also set a handfull of CSS styles on the canvas that are good defaults for pixel graphics.

- `image-rendering: pixelated`
- `user-select: none`
- `border: none`
- `outline-style: none`;

## How to add it to your project

First add this **repository as a submodule** in your dependencies folder. Something like `dependencies/`:

```sh
git submodule add https://github.com/emoon/minifb.git dependencies/minifb
```

Then in your `CMakeLists.txt` file, include the following:

```cmake
add_subdirectory(dependencies/minifb)

# Link MiniFB to your project:
target_link_libraries(${PROJECT_NAME}
    minifb
)
```

Fill out the rest of your `CMakeLists.txt` file with your source files and dependencies.

### DOS (DJGPP)

Use the `tests/dos/tools/download-dos-tools.sh` file to download all the tools needed to compile, run and debug MiniFB DOS applications. The Bash script will download the following tools:

- [DJGPP](https://www.delorie.com/djgpp/), a GCC fork targeting 32-bit protected mode DOS.
- [GDB 7.1a](https://github.com/badlogic/gdb-7.1a), a GDB fork that can remotely debug 32-bit COFF executables via TCP, running in e.g. DOSBox-x, VirtualBox, or a real machine.
- [DOSBox-x](https://github.com/badlogic/dosbox-x/), a fork of the popular DOS emulator with some modifications to enable remote debugging via GDB.

The tools are downloaded to the `tests/dos/tools/` folder. The folder also contains a DOSBox-x configuration file `dosbox-x.conf` preconfigured for debugging. The `toolchain-djgpp.cmake` file is a CMake toolchain file for DJGPP.

You can optionally run the script with the argument `--with-vs-code`. If you have [Visual Studio Code](https://code.visualstudio.com/) installed, the script will install extensions needed for C/C++ development and debugging, and create a `.vscode` folder in the repository root containing launch configurations, tasks, and various other settings for DOS development in VS Code.

#### Building and running the examples (DOS)

```sh
cmake -DCMAKE_TOOLCHAIN_FILE=./tests/dos/tools/toolchain-djgpp.cmake -S . -B build
cmake --build build
```

> **Note**: On Windows, you will need a build tool other than Visual Studio. [Ninja](https://ninja-build.org/) is the best and easiest option. Simply download it, put the `ninja.exe` executable somewhere, and make it available on the command line via your `PATH` environment variable. Then invoke the first command above with the addition of `-G Ninja` at the end.

This will generate DOS 32-bit `.exe` files in the `build/` folder which you can run with DOSBox-x like this:

```sh
./tests/dos/tools/dosbox-x/dosbox-x -fastlaunch -exit -conf ./tests/dos/tools/dosbox-x.conf build/<executable-file>
```

Note that the DOS backend can not support multi-window applications. The examples `multiple-windows.c` and `hidpi.c` will thus not run correctly.

#### Compiling your own MiniFB app for DOS

Copy the folder `tests/dos/` from the MiniFB repository to your project and run the `dos/tools/download-dos-tools.sh` file as described above. Pull in MiniFB via CMake as described above.

Then, when configuring your CMake build, specify the DJGPP toolchain file:

```sh
cmake -DCMAKE_TOOLCHAIN_FILE=./dos/tools/toolchain-djgpp.cmake ... rest of your configure parameters ...
```

The build will then generate DOS 32-bit protected mode executables and use the MiniFB DOS backend. You can run the executables as is in DOSBox-x or FreeDOS, or a Windows version that can run DOS applications.

Running the executbales in vanilla MS-DOS requires a DPMI server. Download [CWSDPMI](https://www.ibiblio.org/pub/micro/pc-stuff/freedos/files/distributions/1.2/repos/pkg-html/cwsdpmi.html), extract the ZIP file, and place the `CWSDPMI.EXE` file found in the `BIN/` folder next to your application's executable.

#### Debugging your MiniFB app in DOSBox-x

The MiniFB DOS backend comes with a [GDB stub](https://sourceware.org/gdb/onlinedocs/gdb/Remote-Stub.html) in [`tests/dos/gdbstub.h`](tests/dos/gdbstub.h) that you can incorporate into your application to enable remote debugging your app through GDB. Run the `dos/tools/download-dos-tools.sh` script as described above to get GDB and DOSBox-x versions capable of remote debugging. Then, in the source file that contains your `main()` function, include the `gdbstub.h` file and call the `gdb_start()` and `gdb_checkpoint()` functions like this:

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
./dos/tools/dosbox-x/dosbox-x -fastlaunch -exit -conf ./dos/tools/dosbox-x.conf path/to/your/executable.exe
```

DOSBox-x will start up, your application will wait for GDB to connect (`gdb_start()`).

Run GDB, load the debugging information from the executable and connect to your app running and waiting in DOSBox-x:

```sh
./dos/tools/gdb/gdb
(gdb) file path/to/your/executable.exe
(gdb) target remote localhost:5123
```

GDB will show your app being halted on the `gdb_start()` line. You can now set breakpoints, step, continue, inspect local variables and so on.

If your app is executing and you press `CTRL+C` to interrupt it, you will end up inside `gdb_checkpoint()`. You can then set breakpoints, or step out to inspect your program state.

Alternatively, you can use VS Code to debug via a graphical user interface. Run the `download-dos-tools.sh` script with the `--with-vs-code` flag. This will install C/C++/CMake VS Code extensions and copy the `dos/.vscode` to the project root folder. Open the project root folder in VS Code, select the `djgpp` [CMake kit](https://vector-of-bool.github.io/docs/vscode-cmake-tools/kits.html), select the `Debug` [CMake variant](https://vector-of-bool.github.io/docs/vscode-cmake-tools/getting_started.html#selecting-a-variant), and the [CMake launch target](https://vector-of-bool.github.io/docs/vscode-cmake-tools/debugging.html#selecting-a-launch-target), then run the `DOS debug target` launch configuration.

You can use both the CLI and GUI method for debugging the MiniFB examples as well. See the example [tests/dos/dos.c](tests/dos/dos.c) for usage of the GDB stub.

#### Limitations and caveats

The DOS backend currently does not support the following MiniFB features:

- The flags to `mfb_open_ex()` are ignored
- `mfb_set_viewport()` (no-op)
- `mfb_set_viewport_best_fit()` (no-op)
- `mfb_get_monitor_dpi()` (reports a fixed value)
- `mfb_get_monitor_scale()` (reports a fixed value)
- `mfb_set_target_fps()` (no-op)
- `mfb_get_target_fps()` (no-op)
- Multiple windows are not support
- A window is always full-screen
- The window dimensions are limited to supported VESA modes, e.g. 320x240, 640x480, 800x600, etc. VESA mode support may vary across environments and hardware. The 3 listed here are very well supported. The VESA code will try to get the closest match to the requested window dimensions, and also check if 32-bit color encodings are possible. On many machines, only 24-bit color encodings are possible. The DOS backend will transparently convert the 32-bit buffers provided to `mfb_update_ex()` to 24-bit internally.
- Keyboard handling is limited to the keys found [here](src/dos/DOSMiniFB.c#L24). No other keys will be reported.
- Character input is limited to ASCII based on a US keyboard layout.
