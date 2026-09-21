# Interactive tests

These tests use real platform input. A person performs the actions, while the test checks the MiniFB callback and getter contract.

Configure and build them with:

```sh
cmake -S . -B build -DMINIFB_BUILD_TESTS=ON
cmake --build build --target mouse_events_test keyboard_events_test
```

On DOS the executables are named `mouseevt.exe` and `keyevent.exe`, because DOS filenames are limited to 8 characters. `docs/testing-dos.md` explains how to run them in DOSBox-x, how to read the output, and which steps that backend cannot exercise.

## Mouse events

On Android the mouse test is an app, not an executable. Open `tests/android` in Android Studio, or build it from there with Gradle, and read the output with `adb logcat -s mouse_events_test`. The project is the `examples/android/native2026` one with the sources swapped, so it needs the same NDK and SDK.

Run `mouse_events_test` from the selected build directory. It asks for one action at a time: do what the `NEXT` line says, or press Escape to skip a step you cannot perform. There are eight steps on the desktop backends, five on DOS and two on the touch ones. The optional ones are the horizontal scroll and the side buttons, which plenty of mice do not have, and on DOS the wheel, which needs a driver with the CuteMouse API.

Steps, progress and errors go to stderr; the event log and the final summary go to stdout. Running it as `mouse_events_test > log.txt` therefore leaves a clean panel on screen and keeps the comparable part in the file.

The run ends with a `CONTRACT SUMMARY` block: one line per required step, then one line per measured contract point, in a fixed order and wording, so two runs on two backends can be compared with `diff`. A run with no errors that skipped a required step ends as `INCOMPLETE` rather than `PASS`.

That comparison is the point. Run the same steps with the same device on each backend of the machine and diff the summaries: the button numbers, the scroll signs, the callbacks per notch and the drag behaviour must match. Lines marked `NOT EXERCISED` mean the hardware could not produce that step, and two runs that both skip a line agree about nothing.

Scroll signs belong to the device and to the system scroll settings, not only to the backend: a trackpad and a tilt wheel report opposite signs for what feels like the same direction. The test records them instead of judging them, and only checks that two opposite gestures report opposite signs.

The Web build creates `tests/mouse_events_test.html`. Open the browser developer console before performing the steps.

## Keyboard events

`keyboard_events_test` walks through 31 steps, one at a time, and 24 on DOS. The first five are required and the rest are optional. One of the optional steps asks for the ISO key next to left Shift and records which token the backend reports for it. Windows, X11 and Wayland all name it `MFB_KB_KEY_WORLD_2`, because all three name a key by where it is. An X server that does not number keys the evdev way reports `MFB_KB_KEY_WORLD_1` instead, and `MINIFB_X11_DISABLE_EVDEV_KEYCODES` makes any server behave that way so the difference can be seen without one. The rules it checks are written in `docs/api/api-contract-notes.md`, in the Keyboard subsection of the input model. The required part covers Windows, macOS, X11, Wayland, Web, and DOS. Android and iOS are excluded because MiniFB does not expose general keyboard input on those backends. The focus step does not exist on DOS, which has no window focus.

Use a keyboard layout in which A and R produce lowercase `a` and `r`, release all keys, and turn Caps Lock off before starting. Then do what the `NEXT` line says. It is reprinted after every completed step, so the instructions are always the last thing on screen. Switch applications with the mouse during the focus step, so that extra shortcut keys do not affect the result.

One optional step asks for ten presses of AltGr. Windows presses a Control of its own for that key and reports it, so AltGr is two keys there and one key on every other backend. The step records how many of the ten presses delivered that Control, which is all of them on Windows and none anywhere else, and fails only if one of them is left pressed after the Alt comes up.

One optional step asks you to tap Q quickly, and the test drops to a few frames per second while that step is pending: at the normal rate an event pump is shorter than a human keypress, so a tap could never land whole inside one.

Press Escape to skip the step you are on when you cannot perform it, for example when the keyboard has no right Super key or the system has no character picker. This is what tells an untried step apart from one that was tried and produced nothing: a skipped step reports `SKIPPED`, a step never reached reports `not reached`. A run with no errors that skipped a required step ends as `INCOMPLETE` rather than `PASS`, because the contract was not exercised. Skipping optional steps never changes the result.

Four rules are checked on every callback, without a step of their own: text input never carries a control code point, keys with no `mfb_key` token are never reported, the key callback comes before the character callback that it produced, and keys such as Enter, Tab and Escape deliver no character at all.

Steps, progress and errors go to stderr; the event log and the final summary go to stdout. Running it as `keyboard_events_test > log.txt` therefore leaves a clean panel on screen and keeps the comparable part in the file. A held key produces one collapsed line with the number of repeats instead of hundreds of lines.

`tests/drivers/` holds programs that replay the steps without a person at the keyboard, one per platform. They are not part of the build for now: their README explains what they do and what injected input cannot reproduce.

The Web build creates `tests/keyboard_events_test.html`. Open the browser developer console, click the canvas if it does not already have focus, and then perform the printed steps.
