#include <MiniFB.h>

#include <stdbool.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__APPLE__)
    #include <TargetConditionals.h>
#endif

#define TEST_WIDTH       640u
#define TEST_HEIGHT      480u
#define TEST_KEY_COUNT   ((unsigned) MFB_KB_KEY_LAST + 1u)

#define ERROR_KEY_BUFFER_NULL       (UINT64_C(1) << 0)
#define ERROR_KEY_RANGE             (UINT64_C(1) << 1)
#define ERROR_CALLBACK_KEY_STATE    (UINT64_C(1) << 2)
#define ERROR_KEY_SNAPSHOT          (UINT64_C(1) << 3)
#define ERROR_MODIFIER_STATE        (UINT64_C(1) << 4)
#define ERROR_ACTIVE_GETTER         (UINT64_C(1) << 5)
#define ERROR_FOCUS_STATE           (UINT64_C(1) << 6)
#define ERROR_LOCK_MODIFIER_STATE   (UINT64_C(1) << 7)
#define ERROR_INVALID_TEXT_CODEPOINT (UINT64_C(1) << 8)
#define ERROR_UPDATE                (UINT64_C(1) << 9)
#define ERROR_CONTROL_CODEPOINT     (UINT64_C(1) << 10)
#define ERROR_NON_TEXT_KEY_CHAR     (UINT64_C(1) << 11)
#define ERROR_UNKNOWN_KEY_REPORTED  (UINT64_C(1) << 12)
#define ERROR_CALLBACK_ORDER        (UINT64_C(1) << 13)
#define ERROR_COMPOSITION_AFTER_FOCUS (UINT64_C(1) << 14)
#define ERROR_ALTGR_COMPANION       (UINT64_C(1) << 15)
#define ERROR_GRAVE_KEY_TOKEN       (UINT64_C(1) << 16)

#if defined(__DJGPP__)
    #define TEST_DOS_PLATFORM 1
#else
    #define TEST_DOS_PLATFORM 0
#endif

// Browsers under Windows never name the right Shift, so the step cannot be completed there,
// while the same browsers under Linux report it fine. One Web build serves both systems and
// cannot tell them apart, so the step gives up being required instead of failing every
// Windows run.
#if defined(__EMSCRIPTEN__)
    #define TEST_OVERLAPPING_SHIFT_REQUIRED false
#else
    #define TEST_OVERLAPPING_SHIFT_REQUIRED true
#endif

// A browser only exposes the lock state through getModifierState, and under Linux that value
// lags. A press that turns a lock back off reports no change, and the stale bit does not
// settle until some later key, which then looks like a key changing a lock it never touched.
// Both are the same lag wearing two faces, neither can be told apart from a real fault, and
// there is nothing else to ask, so on the web they are recorded instead of counted as
// contract failures.
#if defined(__EMSCRIPTEN__)
    #define TEST_LOCK_MODS_VERIFIABLE false
#else
    #define TEST_LOCK_MODS_VERIFIABLE true
#endif

// macOS has no Num Lock. The keypad Clear key keeps the MFB_KB_KEY_NUM_LOCK token, as it does
// in GLFW and SDL, but no lock bit ever follows it, so the half of the lock step that asks for
// one cannot be performed and its absence is not a fault to report.
#if defined(__APPLE__)
    #define TEST_NUM_LOCK_EXISTS  false
    #define TEST_LOCK_KEYS_ACTION \
        "Toggle Caps Lock twice, restoring it to its starting state. This system has no Num Lock."
    #define TEST_LOCK_KEYS_DONE   "Caps Lock toggled and restored"
#else
    #define TEST_NUM_LOCK_EXISTS  true
    #define TEST_LOCK_KEYS_ACTION \
        "Toggle Caps Lock twice, then Num Lock twice, restoring both to their starting state."
    #define TEST_LOCK_KEYS_DONE   "Caps Lock and Num Lock toggled and restored"
#endif

// Windows composes dead keys in the layout, and that state is the thread's rather than the
// window's, so leaving and coming back does not clear it. Every Win32 application behaves the
// same way, so the accent surviving is what this platform does, not a fault to report.
#if defined(_WIN32)
    #define TEST_COMPOSITION_SURVIVES_FOCUS true
#else
    #define TEST_COMPOSITION_SURVIVES_FOCUS false
#endif

// The order walks the keyboard instead of grouping by behaviour, so a run is a single sweep
// and not a jump from one block to another. The required steps come first, so a run given up
// half way still covers the contract, and the ones that depend on no particular key come last.
typedef enum {
    STEP_A,
    STEP_SPACE,
    STEP_REPEAT,
    STEP_SHIFT,
#if !TEST_DOS_PLATFORM
    STEP_FOCUS,
#endif

    STEP_FUNCTION_EARLY,
    STEP_FUNCTION_LATE,
    STEP_PRINT_SCREEN,
    STEP_SCROLL_LOCK,
    STEP_PAUSE,

    STEP_NO_TEXT,
    STEP_POSITIONAL,
    STEP_GRAVE_ACCENT,
    STEP_LEFT_MODIFIERS,
    STEP_LEFT_SUPER,
    STEP_RIGHT_MODIFIERS,
#if !TEST_DOS_PLATFORM
    STEP_ALTGR,
#endif
    STEP_RIGHT_SUPER,
    STEP_MENU,

    STEP_NAVIGATION,
    STEP_ARROWS,
    STEP_DUPLICATED_DIGIT,
    STEP_DUPLICATED_SLASH,
    STEP_KEYPAD,
    STEP_LOCK_KEYS,

    STEP_MOUSE_MODS,
    STEP_QUICK_TAP,
#if !TEST_DOS_PLATFORM
    STEP_DEAD_ACUTE,
    STEP_CANCELLED_ACUTE,
    STEP_DEAD_FOCUS,
    STEP_EMOJI,
    STEP_CJK,
#endif
    STEP_COUNT
} TestStepId;

// A step that asks for a set of keys lists them, so the run can say which ones have arrived
// while it waits and which ones never did when it is skipped. The list is for reporting: a
// step whose condition is not just "these keys were pressed" leaves it empty.
typedef struct {
    bool           required;
    const char    *label;
    const char    *action;
    const char    *done_text;
    const mfb_key *keys;
    unsigned       key_count;
} TestStep;

#define TEST_STEP_KEYS(name, ...)     static const mfb_key name[] = { __VA_ARGS__ }

TEST_STEP_KEYS(g_keys_left_mods,   MFB_KB_KEY_LEFT_CONTROL, MFB_KB_KEY_LEFT_ALT);
TEST_STEP_KEYS(g_keys_right_mods,  MFB_KB_KEY_RIGHT_CONTROL, MFB_KB_KEY_RIGHT_ALT);
TEST_STEP_KEYS(g_keys_navigation,  MFB_KB_KEY_HOME, MFB_KB_KEY_END,
                                   MFB_KB_KEY_PAGE_UP, MFB_KB_KEY_PAGE_DOWN,
                                   MFB_KB_KEY_INSERT, MFB_KB_KEY_DELETE);
TEST_STEP_KEYS(g_keys_arrows,      MFB_KB_KEY_UP, MFB_KB_KEY_DOWN,
                                   MFB_KB_KEY_LEFT, MFB_KB_KEY_RIGHT);
TEST_STEP_KEYS(g_keys_shift,       MFB_KB_KEY_LEFT_SHIFT, MFB_KB_KEY_RIGHT_SHIFT);
TEST_STEP_KEYS(g_keys_no_text,     MFB_KB_KEY_ENTER, MFB_KB_KEY_TAB, MFB_KB_KEY_BACKSPACE);
TEST_STEP_KEYS(g_keys_positional,  MFB_KB_KEY_WORLD_2, MFB_KB_KEY_BACKSLASH);
TEST_STEP_KEYS(g_keys_keypad_all,  MFB_KB_KEY_KP_0, MFB_KB_KEY_KP_1, MFB_KB_KEY_KP_2,
                                   MFB_KB_KEY_KP_3, MFB_KB_KEY_KP_4, MFB_KB_KEY_KP_5,
                                   MFB_KB_KEY_KP_6, MFB_KB_KEY_KP_7, MFB_KB_KEY_KP_8,
                                   MFB_KB_KEY_KP_9, MFB_KB_KEY_KP_DECIMAL,
                                   MFB_KB_KEY_KP_ADD, MFB_KB_KEY_KP_SUBTRACT,
                                   MFB_KB_KEY_KP_MULTIPLY, MFB_KB_KEY_KP_DIVIDE,
                                   MFB_KB_KEY_KP_ENTER);
TEST_STEP_KEYS(g_keys_dup_digit,   MFB_KB_KEY_1, MFB_KB_KEY_KP_1);
TEST_STEP_KEYS(g_keys_dup_slash,   MFB_KB_KEY_SLASH, MFB_KB_KEY_KP_DIVIDE);
TEST_STEP_KEYS(g_keys_f_early,     MFB_KB_KEY_F1, MFB_KB_KEY_F2, MFB_KB_KEY_F3,
                                   MFB_KB_KEY_F4, MFB_KB_KEY_F5, MFB_KB_KEY_F6,
                                   MFB_KB_KEY_F7, MFB_KB_KEY_F8, MFB_KB_KEY_F9,
                                   MFB_KB_KEY_F10);
TEST_STEP_KEYS(g_keys_f_late,      MFB_KB_KEY_F11, MFB_KB_KEY_F12);

#define TEST_STEP_LIST(name) name, (unsigned) (sizeof(name) / sizeof(name[0]))
#define TEST_STEP_NO_KEYS    NULL, 0u

static const TestStep g_steps[STEP_COUNT] = {
    { true,  "A key",
             "Press and release A without any modifier.",
             "press, release and U+0061", TEST_STEP_NO_KEYS },
    { true,  "Space key",
             "Press and release Space.",
             "press, release and U+0020", TEST_STEP_NO_KEYS },
    { true,  "auto-repeat",
             "Hold R until this line says it repeats, then release R.",
             "repeated presses observed", TEST_STEP_NO_KEYS },
    { TEST_OVERLAPPING_SHIFT_REQUIRED, "overlapping Shift",
             "Hold Left Shift, press Right Shift, release Left Shift, then release Right Shift.",
             "left and right tracked independently", TEST_STEP_LIST(g_keys_shift) },
#if !TEST_DOS_PLATFORM
    { true,  "focus handling",
             "Hold F, switch to another application with the mouse, release F there, then come back.",
             "focus loss, outside release and focus return completed", TEST_STEP_NO_KEYS },
#endif

    { false, "function keys F1 to F10",
             "Press F1 through F10.",
             "all ten exercised", TEST_STEP_LIST(g_keys_f_early) },
    { false, "function keys F11 and F12",
             "Press F11 and F12. The desktop may keep one of the two.",
             "the late ones reached the application", TEST_STEP_LIST(g_keys_f_late) },
    { false, "Print Screen",
             "Press Print Screen. Most desktops keep this key for their screenshot tool, so the test may never see it.",
             "exercised", TEST_STEP_NO_KEYS },
    { false, "Scroll Lock",
             "Press Scroll Lock. MiniFB has no modifier bit for it, only the key.",
             "exercised", TEST_STEP_NO_KEYS },
    { false, "Pause key",
             "Press and release Pause.",
             "exercised", TEST_STEP_NO_KEYS },

    { false, "keys with no text",
             "Press Enter, Tab and Backspace once each. None should deliver a character.",
             "exercised", TEST_STEP_LIST(g_keys_no_text) },
    { false, "positional naming",
             "Press the key between Left Shift and Z. Then, with a tall Enter, the last key of the "
             "Caps Lock row; with a wide Enter, the key just above Enter.",
             "both named by position", TEST_STEP_LIST(g_keys_positional) },
    { false, "top left key",
             "Press the key below Escape, at the top left.",
             "named by position", TEST_STEP_NO_KEYS },
    { false, "left modifiers",
             "Press and release Left Control and Left Alt.",
             "both exercised", TEST_STEP_LIST(g_keys_left_mods) },
    { false, "left Super",
             "Press the left Windows or Command key. The desktop usually keeps it.",
             "exercised", TEST_STEP_NO_KEYS },
    { false, "right modifiers",
             "Press and release Right Control and Right Alt.",
             "both exercised", TEST_STEP_LIST(g_keys_right_mods) },
#if !TEST_DOS_PLATFORM
    { false, "AltGr companion",
             "Press and release AltGr, the right Alt, ten times. Speed does not matter.",
             "ten presses exercised", TEST_STEP_NO_KEYS },
#endif
    { false, "right Super",
             "Press the right Windows or Command key. Many keyboards do not have one.",
             "exercised", TEST_STEP_NO_KEYS },
    { false, "Menu key",
             "Press the Menu key, the one that opens the context menu.",
             "exercised", TEST_STEP_NO_KEYS },

    { false, "navigation block",
             "Press Home, End, Page Up, Page Down, Insert and Delete.",
             "the whole block exercised", TEST_STEP_LIST(g_keys_navigation) },
    { false, "arrow keys",
             "Press the four arrows.",
             "all four exercised", TEST_STEP_LIST(g_keys_arrows) },
    { false, "duplicated 1",
             "Press the 1 of the number row, then the 1 of the keypad.",
             "the two 1 keys gave different tokens", TEST_STEP_LIST(g_keys_dup_digit) },
    { false, "duplicated slash",
             "Press the key left of Right Shift, then the divide of the keypad.",
             "main block and keypad gave different tokens", TEST_STEP_LIST(g_keys_dup_slash) },
    { false, "keypad block",
             "With Num Lock on, press every key of the keypad.",
             "the whole block exercised", TEST_STEP_LIST(g_keys_keypad_all) },
    { false, "lock keys",
             TEST_LOCK_KEYS_ACTION,
             TEST_LOCK_KEYS_DONE, TEST_STEP_NO_KEYS },

    { false, "modifiers on mouse",
             "Hold Right Alt, click inside the window, then release it.",
             "the held modifier reached a mouse callback", TEST_STEP_NO_KEYS },
    { false, "quick tap",
             "Tap Q several times. The test slows down here so a tap fits in one event pump.",
             "a press and its release arrived in one event pump", TEST_STEP_NO_KEYS },
#if !TEST_DOS_PLATFORM
    { false, "dead acute plus E",
             "With a dead acute key, type acute and then E.",
             "composed accented E observed", TEST_STEP_NO_KEYS },
    { false, "cancelled composition",
             "Type acute and then X, a composition that cannot complete.",
             "accent and X observed", TEST_STEP_NO_KEYS },
    { false, "composition and focus",
             "Press the dead acute, switch to another application, come back, then press E.",
             "exercised", TEST_STEP_NO_KEYS },
    { false, "emoji",
             "Insert an emoji with the system character picker, including a coloured circle "
             "or square if the picker offers one.",
             "supplementary Unicode scalar observed", TEST_STEP_NO_KEYS },
    { false, "CJK text",
             "Commit a short CJK string with an input method.",
             "CJK scalars observed", TEST_STEP_NO_KEYS },
#endif
};
#define SKIP_KEY         MFB_KB_KEY_ESCAPE
#define TEST_FPS         60u
#define TEST_ALTGR_PRESSES 10u
#define TEST_SLOW_FPS    5u

// Enough to repeat every error at the end without a dynamic array. report_error collapses
// each class to one line through a bit of reported_errors, and the only other source is one
// line per required step, so tying the size to the step count keeps it ahead on its own.
#define TEST_MAX_LOGGED_ERRORS (STEP_COUNT + 16u)
#define TEST_ERROR_TEXT        160u

#define DEPRESSED_MOD_MASK ((uint32_t) (MFB_KB_MOD_SHIFT   | \
                                       MFB_KB_MOD_CONTROL | \
                                       MFB_KB_MOD_ALT     | \
                                       MFB_KB_MOD_SUPER))
#define LOCK_MOD_MASK      ((uint32_t) (MFB_KB_MOD_CAPS_LOCK | MFB_KB_MOD_NUM_LOCK))

#if defined(__ANDROID__) || (defined(TARGET_OS_IOS) && TARGET_OS_IOS)
    #define TEST_EXCLUDED_MOBILE_PLATFORM 1
#else
    #define TEST_EXCLUDED_MOBILE_PLATFORM 0
#endif

typedef struct {
    struct mfb_window *window;
    bool               expected_keys[TEST_KEY_COUNT];
    bool               awaiting_release[TEST_KEY_COUNT];
    unsigned           press_count[TEST_KEY_COUNT];
    unsigned           release_count[TEST_KEY_COUNT];
    unsigned           last_press_pump[TEST_KEY_COUNT];
    bool               expected_active;
    bool               finalized;
    bool               saw_char_a;
    bool               saw_char_space;
    bool               saw_non_ascii;
    bool               saw_supplementary;
    bool               saw_composed_e_acute;
    bool               saw_cancelled_acute_x;
    bool               saw_cjk;
    bool               saw_control;
    bool               saw_quick_tap;
    bool               saw_extra_mods;
    bool               lock_mods_known;
    bool               lock_step_active;
    bool               altgr_step_active;
    bool               altgr_control_stuck;
    unsigned           altgr_presses;
    unsigned           altgr_companion_seen;
    bool               lock_step_baseline_known;
    bool               lock_change_unreported;
    bool               dead_focus_active;
    bool               dead_focus_left;
    bool               dead_focus_returned;
    bool               saw_dead_focus_clear;
    bool               composition_kept;
    bool               saw_mouse_mods;
    bool               grave_step_active;
    bool               grave_seen_in_step;
    bool               saw_geometric_shape;
    bool               caps_lock_changed_while_pressed;
    bool               num_lock_changed_while_pressed;
    bool               non_text_key_valid;
    bool               collapsing_repeats;
    bool               focus_test_armed;
    bool               focus_key_down;
    bool               focus_release_candidate;
    bool               focus_loss_seen;
    bool               focus_release_callback;
    bool               focus_reset_pending;
    bool               skipped_required;
    bool               focus_state_cleared;
    bool               focus_returned;
    bool               pending_key_valid;
    bool               pending_char_valid;
    mfb_key            pending_key;
    unsigned           pending_key_pump;
    unsigned           pending_char;
    unsigned           pending_char_pump;
    unsigned           pump_serial;
    unsigned           shift_stage;
    unsigned           awaiting_release_count;
    unsigned           unknown_key_callbacks;
    unsigned           same_pump_transition_count;
    unsigned           key_before_char_count;
    unsigned           char_before_key_count;
    unsigned           cjk_count;
    unsigned           caps_lock_transitions;
    unsigned           num_lock_transitions;
    unsigned           error_count;
    unsigned           first_non_ascii;
    unsigned           first_supplementary;
    unsigned           first_geometric_shape;
    unsigned           first_control;
    uint32_t           first_extra_mods;
    uint32_t           observed_lock_mods;
    uint32_t           lock_step_baseline;
    mfb_key            first_extra_mods_key;
    unsigned           non_text_key_char;
    unsigned           non_text_key_pump;
    unsigned           collapsed_presses;
    unsigned           collapsed_chars;
    mfb_key            collapsed_key;
    mfb_key            non_text_key;
    unsigned           previous_char;
    bool               step_done[STEP_COUNT];
    bool               step_skipped[STEP_COUNT];
    uint64_t           reported_errors;
    char               error_log[TEST_MAX_LOGGED_ERRORS][TEST_ERROR_TEXT];
} KeyboardTest;

static void flush_repeats(KeyboardTest *test);

// Guidance for the person running the test goes to stderr, the record goes to stdout, so
// redirecting stdout to a file leaves the steps and the progress on screen. The web has no
// redirection, and there stderr reaches console.error, which makes the browser render the
// whole accumulated Asyncify await chain on every line, so both share stdout there.
#if defined(__EMSCRIPTEN__)
    #define TEST_GUIDE_STREAM stdout
#else
    #define TEST_GUIDE_STREAM stderr
#endif

static void
guide(const char *format, ...) {
    va_list arguments;
    va_start(arguments, format);
    vfprintf(TEST_GUIDE_STREAM, format, arguments);
    va_end(arguments);
    fputc('\n', TEST_GUIDE_STREAM);
    fflush(TEST_GUIDE_STREAM);
}

// Errors are printed where they happen, hundreds of lines above the verdict, so a copy of
// each one is kept to repeat at the end. Past the last slot only the count grows.
static void
record_error(KeyboardTest *test, const char *text) {
    if (test->error_count < TEST_MAX_LOGGED_ERRORS) {
        snprintf(test->error_log[test->error_count], TEST_ERROR_TEXT, "%s", text);
    }

    test->error_count++;
    guide("ERROR: %s", text);
}

static void
report_error(KeyboardTest *test, uint64_t error, const char *format, ...) {
    if ((test->reported_errors & error) != 0) {
        return;
    }

    flush_repeats(test);

    test->reported_errors |= error;

    char    text[TEST_ERROR_TEXT];
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(text, sizeof(text), format, arguments);
    va_end(arguments);

    record_error(test, text);
}

static void
report_missing(KeyboardTest *test, const char *action) {
    char text[TEST_ERROR_TEXT];

    flush_repeats(test);
    snprintf(text, sizeof(text), "required action was not observed: %s", action);
    record_error(test, text);
}

static void
report_step_done(KeyboardTest *test, TestStepId step) {
    if (test->step_done[step] == true) {
        return;
    }

    flush_repeats(test);
    test->step_done[step] = true;
    guide("DONE: %s, %s", g_steps[step].label, g_steps[step].done_text);
}

static KeyboardTest *
get_test(struct mfb_window *window) {
    return (KeyboardTest *) mfb_get_user_data(window);
}

static const char *
key_name_or_unknown(mfb_key key) {
    const char *name = mfb_get_key_name(key);
    return name != NULL ? name : "Unknown";
}

// A held key repeats many times per second. Printing every callback buries everything else,
// so a run of repeats for one key is counted and reported as a single line.
static void
flush_repeats(KeyboardTest *test) {
    if (test->collapsing_repeats == false) {
        return;
    }

    test->collapsing_repeats = false;
    printf("  KEY: %-16s repeated %u time%s",
           key_name_or_unknown(test->collapsed_key),
           test->collapsed_presses, test->collapsed_presses == 1 ? "" : "s");
    if (test->collapsed_chars > 0) {
        printf(", with %u character%s",
               test->collapsed_chars, test->collapsed_chars == 1 ? "" : "s");
    }
    printf("\n");
    fflush(stdout);

    test->collapsed_presses = 0;
    test->collapsed_chars = 0;
}

// Keys the contract excludes from text input: the application gets the physical key
// and decides for itself whether to insert anything.
static bool
key_produces_no_text(mfb_key key) {
    return key == MFB_KB_KEY_ENTER     || key == MFB_KB_KEY_KP_ENTER ||
           key == MFB_KB_KEY_TAB       || key == MFB_KB_KEY_ESCAPE   ||
           key == MFB_KB_KEY_BACKSPACE || key == MFB_KB_KEY_DELETE;
}

static bool
key_is_valid(mfb_key key) {
    int key_index = (int) key;
    return key_index >= 0 && key_index <= (int) MFB_KB_KEY_LAST;
}

static uint32_t
expected_depressed_mods(const KeyboardTest *test) {
    uint32_t mods = 0;

    if (test->expected_keys[MFB_KB_KEY_LEFT_SHIFT] == true ||
        test->expected_keys[MFB_KB_KEY_RIGHT_SHIFT] == true) {
        mods |= MFB_KB_MOD_SHIFT;
    }
    if (test->expected_keys[MFB_KB_KEY_LEFT_CONTROL] == true ||
        test->expected_keys[MFB_KB_KEY_RIGHT_CONTROL] == true) {
        mods |= MFB_KB_MOD_CONTROL;
    }
    if (test->expected_keys[MFB_KB_KEY_LEFT_ALT] == true ||
        test->expected_keys[MFB_KB_KEY_RIGHT_ALT] == true) {
        mods |= MFB_KB_MOD_ALT;
    }
    if (test->expected_keys[MFB_KB_KEY_LEFT_SUPER] == true ||
        test->expected_keys[MFB_KB_KEY_RIGHT_SUPER] == true) {
        mods |= MFB_KB_MOD_SUPER;
    }

    return mods;
}

static uint32_t
lock_mod_for_key(mfb_key key) {
    if (key == MFB_KB_KEY_CAPS_LOCK) {
        return MFB_KB_MOD_CAPS_LOCK;
    }
    if (key == MFB_KB_KEY_NUM_LOCK) {
        return MFB_KB_MOD_NUM_LOCK;
    }

    return 0;
}

static const char *
lock_state_text(const KeyboardTest *test, uint32_t bit) {
    return ((test->observed_lock_mods ^ test->lock_step_baseline) & bit) == 0
           ? "at its starting state" : "not where it started";
}

// Two toggles each and both left where they started, and none of that is visible from the
// keyboard, so a step that says nothing until it is satisfied is a step pressed blindly.
static void
report_lock_progress(KeyboardTest *test) {
    if (TEST_NUM_LOCK_EXISTS == false) {
        guide("   lock keys: Caps Lock %u toggles, %s",
              test->caps_lock_transitions, lock_state_text(test, MFB_KB_MOD_CAPS_LOCK));
        return;
    }

    guide("   lock keys: Caps Lock %u toggles, %s; Num Lock %u toggles, %s",
          test->caps_lock_transitions, lock_state_text(test, MFB_KB_MOD_CAPS_LOCK),
          test->num_lock_transitions, lock_state_text(test, MFB_KB_MOD_NUM_LOCK));
}

static void
check_lock_modifiers(KeyboardTest *test, mfb_key key, mfb_key_mod mod,
                     bool is_pressed, bool is_repeat) {
    uint32_t actual_mods = (uint32_t) mod & LOCK_MOD_MASK;
    uint32_t key_lock_mod = lock_mod_for_key(key);
    bool     report_lock = false;

    if (test->lock_step_active == true && is_pressed == true && is_repeat == false) {
        if (key_lock_mod == MFB_KB_MOD_CAPS_LOCK) {
            test->caps_lock_changed_while_pressed = false;
        }
        else if (key_lock_mod == MFB_KB_MOD_NUM_LOCK) {
            test->num_lock_changed_while_pressed = false;
        }
    }

    if (test->lock_mods_known == false) {
        test->lock_mods_known = true;
        test->observed_lock_mods = actual_mods;
        if (test->lock_step_active == true) {
            if (test->lock_step_baseline_known == false) {
                test->lock_step_baseline = actual_mods;
                test->lock_step_baseline_known = true;
            }
            // The toggle this very press caused happened before the event that restores the
            // reference, so it cannot be seen, and its release must not be blamed for it.
            if (is_pressed == true && key_lock_mod == MFB_KB_MOD_CAPS_LOCK) {
                test->caps_lock_changed_while_pressed = true;
            }
            else if (is_pressed == true && key_lock_mod == MFB_KB_MOD_NUM_LOCK) {
                test->num_lock_changed_while_pressed = true;
            }
        }
        return;
    }

    uint32_t changed_mods = actual_mods ^ test->observed_lock_mods;
    if ((changed_mods & ~key_lock_mod) != 0) {
        if (TEST_LOCK_MODS_VERIFIABLE == true) {
            report_error(test, ERROR_LOCK_MODIFIER_STATE,
                         "key %s callback changed unrelated lock modifiers from 0x%02x to 0x%02x",
                         key_name_or_unknown(key),
                         (unsigned) test->observed_lock_mods, (unsigned) actual_mods);
        }
        else {
            test->lock_change_unreported = true;
        }
    }
    else if (test->lock_step_active == true) {
        if ((changed_mods & MFB_KB_MOD_CAPS_LOCK) != 0) {
            test->caps_lock_transitions++;
            test->caps_lock_changed_while_pressed = true;
        }
        if ((changed_mods & MFB_KB_MOD_NUM_LOCK) != 0) {
            test->num_lock_transitions++;
            test->num_lock_changed_while_pressed = true;
        }
        report_lock = (changed_mods & LOCK_MOD_MASK) != 0;
    }

    test->observed_lock_mods = actual_mods;

    if (report_lock == true) {
        report_lock_progress(test);
    }

    if (test->lock_step_active == true && is_pressed == false) {
        if (key_lock_mod == MFB_KB_MOD_CAPS_LOCK &&
            test->caps_lock_changed_while_pressed == false) {
            if (TEST_LOCK_MODS_VERIFIABLE == true) {
                report_error(test, ERROR_LOCK_MODIFIER_STATE,
                             "Caps Lock completed without changing MFB_KB_MOD_CAPS_LOCK");
            }
            else {
                test->lock_change_unreported = true;
            }
        }
        else if (key_lock_mod == MFB_KB_MOD_NUM_LOCK &&
                 TEST_NUM_LOCK_EXISTS == true &&
                 test->num_lock_changed_while_pressed == false) {
            if (TEST_LOCK_MODS_VERIFIABLE == true) {
                report_error(test, ERROR_LOCK_MODIFIER_STATE,
                             "Num Lock completed without changing MFB_KB_MOD_NUM_LOCK");
            }
            else {
                test->lock_change_unreported = true;
            }
        }
    }
}

static void
check_key_snapshot(KeyboardTest *test) {
    const uint8_t *keys = mfb_get_key_buffer(test->window);
    if (keys == NULL) {
        report_error(test, ERROR_KEY_BUFFER_NULL,
                     "mfb_get_key_buffer returned NULL for a valid window");
        return;
    }

    for (unsigned key = 0; key < TEST_KEY_COUNT; ++key) {
        bool actual = keys[key] != 0;
        if (actual != test->expected_keys[key]) {
            report_error(test, ERROR_KEY_SNAPSHOT,
                         "key %u snapshot is %d, but callback history says it should be %d",
                         key, actual, test->expected_keys[key]);
            return;
        }
    }
}


static bool
key_matches_ascii(mfb_key key, unsigned codepoint) {
    if (key == MFB_KB_KEY_SPACE) {
        return codepoint == UINT32_C(0x20);
    }

    if (key >= MFB_KB_KEY_A && key <= MFB_KB_KEY_Z) {
        unsigned offset = (unsigned) key - (unsigned) MFB_KB_KEY_A;
        return codepoint == (unsigned) 'a' + offset ||
               codepoint == (unsigned) 'A' + offset;
    }

    return false;
}

static void
record_key_char_order_on_press(KeyboardTest *test, mfb_key key) {
    if (test->pending_char_valid == true &&
        test->pending_char_pump == test->pump_serial &&
        key_matches_ascii(key, test->pending_char) == true) {
        test->char_before_key_count++;
        report_error(test, ERROR_CALLBACK_ORDER,
                     "character U+%04X arrived before the %s key callback that produced it",
                     test->pending_char, key_name_or_unknown(key));
        test->pending_char_valid = false;
        test->pending_key_valid = false;
        return;
    }

    test->pending_key = key;
    test->pending_key_pump = test->pump_serial;
    test->pending_key_valid = true;
}

static void
record_key_char_order_on_char(KeyboardTest *test, unsigned codepoint) {
    if (test->pending_key_valid == true &&
        test->pending_key_pump == test->pump_serial &&
        key_matches_ascii(test->pending_key, codepoint) == true) {
        test->key_before_char_count++;
        test->pending_key_valid = false;
        test->pending_char_valid = false;
        return;
    }

    test->pending_char = codepoint;
    test->pending_char_pump = test->pump_serial;
    test->pending_char_valid = true;
}

static bool
is_cjk_codepoint(unsigned codepoint) {
    return (codepoint >= UINT32_C(0x3040) && codepoint <= UINT32_C(0x30ff)) ||
           (codepoint >= UINT32_C(0x3400) && codepoint <= UINT32_C(0x9fff)) ||
           (codepoint >= UINT32_C(0xac00) && codepoint <= UINT32_C(0xd7af));
}

static void
update_shift_stage(KeyboardTest *test, mfb_key key, bool is_pressed) {
    switch (test->shift_stage) {
        case 0:
            if (key == MFB_KB_KEY_LEFT_SHIFT && is_pressed == true) {
                test->shift_stage = 1;
            }
            break;

        case 1:
            if (key == MFB_KB_KEY_RIGHT_SHIFT && is_pressed == true) {
                test->shift_stage = 2;
            }
            else if (key == MFB_KB_KEY_LEFT_SHIFT && is_pressed == false) {
                test->shift_stage = 0;
            }
            break;

        case 2:
            if (key == MFB_KB_KEY_LEFT_SHIFT && is_pressed == false) {
                test->shift_stage = 3;
            }
            else if (key == MFB_KB_KEY_RIGHT_SHIFT && is_pressed == false) {
                test->shift_stage = 1;
            }
            break;

        case 3:
            if (key == MFB_KB_KEY_RIGHT_SHIFT && is_pressed == false) {
                test->shift_stage = 4;
            }
            break;

        default:
            break;
    }
}

// One place decides whether a step is finished, so the NEXT line, the DONE line and the
// final summary can never disagree about it.
static void
evaluate_steps(const KeyboardTest *test, bool *done) {
    done[STEP_A] = test->press_count[MFB_KB_KEY_A] > 0 &&
                   test->release_count[MFB_KB_KEY_A] > 0 &&
                   test->saw_char_a == true;
    done[STEP_SPACE] = test->press_count[MFB_KB_KEY_SPACE] > 0 &&
                       test->release_count[MFB_KB_KEY_SPACE] > 0 &&
                       test->saw_char_space == true;
    done[STEP_REPEAT] = test->press_count[MFB_KB_KEY_R] >= 2;
    done[STEP_SHIFT] = test->shift_stage >= 4;
#if !TEST_DOS_PLATFORM
    done[STEP_FOCUS] = test->focus_returned;
#endif
    done[STEP_NO_TEXT] = test->press_count[MFB_KB_KEY_ENTER] > 0 &&
                         test->press_count[MFB_KB_KEY_TAB] > 0 &&
                         test->press_count[MFB_KB_KEY_BACKSPACE] > 0;
    done[STEP_LEFT_MODIFIERS] = test->press_count[MFB_KB_KEY_LEFT_CONTROL] > 0 &&
                                test->press_count[MFB_KB_KEY_LEFT_ALT] > 0;
    done[STEP_RIGHT_MODIFIERS] = test->press_count[MFB_KB_KEY_RIGHT_CONTROL] > 0 &&
                                 test->press_count[MFB_KB_KEY_RIGHT_ALT] > 0;
#if !TEST_DOS_PLATFORM
    done[STEP_ALTGR] = test->altgr_presses >= TEST_ALTGR_PRESSES;
#endif
    done[STEP_LEFT_SUPER] = test->press_count[MFB_KB_KEY_LEFT_SUPER] > 0;
    done[STEP_RIGHT_SUPER] = test->press_count[MFB_KB_KEY_RIGHT_SUPER] > 0;
    done[STEP_MENU] = test->press_count[MFB_KB_KEY_MENU] > 0;
    // The key next to the tall Enter prints c-cedilla on a Spanish board and sits where an
    // ANSI one puts the backslash, so its name has to come from the position and not the text.
    done[STEP_POSITIONAL] = (test->press_count[MFB_KB_KEY_WORLD_1] > 0 ||
                             test->press_count[MFB_KB_KEY_WORLD_2] > 0) &&
                            test->press_count[MFB_KB_KEY_BACKSLASH] > 0;
    done[STEP_GRAVE_ACCENT] = test->grave_seen_in_step;
    done[STEP_DUPLICATED_DIGIT] = test->press_count[MFB_KB_KEY_1] > 0 &&
                                  test->press_count[MFB_KB_KEY_KP_1] > 0;
    done[STEP_DUPLICATED_SLASH] = test->press_count[MFB_KB_KEY_SLASH] > 0 &&
                                  test->press_count[MFB_KB_KEY_KP_DIVIDE] > 0;
    done[STEP_KEYPAD] = true;
    for (unsigned i = 0; i < (sizeof(g_keys_keypad_all) / sizeof(g_keys_keypad_all[0])); ++i) {
        if (test->press_count[g_keys_keypad_all[i]] == 0) {
            done[STEP_KEYPAD] = false;
            break;
        }
    }
    done[STEP_PAUSE] = test->press_count[MFB_KB_KEY_PAUSE] > 0;
    done[STEP_LOCK_KEYS] = test->lock_step_baseline_known == true &&
                           test->caps_lock_transitions >= 2 &&
                           (TEST_NUM_LOCK_EXISTS == false ||
                            test->num_lock_transitions >= 2) &&
                           test->observed_lock_mods == test->lock_step_baseline &&
                           test->expected_keys[MFB_KB_KEY_CAPS_LOCK] == false &&
                           test->expected_keys[MFB_KB_KEY_NUM_LOCK] == false;
    done[STEP_SCROLL_LOCK] = test->press_count[MFB_KB_KEY_SCROLL_LOCK] > 0;
    done[STEP_PRINT_SCREEN] = test->press_count[MFB_KB_KEY_PRINT_SCREEN] > 0;
    done[STEP_NAVIGATION] = test->press_count[MFB_KB_KEY_HOME] > 0 &&
                            test->press_count[MFB_KB_KEY_END] > 0 &&
                            test->press_count[MFB_KB_KEY_PAGE_UP] > 0 &&
                            test->press_count[MFB_KB_KEY_PAGE_DOWN] > 0 &&
                            test->press_count[MFB_KB_KEY_INSERT] > 0 &&
                            test->press_count[MFB_KB_KEY_DELETE] > 0;
    done[STEP_ARROWS] = test->press_count[MFB_KB_KEY_UP] > 0 &&
                        test->press_count[MFB_KB_KEY_DOWN] > 0 &&
                        test->press_count[MFB_KB_KEY_LEFT] > 0 &&
                        test->press_count[MFB_KB_KEY_RIGHT] > 0;
    // F11 and F12 came late, and DOS gave them 0x57 and 0x58 instead of continuing the run
    // that ends at F10, so a table built by counting upwards loses them.
    done[STEP_FUNCTION_EARLY] = true;
    for (unsigned i = 0; i < (sizeof(g_keys_f_early) / sizeof(g_keys_f_early[0])); ++i) {
        if (test->press_count[g_keys_f_early[i]] == 0) {
            done[STEP_FUNCTION_EARLY] = false;
            break;
        }
    }
    done[STEP_FUNCTION_LATE] = test->press_count[MFB_KB_KEY_F11] > 0 &&
                               test->press_count[MFB_KB_KEY_F12] > 0;
    done[STEP_MOUSE_MODS] = test->saw_mouse_mods;
    done[STEP_QUICK_TAP] = test->saw_quick_tap;
#if !TEST_DOS_PLATFORM
    // The DOS backend maps scancodes through a US ASCII table, so it has no dead keys, no
    // composition and no Unicode. These are not slow to reach there, they are unreachable.
    done[STEP_DEAD_ACUTE] = test->saw_composed_e_acute;
    done[STEP_CANCELLED_ACUTE] = test->saw_cancelled_acute_x;
    done[STEP_DEAD_FOCUS] = test->saw_dead_focus_clear || test->composition_kept;
    done[STEP_EMOJI] = test->saw_supplementary;
    // One scalar would pass even on a backend that delivers only the first of a commit.
    done[STEP_CJK] = test->cjk_count >= 2;
#endif
}

static bool
step_is_pending(const KeyboardTest *test, TestStepId step) {
    return test->step_done[step] == false && test->step_skipped[step] == false;
}

static int
pending_step(const KeyboardTest *test) {
    for (unsigned step = 0; step < STEP_COUNT; ++step) {
        if (step_is_pending(test, (TestStepId) step) == true) {
            return (int) step;
        }
    }

    return -1;
}

static void
announce_next_step(KeyboardTest *test) {
    int step = pending_step(test);

    test->grave_step_active = (step == STEP_GRAVE_ACCENT);

#if !TEST_DOS_PLATFORM
    test->dead_focus_active = (step == STEP_DEAD_FOCUS);

    if (step == STEP_ALTGR && test->altgr_step_active == false) {
        test->altgr_presses = 0;
        test->altgr_companion_seen = 0;
    }
    test->altgr_step_active = (step == STEP_ALTGR);
#endif

    if (step == STEP_LOCK_KEYS && test->lock_step_active == false) {
        test->lock_step_active = true;
        test->caps_lock_transitions = 0;
        test->num_lock_transitions = 0;
        test->caps_lock_changed_while_pressed = false;
        test->num_lock_changed_while_pressed = false;
        test->lock_step_baseline_known = test->lock_mods_known;
        if (test->lock_step_baseline_known == true) {
            test->lock_step_baseline = test->observed_lock_mods;
        }
    }

    // A pump at the normal rate is about eight milliseconds, shorter than any human tap, so
    // the quick tap step can only be reached while the loop runs slowly.
    mfb_set_target_fps(step == STEP_QUICK_TAP ? TEST_SLOW_FPS : TEST_FPS);

    guide("");
    if (step < 0) {
        guide("NEXT: nothing left. Press Escape or close the window to finish.");
        return;
    }

    guide("NEXT: %s (step %d of %d, %s).",
          g_steps[step].action,
          step + 1, (int) STEP_COUNT,
          g_steps[step].required == true ? "required" : "optional"
    );
    guide("      Escape skips this step.");

    if (step == STEP_LOCK_KEYS && test->lock_step_baseline_known == true) {
        guide("      Caps Lock is %s and Num Lock is %s right now.",
              (test->lock_step_baseline & MFB_KB_MOD_CAPS_LOCK) != 0 ? "on" : "off",
              (test->lock_step_baseline & MFB_KB_MOD_NUM_LOCK) != 0 ? "on" : "off");
    }
}

// A step that asks for several keys is opaque while it waits: it either completes or it does
// not, and nothing says which key never arrived. Both of these speak for it.
static void
report_step_key(KeyboardTest *test, mfb_key key) {
    int step = pending_step(test);
    if (step < 0 || g_steps[step].keys == NULL) {
        return;
    }

    unsigned seen = 0;
    bool     wanted = false;
    for (unsigned i = 0; i < g_steps[step].key_count; ++i) {
        mfb_key wanted_key = g_steps[step].keys[i];
        if (wanted_key == key) {
            wanted = true;
        }
        if (test->press_count[wanted_key] > 0) {
            seen++;
        }
    }

    if (wanted == true) {
        guide("   OK: %s, %u of %u for %s",
              key_name_or_unknown(key), seen, g_steps[step].key_count, g_steps[step].label);
    }
}

static void
report_step_missing(KeyboardTest *test, int step) {
    if (g_steps[step].keys == NULL) {
        return;
    }

    char     missing[160];
    unsigned used = 0;
    for (unsigned i = 0; i < g_steps[step].key_count; ++i) {
        mfb_key key = g_steps[step].keys[i];
        if (test->press_count[key] > 0) {
            continue;
        }
        int written = snprintf(missing + used, sizeof(missing) - used, "%s%s",
                               used == 0 ? "" : ", ", key_name_or_unknown(key));
        if (written <= 0 || (unsigned) written >= sizeof(missing) - used) {
            break;
        }
        used += (unsigned) written;
    }

    if (used > 0) {
        guide("      never seen: %s", missing);
    }
}

static void
skip_pending_step(KeyboardTest *test) {
    int step = pending_step(test);

    if (step < 0) {
        return;
    }

    flush_repeats(test);
    test->step_skipped[step] = true;
    if (step == STEP_LOCK_KEYS) {
        test->lock_step_active = false;
    }
    guide("SKIPPED: %s", g_steps[step].action);
    report_step_missing(test, step);
    announce_next_step(test);
}

static void
update_progress(KeyboardTest *test) {
    bool done[STEP_COUNT] = { false };
    bool advanced = false;

    evaluate_steps(test, done);

    for (unsigned step = 0; step < STEP_COUNT; ++step) {
        if (done[step] == true && test->step_done[step] == false &&
            test->step_skipped[step] == false) {
            report_step_done(test, (TestStepId) step);
            if (step == STEP_LOCK_KEYS) {
                test->lock_step_active = false;
            }
            advanced = true;
        }
    }

    if (advanced == true) {
        announce_next_step(test);
    }
}

// Windows reports the Control that AltGr presses of its own; the other backends report only
// the Alt. Counting from the Alt keeps a press that began before this step from counting half.
static void
check_altgr_companion(KeyboardTest *test, mfb_key key, bool is_pressed, bool is_repeat) {
    if (test->altgr_step_active == false || key != MFB_KB_KEY_RIGHT_ALT || is_repeat == true) {
        return;
    }

    if (is_pressed == true) {
        test->altgr_presses++;
        if (test->expected_keys[MFB_KB_KEY_LEFT_CONTROL] == true) {
            test->altgr_companion_seen++;
        }
        return;
    }

    // The companion comes up before the Alt does, so it has had its chance by now.
    if (test->expected_keys[MFB_KB_KEY_LEFT_CONTROL] == true) {
        test->altgr_control_stuck = true;
        report_error(test, ERROR_ALTGR_COMPANION,
                     "AltGr came up with its companion Control still held");
    }
}

// The step needs state of its own. The counters are global and update_progress completes any
// step it finds satisfied, even one not announced yet, so a key pressed during an earlier step
// would answer this one too.
static void
check_grave_step(KeyboardTest *test, mfb_key key, bool is_pressed, bool is_repeat) {
    if (test->grave_step_active == false || is_pressed == false || is_repeat == true) {
        return;
    }

    if (key == MFB_KB_KEY_GRAVE_ACCENT) {
        test->grave_seen_in_step = true;
    }
    else if (key == MFB_KB_KEY_WORLD_1 || key == MFB_KB_KEY_WORLD_2) {
        report_error(test, ERROR_GRAVE_KEY_TOKEN,
                     "the key below Escape reported %s, the token of the key next to left Shift",
                     key_name_or_unknown(key));
    }
}

static void
keyboard(struct mfb_window *window, mfb_key key, mfb_key_mod mod, bool is_pressed) {
    KeyboardTest *test = get_test(window);
    int key_index = (int) key;
    const char *key_name = key_name_or_unknown(key);
    bool is_repeat = is_pressed == true &&
                     key_is_valid(key) == true &&
                     test->expected_keys[key_index] == true;

    if (is_repeat == true) {
        if (test->collapsing_repeats == true && test->collapsed_key == key) {
            test->collapsed_presses++;
        }
        else {
            flush_repeats(test);
            test->collapsing_repeats = true;
            test->collapsed_key = key;
            test->collapsed_presses = 1;
        }
    }
    else {
        flush_repeats(test);
        printf("  KEY: %-16s %s mods=0x%02x pump=%u\n",
               key_name, is_pressed == true ? "pressed " : "released",
               (unsigned) mod, test->pump_serial);
        fflush(stdout);
    }

    if (key_is_valid(key) == false) {
        test->unknown_key_callbacks++;
        if (key == MFB_KB_KEY_UNKNOWN) {
            report_error(test, ERROR_UNKNOWN_KEY_REPORTED,
                         "keyboard callback reported MFB_KB_KEY_UNKNOWN; that key has no token");
        }
        else {
            report_error(test, ERROR_KEY_RANGE,
                         "keyboard callback reported out-of-range key %d", key_index);
        }
        return;
    }

    check_lock_modifiers(test, key, mod, is_pressed, is_repeat);

    const uint8_t *keys = mfb_get_key_buffer(window);
    if (keys == NULL) {
        report_error(test, ERROR_KEY_BUFFER_NULL,
                     "mfb_get_key_buffer returned NULL inside a keyboard callback");
    }
    else if ((keys[key_index] != 0) != is_pressed) {
        report_error(test, ERROR_CALLBACK_KEY_STATE,
                     "key %s getter is %d inside its %s callback",
                     key_name, keys[key_index] != 0,
                     is_pressed == true ? "press" : "release");
    }

    test->expected_keys[key_index] = is_pressed;
    if (is_pressed == true) {
        test->press_count[key_index]++;
        if (is_repeat == false) {
            report_step_key(test, key);
        }
        test->last_press_pump[key_index] = test->pump_serial;
        record_key_char_order_on_press(test, key);
        if (key_produces_no_text(key) == true) {
            test->non_text_key = key;
            test->non_text_key_pump = test->pump_serial;
            test->non_text_key_valid = true;
        }
    }
    else {
        test->release_count[key_index]++;
        if (test->awaiting_release[key_index] == true) {
            test->awaiting_release[key_index] = false;
            test->awaiting_release_count--;
        }
        if (test->last_press_pump[key_index] == test->pump_serial) {
            test->same_pump_transition_count++;
            if (key == MFB_KB_KEY_Q) {
                test->saw_quick_tap = true;
            }
        }
        test->last_press_pump[key_index] = 0;
        if (test->pending_key_valid == true && test->pending_key == key) {
            test->pending_key_valid = false;
        }
    }

    check_altgr_companion(test, key, is_pressed, is_repeat);
    check_grave_step(test, key, is_pressed, is_repeat);

    // A bit the callback history says should be there and is not is a defect. An extra bit
    // is the platform reporting a modifier it never delivered as a key, such as the Control
    // that AltGr sets on Windows, so it is recorded instead of failed.
    uint32_t expected_mods = expected_depressed_mods(test);
    uint32_t actual_mods = (uint32_t) mod & DEPRESSED_MOD_MASK;
    uint32_t missing_mods = expected_mods & ~actual_mods;
    uint32_t extra_mods = actual_mods & ~expected_mods;

    if (missing_mods != 0) {
        report_error(test, ERROR_MODIFIER_STATE,
                     "key %s callback reports depressed modifiers 0x%02x, missing 0x%02x",
                     key_name, (unsigned) actual_mods, (unsigned) missing_mods);
    }
    if (extra_mods != 0 && test->saw_extra_mods == false) {
        test->saw_extra_mods = true;
        test->first_extra_mods = extra_mods;
        test->first_extra_mods_key = key;
    }

    check_key_snapshot(test);
    update_shift_stage(test, key, is_pressed);

    if (key == MFB_KB_KEY_F) {
        if (is_pressed == true && test->focus_loss_seen == false) {
            test->focus_test_armed = true;
            test->focus_key_down = true;
            test->focus_release_candidate = false;
        }
        else if (is_pressed == false && test->focus_test_armed == true) {
            test->focus_key_down = false;
            test->focus_release_candidate = true;
            if (test->focus_loss_seen == true ||
                mfb_is_window_active(window) == false) {
                test->focus_release_callback = true;
            }
        }
    }

    update_progress(test);

    // Escape always means "I am done with this". While a step is waiting that is the step,
    // and once none are left it is the run.
    if (key == SKIP_KEY && is_pressed == false) {
        if (pending_step(test) >= 0) {
            skip_pending_step(test);
        }
        else {
            mfb_close(window);
        }
    }
}

static void
char_input(struct mfb_window *window, unsigned int codepoint) {
    KeyboardTest *test = get_test(window);

    if (test->collapsing_repeats == true) {
        test->collapsed_chars++;
    }
    else {
        printf("  CHAR: U+%04X pump=%u\n", codepoint, test->pump_serial);
        fflush(stdout);
    }

    record_key_char_order_on_char(test, codepoint);

    if (codepoint == 0 ||
        codepoint > UINT32_C(0x10ffff) ||
        (codepoint >= UINT32_C(0xd800) && codepoint <= UINT32_C(0xdfff))) {
        report_error(test, ERROR_INVALID_TEXT_CODEPOINT,
                     "char_input reported U+%04X, which is not a Unicode scalar value",
                     codepoint);
    }

    bool is_control = (codepoint > 0 && codepoint < UINT32_C(0x20)) ||
                      codepoint == UINT32_C(0x7f) ||
                      (codepoint >= UINT32_C(0x80) && codepoint <= UINT32_C(0x9f));
    if (is_control == true) {
        if (test->saw_control == false) {
            test->first_control = codepoint;
        }
        test->saw_control = true;
        report_error(test, ERROR_CONTROL_CODEPOINT,
                     "char_input reported the control code point U+%04X; text carries no controls",
                     codepoint);
    }

    if (test->non_text_key_valid == true &&
        test->non_text_key_pump == test->pump_serial) {
        test->non_text_key_char = codepoint;
        if (is_control == false) {
            report_error(test, ERROR_NON_TEXT_KEY_CHAR,
                         "%s produced the character U+%04X; that key delivers no text",
                         key_name_or_unknown(test->non_text_key), codepoint);
        }
    }

    if (codepoint == (unsigned) 'a') {
        test->saw_char_a = true;
    }
    if (codepoint == UINT32_C(0x20)) {
        test->saw_char_space = true;
    }
    if (codepoint > UINT32_C(0x7f)) {
        if (test->saw_non_ascii == false) {
            test->first_non_ascii = codepoint;
        }
        test->saw_non_ascii = true;
    }
    if (codepoint > UINT32_C(0xffff) && codepoint <= UINT32_C(0x10ffff)) {
        if (test->saw_supplementary == false) {
            test->first_supplementary = codepoint;
        }
        test->saw_supplementary = true;
    }
    // The record above accepts any scalar over U+FFFF. This block is narrower on purpose: it
    // is the one that a backend filtering the function key range with a sixteen bit mask
    // drops, and no wider record would show that.
    if (codepoint >= UINT32_C(0x1f700) && codepoint <= UINT32_C(0x1f7ff)) {
        if (test->saw_geometric_shape == false) {
            test->first_geometric_shape = codepoint;
        }
        test->saw_geometric_shape = true;
    }
    if (codepoint == UINT32_C(0x00e9) || codepoint == UINT32_C(0x00c9)) {
        test->saw_composed_e_acute = true;
    }
    if (test->dead_focus_returned == true) {
        test->dead_focus_returned = false;
        if (codepoint == UINT32_C(0x00e9) || codepoint == UINT32_C(0x00c9)) {
            if (TEST_COMPOSITION_SURVIVES_FOCUS == true) {
                test->composition_kept = true;
            }
            else {
                report_error(test, ERROR_COMPOSITION_AFTER_FOCUS,
                             "a composition abandoned before the focus was lost still produced U+%04X",
                             codepoint);
            }
        }
        else if (codepoint == (unsigned) 'e' || codepoint == (unsigned) 'E') {
            test->saw_dead_focus_clear = true;
        }
    }
    if ((test->previous_char == UINT32_C(0x00b4) ||
         test->previous_char == UINT32_C(0x0301)) &&
        (codepoint == (unsigned) 'x' || codepoint == (unsigned) 'X')) {
        test->saw_cancelled_acute_x = true;
    }
    if (is_cjk_codepoint(codepoint) == true) {
        test->saw_cjk = true;
        test->cjk_count++;
    }

    test->previous_char = codepoint;
    update_progress(test);
}

static void
active(struct mfb_window *window, bool is_active) {
    KeyboardTest *test = get_test(window);
    bool getter_active = mfb_is_window_active(window);

    flush_repeats(test);

    printf("  ACTIVE: %s pump=%u\n",
           is_active == true ? "true" : "false", test->pump_serial);
    fflush(stdout);

    if (getter_active != is_active) {
        report_error(test, ERROR_ACTIVE_GETTER,
                     "mfb_is_window_active is %d inside an active callback that reports %d",
                     getter_active, is_active);
    }
    test->expected_active = is_active;

    if (is_active == false) {
        for (unsigned key = 0; key < TEST_KEY_COUNT; ++key) {
            if (test->expected_keys[key] == true && test->awaiting_release[key] == false) {
                test->awaiting_release[key] = true;
                test->awaiting_release_count++;
            }
        }

        if (test->focus_key_down == true ||
            test->focus_release_candidate == true) {
            test->focus_loss_seen = true;
            if (test->focus_release_candidate == true) {
                test->focus_release_callback = true;
            }
        }
        test->focus_reset_pending = true;
        test->pending_key_valid = false;
        test->pending_char_valid = false;
        if (test->dead_focus_active == true) {
            test->dead_focus_left = true;
        }
    }
    else {
        // The state is taken from the platform when focus returns, with no callbacks, so a
        // key that moved while the window was away has no history here. It has to be read now
        // and not at the end of the pump: a key callback later in this same pump compares the
        // buffer against this expectation.
        const uint8_t *focus_keys = mfb_get_key_buffer(window);
        if (focus_keys != NULL) {
            for (unsigned key = 0; key < TEST_KEY_COUNT; ++key) {
                test->expected_keys[key] = focus_keys[key] != 0;
            }
        }
        test->lock_mods_known = false;
        if (test->focus_loss_seen == true) {
            test->focus_returned = true;
        }
        if (test->dead_focus_left == true) {
            test->dead_focus_returned = true;
            test->dead_focus_left = false;
        }
    }

    update_progress(test);
}

// The mod argument travels on mouse events too, and a backend that rebuilds it from the
// layout alone is only caught here, where no key event is on its way to correct it.
static void
mouse_button(struct mfb_window *window, mfb_mouse_button button, mfb_key_mod mod, bool is_pressed) {
    KeyboardTest *test = get_test(window);

    if (is_pressed == false) {
        return;
    }

    flush_repeats(test);
    printf("  MOUSE: button %d      pressed  mods=0x%02x pump=%u\n",
           (int) button, (unsigned) mod, test->pump_serial);
    fflush(stdout);

    uint32_t expected_mods = expected_depressed_mods(test);
    uint32_t actual_mods   = (uint32_t) mod & DEPRESSED_MOD_MASK;
    uint32_t missing_mods  = expected_mods & ~actual_mods;

    if (missing_mods != 0) {
        report_error(test, ERROR_MODIFIER_STATE,
                     "mouse button callback reports depressed modifiers 0x%02x, missing 0x%02x",
                     (unsigned) actual_mods, (unsigned) missing_mods);
    }
    else if (test->expected_keys[MFB_KB_KEY_RIGHT_ALT] == true) {
        // Any modifier exercises the check, but only AltGr proves the union: the layout calls
        // it no modifier, so the bit can come from nowhere but the held key.
        test->saw_mouse_mods = true;
    }

    update_progress(test);
}

static bool
window_close(struct mfb_window *window) {
    (void) window;
    return true;
}

static void
begin_pump(KeyboardTest *test) {
    test->pump_serial++;
    test->pending_key_valid = false;
    test->pending_char_valid = false;
}

static void
check_after_pump(KeyboardTest *test) {
    const uint8_t *keys = mfb_get_key_buffer(test->window);

    if (keys == NULL) {
        report_error(test, ERROR_KEY_BUFFER_NULL,
                     "mfb_get_key_buffer returned NULL after an event pump");
        return;
    }

    if (test->focus_reset_pending == true) {
        // Checking the buffer would be wrong here: focus can be lost and regained inside one
        // pump, and the state is resynchronized on the way back. What the contract promises
        // is a release callback for every key that was held.
        if (test->awaiting_release_count > 0) {
            report_error(test, ERROR_FOCUS_STATE,
                         "%u key%s held at focus loss never got a release callback",
                         test->awaiting_release_count,
                         test->awaiting_release_count == 1 ? "" : "s");
        }
        else {
            test->focus_state_cleared = true;
        }

        // Take the real state instead of assuming zero. A backend that leaves a key
        // pressed is already reported above, and assuming zero would report the same
        // cause again on every later snapshot check.
        for (unsigned key = 0; key < TEST_KEY_COUNT; ++key) {
            test->expected_keys[key] = keys[key] != 0;
        }
        test->focus_key_down = false;
        test->focus_reset_pending = false;
    }
    else {
        check_key_snapshot(test);
    }

    bool active_now = mfb_is_window_active(test->window);
    if (active_now != test->expected_active) {
        report_error(test, ERROR_ACTIVE_GETTER,
                     "active getter changed from %d to %d without a matching callback",
                     test->expected_active, active_now);
    }

    update_progress(test);
}


// A step reads the same way in the summary whether it was done, skipped or never reached.
// It re-evaluates from the final state, so a step already reported as done has to stay done.
static const char *
step_result(const KeyboardTest *test, TestStepId step, const bool *done) {
    if (test->step_skipped[step] == true) {
        return "SKIPPED";
    }
    if (done[step] == true || test->step_done[step] == true) {
        return g_steps[step].done_text;
    }

    return g_steps[step].required == true ? "NOT DONE" : "not reached";
}

static void
print_summary(const KeyboardTest *test) {
    bool done[STEP_COUNT] = { false };

    evaluate_steps(test, done);

    puts("");
    puts("KEYBOARD CONTRACT SUMMARY");
    for (unsigned step = 0; step < STEP_COUNT; ++step) {
        if (g_steps[step].required == true) {
            printf("  %-24s %s\n", g_steps[step].label,
                   step_result(test, (TestStepId) step, done));
        }
    }
    printf("  %-24s %s\n", "callback key getter",
           (test->reported_errors & ERROR_CALLBACK_KEY_STATE) == 0 ? "consistent" : "INCONSISTENT");
    printf("  %-24s %s\n", "full key snapshot",
           (test->reported_errors & ERROR_KEY_SNAPSHOT) == 0 ? "consistent" : "INCONSISTENT");
    printf("  %-24s %s\n", "depressed modifiers",
           (test->reported_errors & ERROR_MODIFIER_STATE) == 0 ? "consistent" : "INCONSISTENT");
    printf("  %-24s %s\n", "lock modifiers",
           (test->reported_errors & ERROR_LOCK_MODIFIER_STATE) != 0 ? "INCONSISTENT" :
           test->lock_change_unreported == true ? "not reported by the platform" :
           (done[STEP_LOCK_KEYS] == true ? "toggled and restored" : "not exercised"));
    printf("  %-24s %s\n", "keyboard before char",
           (test->reported_errors & ERROR_CALLBACK_ORDER) == 0 ? "respected" : "VIOLATED");
    if (test->saw_control == true) {
        printf("  %-24s DELIVERED, first U+%04X\n", "control code points", test->first_control);
    }
    else {
        printf("  %-24s none delivered\n", "control code points");
    }
    if (test->non_text_key_valid == false) {
        printf("  %-24s not exercised\n", "non-text key characters");
    }
    else if (test->non_text_key_char == 0) {
        printf("  %-24s none delivered\n", "non-text key characters");
    }
    else {
        printf("  %-24s DELIVERED, %s gave U+%04X\n", "non-text key characters",
               key_name_or_unknown(test->non_text_key), test->non_text_key_char);
    }
    printf("  %-24s %u\n", "unknown key callbacks", test->unknown_key_callbacks);
    if (test->saw_extra_mods == true) {
        printf("  %-24s 0x%02x, first with %s\n", "modifiers with no key",
               (unsigned) test->first_extra_mods, key_name_or_unknown(test->first_extra_mods_key));
    }
    else {
        printf("  %-24s none\n", "modifiers with no key");
    }

#if TEST_DOS_PLATFORM
    printf("  %-24s n/a, DOS has no window focus\n", "focus state cleanup");
    printf("  %-24s n/a, DOS has no window focus\n", "focus release callback");
#else
    if (test->step_skipped[STEP_FOCUS] == true) {
        printf("  %-24s SKIPPED\n", "focus state cleanup");
        printf("  %-24s SKIPPED\n", "focus release callback");
    }
    else {
        printf("  %-24s %s\n", "focus state cleanup",
               test->focus_loss_seen == false ? "NOT DONE" :
               ((test->reported_errors & ERROR_FOCUS_STATE) != 0 ? "INCONSISTENT" :
                (test->focus_state_cleared == true ? "all keys released" : "NOT DONE")));
        printf("  %-24s %s\n", "focus release callback",
               test->focus_loss_seen == false ? "NOT DONE" :
               (test->focus_release_callback == true ? "delivered" : "not delivered"));
    }
#endif

    puts("");
    puts("OPTIONAL CAPABILITY SUMMARY");
    for (unsigned step = 0; step < STEP_COUNT; ++step) {
        if (g_steps[step].required == false) {
            printf("  %-24s %s\n", g_steps[step].label,
                   step_result(test, (TestStepId) step, done));
        }
    }
#if !TEST_DOS_PLATFORM
    if (test->altgr_presses == 0) {
        printf("  %-24s not exercised\n", "AltGr companion Control");
    }
    else {
        printf("  %-24s %s, reported on %u of %u\n", "AltGr companion Control",
               test->altgr_control_stuck == true ? "LEFT HELD" : "released with the Alt",
               test->altgr_companion_seen, test->altgr_presses);
    }
#endif
    if (test->saw_non_ascii == true) {
        printf("  %-24s first U+%04X\n", "non-ASCII text", test->first_non_ascii);
    }
    else {
        printf("  %-24s none\n", "non-ASCII text");
    }
    if (test->saw_supplementary == true) {
        printf("  %-24s first U+%04X\n", "supplementary scalar", test->first_supplementary);
    }
    if (test->saw_geometric_shape == true) {
        printf("  %-24s first U+%04X\n", "U+1F7xx scalar", test->first_geometric_shape);
    }
    if (test->cjk_count > 0) {
        printf("  %-24s %u scalar callback%s\n", "CJK scalars",
               test->cjk_count, test->cjk_count == 1 ? "" : "s");
    }
    printf("  %-24s %s\n", "composition on focus",
           test->composition_kept == true    ? "kept by the platform" :
           test->saw_dead_focus_clear == true ? "cleared" : "not exercised");
    if (test->press_count[MFB_KB_KEY_WORLD_1] > 0) {
        printf("  %-24s World_1\n", "ISO key token");
    }
    else if (test->press_count[MFB_KB_KEY_WORLD_2] > 0) {
        printf("  %-24s World_2\n", "ISO key token");
    }
    printf("  %-24s %u\n", "same-pump transitions", test->same_pump_transition_count);
    fflush(stdout);
}

static void
finish_test(KeyboardTest *test) {
    bool done[STEP_COUNT] = { false };
    unsigned skipped = 0;

    if (test->finalized == true) {
        return;
    }
    test->finalized = true;
    flush_repeats(test);
    evaluate_steps(test, done);

    for (unsigned step = 0; step < STEP_COUNT; ++step) {
        if (g_steps[step].required == false || test->step_skipped[step] == true) {
            continue;
        }
        if (done[step] == false && test->step_done[step] == false) {
            report_missing(test, g_steps[step].action);
        }
    }

    // The repeat step reports done as soon as the key repeats, so the release it asks for
    // has to be checked separately.
    if (test->step_skipped[STEP_REPEAT] == false &&
        test->press_count[MFB_KB_KEY_R] >= 2 &&
        test->release_count[MFB_KB_KEY_R] == 0) {
        report_missing(test, "Release R after it repeats.");
    }

    print_summary(test);

    for (unsigned step = 0; step < STEP_COUNT; ++step) {
        if (g_steps[step].required == true && test->step_skipped[step] == true) {
            skipped++;
        }
    }

    if (test->error_count > 0) {
        unsigned listed = test->error_count < TEST_MAX_LOGGED_ERRORS
                          ? test->error_count : TEST_MAX_LOGGED_ERRORS;

        printf("FAIL: %u error%s detected.\n",
               test->error_count, test->error_count == 1 ? "" : "s");
        for (unsigned i = 0; i < listed; ++i) {
            printf(" - ERROR: %s\n", test->error_log[i]);
        }
        if (test->error_count > listed) {
            printf(" - and %u more not kept.\n", test->error_count - listed);
        }
        guide("Result: FAIL, %u error%s detected.",
              test->error_count, test->error_count == 1 ? "" : "s");
    }
    else if (skipped > 0) {
        test->skipped_required = true;
        printf("INCOMPLETE: no errors, but %u required action%s skipped.\n",
               skipped, skipped == 1 ? " was" : "s were");
        guide("Result: INCOMPLETE, %u required action%s skipped.",
              skipped, skipped == 1 ? " was" : "s were");
    }
    else {
        puts("PASS: all required actions completed and no errors were detected.");
        guide("Result: PASS.");
    }
    fflush(stdout);
}

static void
print_instructions(void) {
    guide("MiniFB interactive keyboard event contract test");
    guide("");
    guide("The test asks for one action at a time. Do what the NEXT line says, or press Escape");
    guide("to skip a step you cannot perform. When none are left, it finishes the run.");
    guide("");
    guide("Before starting, release every key and turn Caps Lock off. Use a keyboard layout");
    guide("where A and R produce lowercase a and r.");
#if !defined(__EMSCRIPTEN__)
    guide("");
    guide("Steps and progress go to stderr, the event log and the summary go to stdout, so");
    guide("running it as \"keyboard_events_test > log.txt\" keeps this panel on screen.");
#endif
}

static void
fill_buffer(uint32_t *buffer) {
    for (unsigned y = 0; y < TEST_HEIGHT; ++y) {
        for (unsigned x = 0; x < TEST_WIDTH; ++x) {
            bool border = x < 8 || y < 8 ||
                          x >= TEST_WIDTH - 8 || y >= TEST_HEIGHT - 8;
            buffer[y * TEST_WIDTH + x] = border == true
                ? MFB_RGB(100, 170, 110)
                : MFB_RGB(18, 24, 32);
        }
    }
}

int
main(void) {
#if TEST_EXCLUDED_MOBILE_PLATFORM
    fputs("keyboard_events_test excludes Android and iOS by design\n", stderr);
    return EXIT_SUCCESS;
#else
    print_instructions();

    uint32_t *buffer =
        (uint32_t *) malloc(TEST_WIDTH * TEST_HEIGHT * sizeof(uint32_t));
    if (buffer == NULL) {
        fputs("ERROR: could not allocate the test framebuffer\n", stderr);
        return EXIT_FAILURE;
    }
    fill_buffer(buffer);

    mfb_set_log_level(MFB_LOG_WARNING);

    KeyboardTest test = { 0 };
    test.window = mfb_open_ex("MiniFB keyboard event contract test",
                              TEST_WIDTH, TEST_HEIGHT, 0);
    if (test.window == NULL) {
        fputs("ERROR: could not open the test window\n", stderr);
        free(buffer);
        return EXIT_FAILURE;
    }

    mfb_set_user_data(test.window, &test);
    test.expected_active = mfb_is_window_active(test.window);

    const uint8_t *initial_keys = mfb_get_key_buffer(test.window);
    if (initial_keys == NULL) {
        report_error(&test, ERROR_KEY_BUFFER_NULL,
                     "mfb_get_key_buffer returned NULL for a new window");
    }
    else {
        // The contract says the initial state mirrors the real keyboard, so a key held
        // while the window opens is expected rather than an error.
        for (unsigned key = 0; key < TEST_KEY_COUNT; ++key) {
            test.expected_keys[key] = initial_keys[key] != 0;
            if (test.expected_keys[key] == true) {
                printf("  INITIAL: %s is already pressed\n",
                       key_name_or_unknown((mfb_key) key));
            }
        }
    }

    mfb_set_active_callback(test.window, active);
    mfb_set_close_callback(test.window, window_close);
    mfb_set_keyboard_callback(test.window, keyboard);
    mfb_set_char_input_callback(test.window, char_input);
    mfb_set_mouse_button_callback(test.window, mouse_button);
    announce_next_step(&test);

    for (;;) {
        begin_pump(&test);
        mfb_update_state state = mfb_update(test.window, buffer);
        if (state != MFB_STATE_OK) {
            if (state != MFB_STATE_EXIT) {
                report_error(&test, ERROR_UPDATE,
                             "mfb_update returned unexpected state %d", state);
            }
            test.window = NULL;
            break;
        }
        check_after_pump(&test);

        begin_pump(&test);
        if (mfb_wait_sync(test.window) == false) {
            test.window = NULL;
            break;
        }
        check_after_pump(&test);
    }

    finish_test(&test);
    free(buffer);
    return test.error_count == 0 && test.skipped_required == false
         ? EXIT_SUCCESS
         : EXIT_FAILURE;
#endif
}
