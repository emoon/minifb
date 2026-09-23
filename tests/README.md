# Interactive tests

These tests use real platform input. A person performs the actions, and the test checks that the MiniFB callbacks and getters follow the contract.

Configure and build them with:

```sh
cmake -S . -B build -DMINIFB_BUILD_TESTS=ON
cmake --build build --target mouse_events_test keyboard_events_test window_events_test
```

On DOS the mouse and keyboard tests are named `mouseevt.exe` and `keyevent.exe`, because DOS file names are limited to 8 characters. The window test is not built for DOS, because DOS has no window to focus, resize or close. `docs/testing-dos.md` explains how to run the tests in DOSBox-x, how to read the output, and which steps DOS cannot perform.

## Running a test

Run the test from the build directory. It asks for one action at a time: do what the `NEXT` line says. The `NEXT` line is printed again after every completed step, so the current instruction is always the last line on screen.

Press Escape to skip a step you cannot perform, for example when the mouse has no side buttons. A skipped step reports `SKIPPED`, and a step the run never reached reports `not reached`. This tells an untried step apart from one that was tried and produced nothing.

Steps, progress and errors go to stderr. The event log and the final summary go to stdout. If you run a test as `mouse_events_test > log.txt`, the instructions stay on screen and the file gets the part you want to compare.

The run ends with a `CONTRACT SUMMARY` block. It has one line per required step, then one line per contract point, always in the same order and with the same wording. This lets you compare two runs on two backends with `diff`. Run the same steps with the same device on each backend of the machine, and compare the summaries.

The result is one of three:

- `PASS`: every required step was done and no error was found.
- `FAIL`: at least one error was found. The errors are listed again at the end.
- `INCOMPLETE`: no error was found, but a required step was skipped, so part of the contract was not tested. Skipping optional steps never changes the result.

The Web build creates an HTML page for each test, for example `tests/mouse_events_test.html`, in the build directory. Open it, open the browser developer console, and follow the steps printed there.

## Mouse events

`mouse_events_test` has eight steps on the desktop backends, five on DOS and two on Android and iOS. The optional steps are the horizontal scroll and the side buttons, because many mice have neither. On DOS the wheel is optional too, because it needs a mouse driver with the CuteMouse API.

The summary must match between backends: the button numbers, the scroll signs, the number of callbacks per wheel notch, and what happens when you drag outside the window. A line marked `NOT EXERCISED` means that the hardware could not perform that step. Two runs that both have that line tell you nothing about it.

Scroll signs depend on the device and on the system scroll settings, not only on the backend. A trackpad and a tilt wheel can report opposite signs for what feels like the same movement. The test records the signs without checking them. It only checks that two opposite movements report opposite signs. Compare the signs only between runs on the same machine, with the same device and settings.

On Android the mouse test is an app, not an executable. Open `tests/android` in Android Studio, or build it there with Gradle, and read the output with `adb logcat -s mouse_events_test`. The project is a copy of `examples/android/native2026` with different sources, so it needs the same NDK and SDK.

## Keyboard events

`keyboard_events_test` has 32 steps, and 25 on DOS. The first five are required and the rest are optional. The required steps cover Windows, macOS, X11, Wayland, Web and DOS. Android and iOS are not covered, because MiniFB has no general keyboard input on those backends. DOS has no focus step, because it has no window focus.

Before you start, use a keyboard layout in which A and R produce lowercase `a` and `r`, release all keys, and turn Caps Lock off. During the focus step, switch applications with the mouse, so that no extra keys reach the test.

Four rules are checked on every callback, without a step of their own:

- Text input never contains a control code point.
- A key with no `mfb_key` token is never reported.
- The key callback comes before the character callback that the key produced.
- Keys such as Enter, Tab and Escape produce no character.

A held key produces one line in the log with the number of repeats, not hundreds of lines.

Some steps need more explanation:

- **ISO key and the key below Escape.** One step asks for the ISO key next to left Shift. Windows, X11, Wayland and macOS all report it as `MFB_KB_KEY_WORLD_2`, because all four name a key by its position. An X server that does not number keys the evdev way reports `MFB_KB_KEY_WORLD_1` instead. `MINIFB_X11_DISABLE_EVDEV_KEYCODES` makes any X server behave that way, so you can see the difference without such a server. The next step asks for the key below Escape, which must be `MFB_KB_KEY_GRAVE_ACCENT`. Together, the two steps catch a backend that swaps the two keys, as macOS does on an ISO keyboard unless the backend corrects it.
- **Locks.** The lock step asks for Num Lock only where that lock exists. macOS has no Num Lock, so there the step asks only for Caps Lock. The keypad Clear key still reports `MFB_KB_KEY_NUM_LOCK` on macOS, because the key exists even though the lock does not.
- **AltGr.** One step asks you to press AltGr ten times. Windows presses a Control key of its own for AltGr and reports it, so AltGr is two keys on Windows and one key on every other backend. The step records how many of the ten presses also delivered that Control: all of them on Windows, none anywhere else. It fails only if that Control is still pressed after the Alt key is released.
- **Quick tap.** One step asks you to tap Q quickly. While that step is waiting, the test runs at a few frames per second. At the normal rate one event pump is shorter than a keypress, so a quick tap could never start and end inside one pump.
- **Emoji on macOS.** Ctrl+Command+Space does not open the emoji picker here. That shortcut belongs to the "Emoji & Symbols" item that AppKit adds to the Edit menu, and MiniFB creates no menu bar. Open the Character Viewer from the Input menu instead. To show that menu, go to the keyboard settings, open Input Sources and turn on "Show Input menu in menu bar". Then choose "Show Emoji & Symbols" from its icon. The viewer opens as a floating panel, so the test window keeps the focus, and a double click inserts the character. Pick one of the coloured circles or squares from `U+1F7E0` to `U+1F7EB`. They are above `U+FFFF`, which is the range that a 16-bit mask would lose. The red and blue circles are not in that block.

On Web, click the canvas if it does not have the focus before you start.

## Window events

`window_events_test` checks the active, resize and close callbacks. On the desktop backends it has seven steps:

1. Switch to another application and back.
2. Resize the window.
3. Move the window.
4. Click the close button. The test refuses the request, so the window stays open.
5. Minimize the window and restore it (optional).
6. Maximize the window and restore it (optional).
7. Click the close button again. The test accepts the request and the run ends.

The test cannot see a move, a minimize or a maximize, so those steps end when you press Space. Closing the window during any other step ends the run, so you can always stop.

The test checks these rules:

- `mfb_is_window_active` agrees with the active callback, and the callback always reports a change.
- The resize callback reports a new size, never the current one. Moving the window must not produce a resize, and neither must restoring it at the size it already had.
- The window size getters agree with the resize callback, and the drawable area fits in the window.
- A refused close request keeps the window open. An accepted one closes it, at the latest on the next call to `mfb_update` or `mfb_wait_sync`.
- `mfb_close` closes the window without calling the close callback.

Accepting a close request and calling `mfb_close` both end the run, so one window cannot test both. For that reason the test checks `mfb_close` on a small second window before the steps start. You will see that window for a moment.

The summary also records some things without checking them: the window size and monitor scale at open, whether the window starts active, which callbacks arrive in the first event pump, and what minimizing and maximizing produce. Not every platform reports a minimize, so compare those lines between backends.

On Web there are two steps: click outside the canvas and back, and resize the browser window. A page has no close button and cannot be moved, minimized or maximized. Press Escape after the last step. The run then ends with `mfb_close`, which is how Web checks it.
