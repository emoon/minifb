# Testing the DOS backend

## Why this exists

Running the interactive tests on DOS means running them under DOSBox-x, and two things get in
the way before any test result is visible. The executable can refuse to load for a reason that
has nothing to do with MiniFB, and part of the input the tests ask for does not exist on DOS.
This page covers both.

## Short filenames

DOS filenames are limited to an eight-character name and a three-character extension, so the
DJGPP build renames the executables:

| Target | DOS executable |
| --- | --- |
| `keyboard_events_test` | `keyevent.exe` |
| `mouse_events_test` | `mouseevt.exe` |
| `input_events` | `inpevent.exe` |
| `input_events_cpp` | `inpevcpp.exe` |
| `multiple_windows` | `multiwin.exe` |
| `fullscreen` | `fullscrn.exe` |
| `hide_cursor` | `hidecurs.exe` |
| `debug_dos` | `debugdos.exe` |
| `minifb_version_info` | `verinfo.exe` |

Names that already fit, such as `noise` and `timer`, keep them. The build sets this with
`minifb_set_dos_name` in `cmake/minifb_cmake_helpers.cmake`, and it only takes effect for
DJGPP. The CMake target names never change, so the build commands are the same on every
platform.

## Loading the executable

A DJGPP program starts as a small real mode stub that reopens its own file to load the rest of
itself. It gets the path to that file from DOS. DOSBox-x hands it a plain truncation of the
path instead of the 8.3 alias the file really has, so the stub looks for a file that does not
exist and the program dies before `main`:

```bash
C:\BUILD-DO\TESTS\KEYBOARD.EXE: can't open
```

The real alias in that example was `C:\BUILD-~1\TESTS\KEYBOA~2.EXE`. Every long component of
the path is truncated the same wrong way, so a short executable name alone is not enough: a
build directory such as `build-dos-djgpp` breaks the path on its own. Dropping `lfn=true` does
not help either, because then the shell cannot resolve the long path at all and answers `Bad
command or filename`.

Two ways around it. Pass the executable to DOSBox-x and let it mount the directory that holds
it:

```sh
./tools/dos/dosbox-x/dosbox-x -fastlaunch -exit -conf ./tools/dos/dosbox-x.conf \
    build-dos/tests/keyevent.exe
```

Or mount that directory yourself, so that no long name is left in the path:

```sh
./tools/dos/dosbox-x/dosbox-x -fastlaunch -conf ./tools/dos/dosbox-x.conf \
    -c "mount c /path/to/build-dos/tests" -c "c:" -c "keyevent.exe > key_dos.txt" -c "exit"
```

## Reading the result

The tests print the steps, the progress and the errors on stderr, and the event log and the
final summary on stdout, so `keyevent.exe > key_dos.txt` keeps the instructions on screen and
the comparable part in the file. The file is written in the mounted directory, which is a
normal directory on the host, so it can be diffed against a run on another backend without
copying anything out of the emulator.

DOS writes that file when the program exits. Escape skips the step you are on, and once none
are left it ends the run, in both tests.
Closing the DOSBox-x window or killing it from outside leaves the file empty, however many
steps were completed.

## What DOS cannot report

There is no window focus on DOS, so the focus step of the keyboard test does not exist, and
neither do the steps that need a compose key, a character picker or an input method. The
keyboard test compiles those out and runs 24 steps instead of 30. The mouse test loses the
steps that need a window: there is no boundary to cross or to drag across, so it runs five
steps instead of eight, and the wheel one is optional because a driver without the CuteMouse
API has no wheel to report.

Everything else the test asks for reaches the backend. It installs its own interrupt 9 handler
and reads the raw bytes from port 0x60, so `E0` prefixed keys are told apart from the base
ones: right Ctrl and Alt, the navigation block, keypad Enter and keypad divide all report
their own key. Num Lock is tracked by the backend, because the BIOS translation is not in the
path: with it off the keypad reports the navigation keys, which is what the hardware means.

## What the emulator does to the keyboard

DOSBox-x does not hand the guest the scancodes the keyboard sent. It synthesises them from the
host key, and `usescancodes` in `tools/dos/dosbox-x.conf` picks how. Neither setting gets
every key right, and the two fail on different ones:

| Key | `usescancodes = true` | `usescancodes = false` |
| --- | --- | --- |
| ISO key between Left Shift and Z | reports Right arrow | correct |
| keypad 0 | reports Up | correct |
| keypad decimal | reports Page Up | correct |
| keypad 3 | reports Home | correct |
| key left of the tall Enter | correct | nothing arrives |
| key left of Right Shift | correct | reports Minus |
| Right Alt | correct | nothing arrives |

`false` is what the conf ships, and the reason is the kind of failure rather than the count.
With `true` the ISO key sends the right arrow, so the arrow step of the keyboard test passes
after three arrows instead of four: a green step that was never exercised. With `false` the
keys that fail send nothing usable, the step waits, and it gets skipped. A declared hole is
worth more than a false pass.

Pause is worse than either row above. With `false` DOSBox-x drops the `E1` prefix and sends a
plain Control followed by Num Lock, so pressing Pause reports two keys nobody pressed and
toggles Num Lock inside the guest. The backend decodes the real `E1 1D 45 E1 9D C5` that
hardware sends, and never sees it here.

The `false` column is a US keyboard talking. DOSBox-x maps by symbol in that mode, and the log
it writes at startup says which layout it thinks it has:

```
LOG: Current X11 keyboard layout (token) is: 'us'
LOG: Host keyboard layout is now us (US English)
```

Under WSL that line said `us` on a Spanish keyboard, which is why the key that carries the
minus sign lands on the US minus position and the one that carries a c-cedilla, absent from a
US layout, lands nowhere.

Telling the X server the truth is not the lever, though. After `setxkbmap es` the same startup
detects the layout and then goes on using the other one:

```
LOG: Current X11 keyboard layout (token) is: 'es'
LOG: Host keyboard layout is now us (US English)
```

The three keys stay lost.

## Reading raw scancodes

`dos_scancodes.c` prints every byte the keyboard controller delivers, with its `E0` or `E1`
prefix, so what a key sends can be told from what the backend makes of it:

```
   52   code 0x52   press
E0 48   code 0x48   press
```

It stays out of the CMake build on purpose: it is a probe, not a test. Compile it by hand,
with a name that already fits 8.3. It has to be `gnu99` rather than `c99`, because DJGPP hides
`kbhit` and `__dpmi_yield` in strict ANSI mode:

```sh
tools/dos/djgpp/bin/i586-pc-msdosdjgpp-gcc -std=gnu99 -O2 tests/dos_scancodes.c -o scancode.exe
```

It takes interrupt 9 over, so while it runs the BIOS sees nothing: the lock LEDs do not follow
and the flags at `0040:0017` stay where they were. That does not affect the measurement, since
the bytes are synthesised before the BIOS would see them anyway. Escape ends the run.

The rows of the table above where a key sends something wrong were measured with it. The rows
that say `correct`, and the Pause sequence, came from a full run of the keyboard test.

## What the emulator does to the mouse

DOSBox-X keeps the middle button for itself. `middle_unlock` lets it release a locked mouse,
and with the shipped `autolock = false` the "manual" setting means every middle click is
consumed before the guest sees it, so `INT 33h` function 0x03 never sets bit 2 and the mouse
test cannot get past its button step. `tools/dos/dosbox-x.conf` sets `none`, which costs
nothing here: with autolock off there is no lock to release. `Ctrl+F10` still works.

The wheel is a separate question, answered by the section below.

## The mouse wheel

A DOS mouse driver has no wheel of its own. DOSBox-x can convert the host wheel into extended
Up and Down key presses with `mouse_wheel_key`, and the value that turns that off for the
integrated DOS is `0`, not `-1`: with `-1` the wheel still arrives as arrow keys, and it
poisons any step that asks for them. `tools/dos/dosbox-x.conf` sets `0`, which also leaves the
CuteMouse wheel API in place, and the backend reads the wheel from the driver through
interrupt 33h instead.

Where the driver puts that counter is not the same everywhere. CuteMouse documents BX of
function 0x0b; DOSBox-X answers zero there and fills BH of function 0x03 instead, which is
what `mouseraw.exe` measured. The backend reads both, and still asks for function 0x0b even
when BH already answered, because on the drivers that keep a counter that read is what clears
it.

`MINIFB_DOS_WHEEL_FROM_ARROW_KEYS` is the other route, for a driver with no wheel API at all.
It reads the extended Up and Down keys as scroll, which costs the arrow keys for the rest of
the run, so it has to be asked for at build time.

`dos_mouse_raw.c` asks the driver instead of the backend. It prints what function 0x11
answers, and then one line whenever a button changes or either
place that can carry the wheel is not zero: BX of function 0x0b, and BH of function 0x03,
because drivers differ in which of the two they use. Every register is printed raw, so a
driver that reports nothing can be told apart from a backend reading the wrong one. The
middle button is bit 2 of BX in function 0x03, and it shows there whether or not the wheel
works. Escape ends the run and prints which buttons reached DOS and which register carried
the wheel, so the answer does not have to be read out of the column dump. It is built by hand
like the scancode probe:

```sh
tools/dos/djgpp/bin/i586-pc-msdosdjgpp-gcc -std=gnu99 -O2 tests/dos_mouse_raw.c -o mouseraw.exe
```

## Quieting the log

DOSBox-x logs an `INT10:Unhandled mode 9 for scroll` error for every line the tests scroll,
which buries the output. `int10 = never` in the `[log]` section silences it without hiding
anything else.
