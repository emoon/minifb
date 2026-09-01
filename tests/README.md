# Interactive tests

These tests use real platform input. A person performs the actions, while the
test checks the MiniFB callback and getter contract.

Configure and build them with:

```sh
cmake -S . -B build -DMINIFB_BUILD_TESTS=ON
cmake --build build --target mouse_events_test
```

On Android the test is an app, not an executable. Open `tests/android` in Android
Studio, or build it from there with Gradle, and read the output with
`adb logcat -s mouse_events_test`. The project is the `examples/android/native2026`
one with the sources swapped, so it needs the same NDK and SDK.

Run `mouse_events_test` from the selected build directory. It prints the manual
steps once at startup, then one progress line for each completed step. Closing
the window prints a final `PASS` or `FAIL` result. Closing it before completing
a required step is an error.

Follow the printed order exactly. The test then ends with a `CONTRACT SUMMARY`
block: one line per contract point, in a fixed order and wording, so two runs on
two backends can be compared with `diff`.

That comparison is the point. Run the same steps with the same device on each
backend of the machine and diff the summaries: the button numbers, the scroll
signs, the callbacks per notch and the drag behaviour must match. Lines marked
`NOT EXERCISED` mean the hardware could not produce that step, and two runs that
both skip a line agree about nothing.

Scroll signs belong to the device and to the system scroll settings, not only to
the backend: a trackpad and a tilt wheel report opposite signs for what feels
like the same direction. The test records them instead of judging them, and only
checks that two opposite gestures report opposite signs.

The Web build creates `tests/mouse_events_test.html`. Open the browser developer
console before performing the steps.
