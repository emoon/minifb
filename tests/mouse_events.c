#include <MiniFB.h>

#include <stdbool.h>
#include <stdarg.h>
#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(__APPLE__)
    #include <TargetConditionals.h>
#endif

#define TEST_WIDTH              640u
#define TEST_HEIGHT             480u
#define TEST_MOUSE_BUTTON_COUNT 8u

#define ERROR_BUTTON_BUFFER_NULL       (UINT64_C(1) << 0)
#define ERROR_BUTTON_RANGE             (UINT64_C(1) << 1)
#define ERROR_BUTTON_DUPLICATE         (UINT64_C(1) << 2)
#define ERROR_BUTTON_STATE             (UINT64_C(1) << 3)
#define ERROR_BUTTON_SNAPSHOT          (UINT64_C(1) << 4)
#define ERROR_MOVE_GETTER_X            (UINT64_C(1) << 5)
#define ERROR_MOVE_GETTER_Y            (UINT64_C(1) << 6)
#define ERROR_POINTER_ID               (UINT64_C(1) << 7)
#define ERROR_SCROLL_GETTER_X          (UINT64_C(1) << 8)
#define ERROR_SCROLL_GETTER_Y          (UINT64_C(1) << 9)
#define ERROR_SCROLL_ZERO              (UINT64_C(1) << 10)
#define ERROR_SCROLL_STALE             (UINT64_C(1) << 11)
#define ERROR_ENTER_DUPLICATE          (UINT64_C(1) << 12)
#define ERROR_ENTER_GETTER             (UINT64_C(1) << 13)
#define ERROR_LEAVE_WHILE_HELD         (UINT64_C(1) << 14)
#define ERROR_INSIDE_SNAPSHOT          (UINT64_C(1) << 15)
#define ERROR_UPDATE                   (UINT64_C(1) << 16)
#define ERROR_BUTTON_ORDER             (UINT64_C(1) << 17)
#define ERROR_SIDE_BUTTON_NUMBER       (UINT64_C(1) << 19)
#define ERROR_TOUCH_POINTER_ID         (UINT64_C(1) << 21)
#define ERROR_FINGER_USES_MOUSE_ID     (UINT64_C(1) << 22)

#define PROGRESS_POINTER               (UINT32_C(1) << 0)
#define PROGRESS_BUTTONS               (UINT32_C(1) << 1)
#define PROGRESS_SCROLL                (UINT32_C(1) << 2)
#define PROGRESS_DRAG                  (UINT32_C(1) << 3)
#define PROGRESS_READY                 (UINT32_C(1) << 4)
#define PROGRESS_LEFT_BUTTON           (UINT32_C(1) << 5)
#define PROGRESS_RIGHT_BUTTON          (UINT32_C(1) << 6)
#define PROGRESS_MIDDLE_BUTTON         (UINT32_C(1) << 7)
#define PROGRESS_SCROLL_UP             (UINT32_C(1) << 8)
#define PROGRESS_DOUBLE_CLICK          (UINT32_C(1) << 9)
#define PROGRESS_MULTITOUCH            (UINT32_C(1) << 10)

// Side buttons and a horizontal wheel are missing from plenty of mice, so they are checked
// when they show up and reported as unexercised when they do not. Requiring them would turn
// a common mouse into a failing run.
#define MFB_MAX_POINTER_ID_SEEN        15u
#define SIDE_BUTTON_FIRST              4u
#define SIDE_BUTTON_LAST               7u

// A wheel sends one event worth a whole notch. A precision trackpad sends a stream of
// fractions of it, and a deliberate swipe can add up to a fifth of a notch or less, so no
// single magnitude means "on purpose" on both. A gesture counts when it carries a notch, or
// when it is built from several events that agree on a direction.
#define SCROLL_NOTCH_UNITS             1.0f
#define SCROLL_MIN_EVENTS              3u

// A trackpad streams small deltas while the finger moves and stops when it lifts, so a run
// of pumps with no scroll at all is what separates one gesture from the next. Around a tenth
// of a second at the frame rates this test runs at.
#define SCROLL_GESTURE_IDLE_PUMPS      12u

#if defined(__ANDROID__) || (defined(TARGET_OS_IOS) && TARGET_OS_IOS)
    #define TEST_TOUCH_PLATFORM 1
#else
    #define TEST_TOUCH_PLATFORM 0
#endif

#if defined(__DJGPP__)
    #define TEST_DOS_PLATFORM 1
#else
    #define TEST_DOS_PLATFORM 0
#endif

#if defined(__ANDROID__)
#include <android/log.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

// An Android app has no terminal, so stdout and stderr are discarded. The test prints with
// the standard functions to keep one source for every platform, so both are piped into a
// thread that forwards each line to logcat.
static int g_log_pipe[2];

static void *
pump_output_to_log(void *unused) {
    char    line[512];
    size_t  used = 0;
    ssize_t count;

    (void) unused;
    while ((count = read(g_log_pipe[0], line + used, sizeof(line) - used - 1)) > 0) {
        used += (size_t) count;
        line[used] = '\0';

        char *start = line;
        char *end;
        while ((end = strchr(start, '\n')) != NULL) {
            *end = '\0';
            __android_log_write(ANDROID_LOG_INFO, "mouse_events_test", start);
            start = end + 1;
        }

        used = strlen(start);
        memmove(line, start, used);
    }

    return NULL;
}

static void
redirect_output_to_log(void) {
    pthread_t thread;

    setvbuf(stdout, NULL, _IOLBF, 0);
    if (pipe(g_log_pipe) != 0) {
        return;
    }

    dup2(g_log_pipe[1], STDOUT_FILENO);
    dup2(g_log_pipe[1], STDERR_FILENO);

    if (pthread_create(&thread, NULL, pump_output_to_log, NULL) == 0) {
        pthread_detach(thread);
    }
}
#endif

typedef struct {
    struct mfb_window *window;
    bool               expected_buttons[TEST_MOUSE_BUTTON_COUNT];
    bool               saw_button_press[TEST_MOUSE_BUTTON_COUNT];
    bool               saw_button_release[TEST_MOUSE_BUTTON_COUNT];
    bool               saw_move;
    bool               saw_enter;
    bool               saw_leave;
    bool               saw_reenter_after_leave;
    bool               saw_left_drag_outside;
    bool               saw_left_release_outside;
    bool               saw_leave_after_left_release;
    bool               saw_reenter_after_left_release;
    bool               expected_inside;
    bool               finalized;
    unsigned           main_button_stage;
    unsigned           vertical_scroll_stage;
    unsigned           scroll_callbacks_in_update;
    unsigned           notch_callbacks_in_update;
    unsigned           enter_count;
    unsigned           leave_count;
    unsigned           double_click_presses;
    unsigned           double_click_releases;
    unsigned           first_notch_callbacks;
    bool               first_notch_recorded;
    uint32_t           side_buttons_seen;
    uint32_t           pointer_ids_seen;
    unsigned           horizontal_scroll_stage;
    int                horizontal_first_sign;
    int                horizontal_second_sign;
    int                vertical_first_sign;
    int                vertical_second_sign;
    float              horizontal_total;
    float              vertical_total;
    unsigned           idle_scroll_pumps;
    unsigned           horizontal_events;
    unsigned           vertical_events;
    unsigned           simultaneous_pointers;
    unsigned           max_simultaneous_pointers;
    unsigned           error_count;
    uint32_t           reported_progress;
    uint64_t           reported_errors;
} MouseTest;

static void
report_error(MouseTest *test, uint64_t error, const char *format, ...) {
    if ((test->reported_errors & error) != 0) {
        return;
    }

    test->reported_errors |= error;
    test->error_count++;

    va_list arguments;
    va_start(arguments, format);
    fputs("ERROR: ", stdout);
    vprintf(format, arguments);
    fputc('\n', stdout);
    fflush(stdout);
    va_end(arguments);
}

static void
report_missing(MouseTest *test, const char *action) {
    test->error_count++;
    printf("ERROR: required action was not observed: %s\n", action);
    fflush(stdout);
}

static void
report_progress(MouseTest *test, uint32_t progress, const char *description) {
    if ((test->reported_progress & progress) != 0) {
        return;
    }

    test->reported_progress |= progress;
    printf("DONE: %s\n", description);
    fflush(stdout);
}

static bool
any_button_pressed(const MouseTest *test) {
    for (unsigned button = 0; button < TEST_MOUSE_BUTTON_COUNT; ++button) {
        if (test->expected_buttons[button] == true) {
            return true;
        }
    }

    return false;
}

static bool
required_mouse_buttons_done(const MouseTest *test) {
    return test->main_button_stage >= 3;
}

static const char *
main_button_name(unsigned button) {
    switch (button) {
        case MFB_MOUSE_LEFT:
            return "left (MFB_MOUSE_BTN_1)";
        case MFB_MOUSE_RIGHT:
            return "right (MFB_MOUSE_BTN_2)";
        case MFB_MOUSE_MIDDLE:
            return "middle (MFB_MOUSE_BTN_3)";
        default:
            return "unknown";
    }
}

static void
update_main_button_progress(MouseTest *test) {
    if (test->main_button_stage >= 1) {
        report_progress(test, PROGRESS_LEFT_BUTTON,
                        "left mouse button reported as MFB_MOUSE_BTN_1");
    }
    if (test->main_button_stage >= 2) {
        report_progress(test, PROGRESS_RIGHT_BUTTON,
                        "right mouse button reported as MFB_MOUSE_BTN_2");
    }
    if (test->main_button_stage >= 3) {
        report_progress(test, PROGRESS_MIDDLE_BUTTON,
                        "middle mouse button reported as MFB_MOUSE_BTN_3");
    }
}

static bool
any_pointer_press_and_release_done(const MouseTest *test) {
    for (unsigned button = 0; button < TEST_MOUSE_BUTTON_COUNT; ++button) {
        if (test->saw_button_press[button] == true &&
            test->saw_button_release[button] == true) {
            return true;
        }
    }

    return false;
}

static void
update_progress(MouseTest *test) {
#if TEST_TOUCH_PLATFORM
    if (test->saw_move == true && any_pointer_press_and_release_done(test) == true) {
        report_progress(test, PROGRESS_POINTER, "touch press, movement, and release observed");
    }
    if (test->max_simultaneous_pointers >= 2) {
        report_progress(test, PROGRESS_MULTITOUCH, "two contacts reported at the same time");
    }
    uint32_t required_progress = PROGRESS_POINTER | PROGRESS_MULTITOUCH;
#elif TEST_DOS_PLATFORM
    if (test->saw_move == true) {
        report_progress(test, PROGRESS_POINTER, "pointer movement observed");
    }
    update_main_button_progress(test);
    if (required_mouse_buttons_done(test) == true) {
        report_progress(test, PROGRESS_BUTTONS,
                        "left, right, and middle button sequence completed");
    }
    if (test->double_click_presses >= 2 && test->double_click_releases >= 2) {
        report_progress(test, PROGRESS_DOUBLE_CLICK,
                        "double click reported as two press and release pairs");
    }
    uint32_t required_progress = PROGRESS_POINTER | PROGRESS_BUTTONS | PROGRESS_DOUBLE_CLICK;
#else
    if (test->saw_move == true && test->saw_enter == true &&
        test->saw_leave == true && test->saw_reenter_after_leave == true) {
        report_progress(test, PROGRESS_POINTER, "pointer entry, movement, exit, and re-entry observed");
    }
    update_main_button_progress(test);
    if (required_mouse_buttons_done(test) == true) {
        report_progress(test, PROGRESS_BUTTONS,
                        "left, right, and middle button sequence completed");
    }
    if (test->vertical_scroll_stage >= 1) {
        report_progress(test, PROGRESS_SCROLL_UP, "first vertical scroll recorded");
    }
    if (test->vertical_scroll_stage >= 2) {
        report_progress(test, PROGRESS_SCROLL, "opposite vertical scroll recorded");
    }
    if (test->saw_left_release_outside == true &&
        test->saw_leave_after_left_release == true &&
        test->saw_reenter_after_left_release == true) {
        report_progress(test, PROGRESS_DRAG, "outside drag, release, leave, and re-entry observed");
    }
    if (test->double_click_presses >= 2 && test->double_click_releases >= 2) {
        report_progress(test, PROGRESS_DOUBLE_CLICK,
                        "double click reported as two press and release pairs");
    }
    uint32_t required_progress = PROGRESS_POINTER | PROGRESS_BUTTONS |
                                 PROGRESS_SCROLL | PROGRESS_DRAG | PROGRESS_DOUBLE_CLICK;
#endif

    if ((test->reported_progress & required_progress) == required_progress) {
        report_progress(test, PROGRESS_READY,
                        "all required actions observed; close the window to finish");
    }
}

static bool
point_is_outside(struct mfb_window *window, int packed_x, int packed_y) {
    int x = mfb_decode_touch_pos(packed_x);
    int y = mfb_decode_touch_pos(packed_y);
    unsigned width = mfb_get_window_width(window);
    unsigned height = mfb_get_window_height(window);

    return x < 0 || y < 0 || x >= (int) width || y >= (int) height;
}

static void
check_pointer_ids(MouseTest *test, int packed_x, int packed_y) {
    int id_x = mfb_decode_touch_id(packed_x);
    int id_y = mfb_decode_touch_id(packed_y);

    if (id_x != id_y) {
        report_error(test, ERROR_POINTER_ID,
                     "the X and Y values identify different pointers (%d and %d)",
                     id_x, id_y);
    }

    if (id_x >= 0 && id_x <= (int) MFB_MAX_POINTER_ID_SEEN) {
        test->pointer_ids_seen |= UINT32_C(1) << id_x;
    }

#if TEST_TOUCH_PLATFORM
    // A finger has to keep its own id: MFB_POINTER_ID_MOUSE is reserved for a mouse, a
    // trackpad or a hovering stylus, and telling them apart depends on it.
    if (id_x == MFB_POINTER_ID_MOUSE) {
        report_error(test, ERROR_FINGER_USES_MOUSE_ID,
                     "a touch reported MFB_POINTER_ID_MOUSE (%d) instead of its own pointer id",
                     MFB_POINTER_ID_MOUSE);
    }
#endif
}

static void
check_button_snapshot(MouseTest *test) {
    const uint8_t *buttons = mfb_get_mouse_button_buffer(test->window);
    if (buttons == NULL) {
        report_error(test, ERROR_BUTTON_BUFFER_NULL,
                     "mfb_get_mouse_button_buffer returned NULL for a valid window");
        return;
    }

    for (unsigned button = 0; button < TEST_MOUSE_BUTTON_COUNT; ++button) {
        bool actual = buttons[button] != 0;
        if (actual != test->expected_buttons[button]) {
            report_error(test, ERROR_BUTTON_SNAPSHOT,
                         "button %u snapshot is %d, but the callback history says it should be %d",
                         button, actual, test->expected_buttons[button]);
            return;
        }
    }
}

static MouseTest *
get_test(struct mfb_window *window) {
    return (MouseTest *) mfb_get_user_data(window);
}

static void
mouse_button(struct mfb_window *window, mfb_mouse_button button, mfb_key_mod mod, bool is_pressed) {
    (void) mod;

    MouseTest *test = get_test(window);
    int button_index = (int) button;
    int packed_x = mfb_get_mouse_x(window);
    int packed_y = mfb_get_mouse_y(window);

    check_pointer_ids(test, packed_x, packed_y);

    //printf("Mouse button: %d\n", button_index);

    if (button_index < 0 || button_index >= (int) TEST_MOUSE_BUTTON_COUNT) {
        report_error(test, ERROR_BUTTON_RANGE,
                     "mouse button callback reported out-of-range button %d", button_index);
        return;
    }

    if (test->expected_buttons[button_index] == is_pressed) {
        report_error(test, ERROR_BUTTON_DUPLICATE,
                     "button %d was reported %s twice without the opposite transition",
                     button_index, is_pressed == true ? "pressed" : "released");
    }

#if !TEST_TOUCH_PLATFORM
    bool valid_transition = test->expected_buttons[button_index] != is_pressed;
#endif
    test->expected_buttons[button_index] = is_pressed;
    if (is_pressed == true) {
        test->saw_button_press[button_index] = true;
        test->simultaneous_pointers++;
        if (test->simultaneous_pointers > test->max_simultaneous_pointers) {
            test->max_simultaneous_pointers = test->simultaneous_pointers;
        }
    }
    else {
        test->saw_button_release[button_index] = true;
        if (test->simultaneous_pointers > 0) {
            test->simultaneous_pointers--;
        }
    }

    // Which of the two is back and which is forward cannot be told apart from here, and it
    // does not matter: the contract is the pair of numbers they use.
    if (button_index >= (int) SIDE_BUTTON_FIRST && button_index <= (int) SIDE_BUTTON_LAST) {
        test->side_buttons_seen |= UINT32_C(1) << button_index;
        if (button_index > (int) MFB_MOUSE_BTN_5) {
            report_error(test, ERROR_SIDE_BUTTON_NUMBER,
                         "side button reported as %d; the other backends report 4 and 5",
                         button_index);
        }
    }

#if !TEST_TOUCH_PLATFORM
    if (button_index == MFB_MOUSE_LEFT && required_mouse_buttons_done(test) == true &&
        valid_transition == true) {
        if (is_pressed == true) {
            test->double_click_presses++;
        }
        else {
            test->double_click_releases++;
        }
    }
#endif

#if !TEST_TOUCH_PLATFORM
    if (button_index >= MFB_MOUSE_LEFT && button_index <= MFB_MOUSE_MIDDLE &&
        test->main_button_stage < 3) {
        unsigned expected_button = MFB_MOUSE_LEFT + test->main_button_stage;
        if ((unsigned) button_index > expected_button) {
            report_error(test, ERROR_BUTTON_ORDER,
                         "button sequence expected %s but received %s",
                         main_button_name(expected_button),
                         main_button_name((unsigned) button_index));
        }
        else if ((unsigned) button_index == expected_button &&
                 is_pressed == false && valid_transition == true) {
            test->main_button_stage++;
        }
    }
#endif

    const uint8_t *buttons = mfb_get_mouse_button_buffer(window);
    if (buttons == NULL) {
        report_error(test, ERROR_BUTTON_BUFFER_NULL,
                     "mfb_get_mouse_button_buffer returned NULL inside a button callback");
    }
    else if ((buttons[button_index] != 0) != is_pressed) {
        report_error(test, ERROR_BUTTON_STATE,
                     "button %d getter state disagrees with its %s callback",
                     button_index, is_pressed == true ? "press" : "release");
    }

    if (button == MFB_MOUSE_LEFT && is_pressed == false &&
        test->saw_left_drag_outside == true &&
        point_is_outside(window, packed_x, packed_y) == true) {
        test->saw_left_release_outside = true;
    }

    update_progress(test);
}

static void
mouse_move(struct mfb_window *window, int x, int y) {
    MouseTest *test = get_test(window);
    int getter_x = mfb_get_mouse_x(window);
    int getter_y = mfb_get_mouse_y(window);

    test->saw_move = true;

    if (getter_x != x) {
        report_error(test, ERROR_MOVE_GETTER_X,
                     "mouse X getter is %d inside a move callback that reports %d",
                     getter_x, x);
    }
    if (getter_y != y) {
        report_error(test, ERROR_MOVE_GETTER_Y,
                     "mouse Y getter is %d inside a move callback that reports %d",
                     getter_y, y);
    }

    check_pointer_ids(test, x, y);

    if (test->expected_buttons[MFB_MOUSE_LEFT] == true &&
        point_is_outside(window, x, y) == true) {
        test->saw_left_drag_outside = true;
    }

    check_button_snapshot(test);
    update_progress(test);
}


static void
mouse_scroll(struct mfb_window *window, mfb_key_mod mod, float delta_x, float delta_y) {
    (void) mod;

    MouseTest *test = get_test(window);
    float getter_x = mfb_get_mouse_scroll_x(window);
    float getter_y = mfb_get_mouse_scroll_y(window);

    if (delta_x != 0.0f) {
        //printf("delta X: %f == %f\n", delta_x, getter_x);
    }
    if (delta_y != 0.0f) {
        //printf("delta Y: %f == %f\n", delta_y, getter_y);
    }
    test->scroll_callbacks_in_update++;
    if (fabsf(delta_x) >= 0.9f || fabsf(delta_y) >= 0.9f) {
        test->notch_callbacks_in_update++;
    }

    // The getters describe the whole pump. An axis that is zero in this callback may
    // still contain a value from an earlier event in the same pump.
    if (delta_x != 0.0f && getter_x != delta_x) {
        report_error(test, ERROR_SCROLL_GETTER_X,
                     "scroll X getter is %.6g inside a callback that reports %.6g",
                     getter_x, delta_x);
    }
    if (delta_y != 0.0f && getter_y != delta_y) {
        report_error(test, ERROR_SCROLL_GETTER_Y,
                     "scroll Y getter is %.6g inside a callback that reports %.6g",
                     getter_y, delta_y);
    }
    if (delta_x == 0.0f && delta_y == 0.0f) {
        report_error(test, ERROR_SCROLL_ZERO,
                     "scroll callback reported zero on both axes");
    }

    // Both axes are added up for the whole gesture. Judging a single delta would read the
    // drift of a vertical swipe as a horizontal one, and judging a running total that never
    // resets lets an up gesture and a down gesture cancel each other out.
    test->horizontal_total += delta_x;
    test->vertical_total += delta_y;
    if (delta_x != 0.0f) {
        test->horizontal_events++;
    }
    if (delta_y != 0.0f) {
        test->vertical_events++;
    }
    test->idle_scroll_pumps = 0;

    check_button_snapshot(test);
    update_progress(test);
}

// The gesture ends when the scroll events stop. Its net displacement decides which axis it
// was and which way it went, so jitter on the other axis and a slow start cannot change the
// verdict. Anything that does not add up to a notch was not a deliberate scroll.
static bool
gesture_was_deliberate(float total, unsigned events) {
    return fabsf(total) >= SCROLL_NOTCH_UNITS || (events >= SCROLL_MIN_EVENTS && total != 0.0f);
}

// Which way the fingers or the wheel actually moved is unknowable from here, and the system
// scroll setting can flip it, so no absolute sign is asserted. What is checked is that two
// opposite gestures report opposite signs; the signs themselves are recorded for the summary,
// where comparing two backends is what gives them meaning.
static void
finish_scroll_gesture(MouseTest *test) {
    float    horizontal        = test->horizontal_total;
    float    vertical          = test->vertical_total;
    unsigned horizontal_events = test->horizontal_events;
    unsigned vertical_events   = test->vertical_events;

    test->horizontal_total  = 0.0f;
    test->vertical_total    = 0.0f;
    test->horizontal_events = 0;
    test->vertical_events   = 0;

    bool is_horizontal = fabsf(horizontal) > fabsf(vertical);
    float total        = is_horizontal == true ? horizontal : vertical;
    unsigned events    = is_horizontal == true ? horizontal_events : vertical_events;

    if (gesture_was_deliberate(total, events) == false) {
        return;
    }

    int sign = total > 0.0f ? 1 : -1;
    printf("GESTURE: %s net=%+.4f events=%u sign=%c%c\n",
           is_horizontal == true ? "horizontal" : "vertical  ",
           total, events, sign > 0 ? '+' : '-', is_horizontal == true ? 'X' : 'Y');
    fflush(stdout);

    // Both axes advance the same way: the first gesture sets the sign, and the next one in
    // the opposite direction completes the pair. Same direction again is just more of the
    // gesture that already counted.
    unsigned *stage      = is_horizontal == true ? &test->horizontal_scroll_stage : &test->vertical_scroll_stage;
    int      *first_sign = is_horizontal == true ? &test->horizontal_first_sign   : &test->vertical_first_sign;
    int      *second_sign = is_horizontal == true ? &test->horizontal_second_sign : &test->vertical_second_sign;

    if (*stage == 0) {
        *first_sign = sign;
        *stage = 1;
    }
    else if (*stage == 1) {
        if (sign == *first_sign) {
            return;
        }
        *second_sign = sign;
        *stage = 2;
    }

    update_progress(test);
}

// A wheel notch arrives as one callback worth a whole notch, and a backend that forwards
// both halves of the platform event sends that same value twice. Only callbacks carrying a
// full notch are counted, so a trackpad, which spreads a gesture over dozens of fractions,
// leaves this unmeasured instead of reporting a number that means something else.
//
// Two notches rolled quickly look exactly like one notch reported twice, which is why the
// instructions ask for a single notch first. The number is meant to be compared between
// backends, not read on its own.
static void
record_first_notch(MouseTest *test) {
    if (test->first_notch_recorded == false && test->notch_callbacks_in_update > 0) {
        test->first_notch_recorded = true;
        test->first_notch_callbacks = test->notch_callbacks_in_update;
    }
}

static void
mouse_enter(struct mfb_window *window, bool is_inside) {
    MouseTest *test = get_test(window);
    bool getter_inside = mfb_is_mouse_inside(window);

    if (getter_inside != is_inside) {
        report_error(test, ERROR_ENTER_GETTER,
                     "mfb_is_mouse_inside is %d inside an enter callback that reports %d",
                     getter_inside, is_inside);
    }
    if (test->expected_inside == is_inside) {
        report_error(test, ERROR_ENTER_DUPLICATE,
                     "mouse enter callback reported %s twice without the opposite transition",
                     is_inside == true ? "inside" : "outside");
    }
    if (is_inside == false && any_button_pressed(test) == true) {
        report_error(test, ERROR_LEAVE_WHILE_HELD,
                     "mouse left the window while at least one button was still held");
    }

    printf("CROSSING: %s (%u enter / %u leave so far)\n",
           is_inside == true ? "enter" : "leave",
           test->enter_count + (is_inside == true ? 1u : 0u),
           test->leave_count + (is_inside == true ? 0u : 1u));
    fflush(stdout);

    test->expected_inside = is_inside;
    if (is_inside == true) {
        test->saw_enter = true;
        test->enter_count++;
        if (test->saw_leave == true) {
            test->saw_reenter_after_leave = true;
        }
        if (test->saw_leave_after_left_release == true) {
            test->saw_reenter_after_left_release = true;
        }
    }
    else {
        test->saw_leave = true;
        test->leave_count++;
        if (test->saw_left_release_outside == true) {
            test->saw_leave_after_left_release = true;
        }
    }

    check_button_snapshot(test);
    update_progress(test);
}

static void
check_after_update(MouseTest *test) {
    check_button_snapshot(test);
    record_first_notch(test);

    if (test->scroll_callbacks_in_update > 0) {
        test->idle_scroll_pumps = 0;
    }
    else if (test->horizontal_total != 0.0f || test->vertical_total != 0.0f) {
        test->idle_scroll_pumps++;
        if (test->idle_scroll_pumps >= SCROLL_GESTURE_IDLE_PUMPS) {
            test->idle_scroll_pumps = 0;
            finish_scroll_gesture(test);
        }
    }

    bool inside = mfb_is_mouse_inside(test->window);
    if (inside != test->expected_inside) {
        report_error(test, ERROR_INSIDE_SNAPSHOT,
                     "mouse-inside getter changed from %d to %d without a callback",
                     test->expected_inside, inside);
    }

    if (test->scroll_callbacks_in_update == 0 &&
        (mfb_get_mouse_scroll_x(test->window) != 0.0f ||
         mfb_get_mouse_scroll_y(test->window) != 0.0f)) {
        report_error(test, ERROR_SCROLL_STALE,
                     "scroll getters were not reset in an event pump with no scroll callback");
    }
}

// One line per contract point, in a fixed order and wording, so two runs on two backends can
// be compared with diff. Anything the hardware could not produce says so out loud: two runs
// that both skip a line agree about nothing.
static void
print_summary(const MouseTest *test) {
    unsigned skipped = 0;

    puts("");
    puts("CONTRACT SUMMARY");

#if TEST_DOS_PLATFORM
    printf("  left button ............. %d\n", MFB_MOUSE_LEFT);
    printf("  right button ............ %d\n", MFB_MOUSE_RIGHT);
    printf("  middle button ........... %d\n", MFB_MOUSE_MIDDLE);
    printf("  double click ............ %u press / %u release\n",
           test->double_click_presses, test->double_click_releases);
    printf("  mouse inside ............ %s\n",
           mfb_is_mouse_inside(test->window) == true ? "true" : "false");
    puts("  window crossings ........ n/a (DOS has no window to enter or leave)");
    puts("  drag outside window ..... n/a (DOS has no window boundary)");
#if defined(MINIFB_DOS_WHEEL_FROM_ARROW_KEYS)
    printf("  vertical scroll ......... %s\n",
           test->vertical_scroll_stage >= 2 ? "both directions" : "NOT DONE");
#else
    puts("  vertical scroll ......... n/a (needs MINIFB_DOS_WHEEL_FROM_ARROW_KEYS)");
#endif
#elif TEST_TOUCH_PLATFORM
    printf("  simultaneous contacts ... %u\n", test->max_simultaneous_pointers);
    printf("  touch pointer ids .......");
    for (unsigned id = 0; id <= MFB_MAX_POINTER_ID_SEEN; ++id) {
        if ((test->pointer_ids_seen & (UINT32_C(1) << id)) != 0) {
            printf(" %u", id);
        }
    }
    printf("\n");
    printf("  mouse id used by a finger %s\n",
           (test->reported_errors & ERROR_FINGER_USES_MOUSE_ID) == 0 ? "no" : "YES");
#else
    printf("  left button ............. %d\n", MFB_MOUSE_LEFT);
    printf("  right button ............ %d\n", MFB_MOUSE_RIGHT);
    printf("  middle button ........... %d\n", MFB_MOUSE_MIDDLE);

    if (test->side_buttons_seen == 0) {
        puts("  side buttons ............ NOT EXERCISED (needs a mouse with 4 or more buttons)");
        skipped++;
    }
    else {
        printf("  side buttons ............");
        for (unsigned button = SIDE_BUTTON_FIRST; button <= SIDE_BUTTON_LAST; ++button) {
            if ((test->side_buttons_seen & (UINT32_C(1) << button)) != 0) {
                printf(" %u", button);
            }
        }
        printf("\n");
    }

    if (test->vertical_scroll_stage == 0) {
        puts("  vertical scroll ......... NOT DONE");
    }
    else if (test->vertical_scroll_stage == 1) {
        printf("  vertical scroll ......... first %cY, second one missing\n",
               test->vertical_first_sign > 0 ? '+' : '-');
    }
    else {
        printf("  vertical scroll ......... first %cY, then %cY\n",
               test->vertical_first_sign > 0 ? '+' : '-',
               test->vertical_second_sign > 0 ? '+' : '-');
    }

    if (test->horizontal_scroll_stage == 0) {
        puts("  horizontal scroll ....... NOT EXERCISED (needs a tilt wheel or a trackpad)");
        skipped++;
    }
    else if (test->horizontal_scroll_stage == 1) {
        printf("  horizontal scroll ....... first %cX, second one missing\n",
               test->horizontal_first_sign > 0 ? '+' : '-');
    }
    else {
        printf("  horizontal scroll ....... first %cX, then %cX\n",
               test->horizontal_first_sign > 0 ? '+' : '-',
               test->horizontal_second_sign > 0 ? '+' : '-');
    }

    if (test->first_notch_recorded == false) {
        puts("  callbacks per notch ..... NOT EXERCISED (needs a wheel with discrete notches)");
        skipped++;
    }
    else {
        printf("  callbacks per notch ..... %u\n", test->first_notch_callbacks);
    }

    printf("  double click ............ %u press / %u release\n",
           test->double_click_presses, test->double_click_releases);
    printf("  leave during drag ....... %s\n",
           test->saw_left_drag_outside == true
               ? ((test->reported_errors & ERROR_LEAVE_WHILE_HELD) == 0
                      ? "no leave until release"
                      : "leave while held")
               : "NOT DONE");
    printf("  release outside window .. %s\n",
           test->saw_left_release_outside == true ? "delivered" : "NOT DONE");
#endif

#if !TEST_DOS_PLATFORM
    printf("  enter/leave ............. %u enter / %u leave%s\n",
           test->enter_count, test->leave_count,
           (test->reported_errors & ERROR_ENTER_DUPLICATE) == 0 ? "" : " (unpaired)");
#endif

    if (skipped > 0) {
        printf("\n%u contract point%s could not be exercised on this hardware, so comparing"
               " this run against another backend proves nothing about %s. Lines marked NOT"
               " DONE are steps that were skipped, not hardware limits.\n",
               skipped, skipped == 1 ? "" : "s", skipped == 1 ? "it" : "them");
    }

    if (test->horizontal_scroll_stage > 0 || test->vertical_scroll_stage > 0) {
        puts("");
        puts("The scroll signs above belong to this device and these system settings. They are"
             " comparable across backends on the same machine, not across machines.");
    }
    fflush(stdout);
}

static void
finish_test(MouseTest *test) {
    if (test->finalized == true) {
        return;
    }
    test->finalized = true;

    if (test->saw_move == false) {
        report_missing(test, "move the pointer inside the window");
    }

#if TEST_TOUCH_PLATFORM
    bool saw_press = false;
    bool saw_release = false;
    for (unsigned button = 0; button < TEST_MOUSE_BUTTON_COUNT; ++button) {
        saw_press = saw_press || test->saw_button_press[button];
        saw_release = saw_release || test->saw_button_release[button];
    }
    if (saw_press == false) {
        report_missing(test, "touch the window");
    }
    if (saw_release == false) {
        report_missing(test, "lift the finger after a touch");
    }
    if (test->max_simultaneous_pointers < 2) {
        report_missing(test, "touch with two fingers at the same time");
    }
#else
    if (required_mouse_buttons_done(test) == false) {
        unsigned expected_button = MFB_MOUSE_LEFT + test->main_button_stage;
        char action[96];
        snprintf(action, sizeof(action), "click %s next", main_button_name(expected_button));
        report_missing(test, action);
    }

    #if !TEST_DOS_PLATFORM
        if (test->saw_enter == false) {
            report_missing(test, "move the pointer into the window");
        }
        if (test->saw_leave == false) {
            report_missing(test, "move the pointer out of the window");
        }
        if (test->saw_reenter_after_leave == false) {
            report_missing(test, "re-enter the window after leaving it");
        }
        if (test->vertical_scroll_stage == 0) {
            report_missing(test, "scroll vertically");
        }
        else if (test->vertical_scroll_stage == 1) {
            report_missing(test, "scroll vertically the opposite way");
        }
        if (test->saw_left_release_outside == false) {
            report_missing(test, "hold the left button, drag outside, and release outside");
        }
        else if (test->saw_leave_after_left_release == false) {
            report_missing(test, "receive a leave event after releasing the left button outside");
        }
        else if (test->saw_reenter_after_left_release == false) {
            report_missing(test, "re-enter the window after releasing the left button outside");
        }
        if (test->double_click_presses < 2 || test->double_click_releases < 2) {
            report_missing(test, "double click the left button");
        }
    #endif
#endif

    print_summary(test);

    if (test->error_count == 0) {
        puts("PASS: all required actions completed and no errors were detected.");
        fflush(stdout);
    }
    else {
        printf("FAIL: %u error%s detected.\n",
                test->error_count, test->error_count == 1 ? "" : "s");
        fflush(stdout);
    }
}

static bool
window_close(struct mfb_window *window) {
    (void) window;
    return true;
}

static void
keyboard(struct mfb_window *window, mfb_key key, mfb_key_mod mod, bool is_pressed) {
    (void) mod;

    if (key == MFB_KB_KEY_ESCAPE && is_pressed == false) {
        mfb_close(window);
    }
}

static void
print_instructions(void) {
    puts("MiniFB interactive mouse event contract test");
    puts("");
    puts("Perform these actions in the test window:");

#if TEST_TOUCH_PLATFORM
    puts("  1. Touch the window, drag the finger, and lift it.");
    puts("  2. Touch with two fingers at the same time.");
    puts("  3. Close the application when finished.");
#elif TEST_DOS_PLATFORM
    puts("  1. Move the pointer around the screen.");
    puts("  2. Click left, then right, then middle. Wait for DONE after each click.");
    puts("  3. Double click the left button.");
    puts("  4. Press Escape when finished.");
#else
    puts("  1. Move the pointer into the window, move it around, leave, and re-enter.");
    puts("  2. Click left, then right, then middle. Wait for DONE after each click.");
    puts("     Expected MiniFB values are 1, 2, and 3, in that order.");
    puts("  3. Scroll one way, pause until DONE appears, then scroll the opposite way.");
    puts("     A wheel or a trackpad both work. The two must report opposite signs; which");
    puts("     one is positive is recorded in the summary, not judged here.");
    puts("     With a wheel, make the very first movement a SINGLE notch: that is what");
    puts("     the callbacks per notch line measures, and two quick notches read the same");
    puts("     as one notch reported twice.");
    puts("  4. Hold the left button, drag outside, release outside, then re-enter.");
    puts("  5. Double click the left button.");
    puts("  6. If your mouse or trackpad can scroll sideways, do it one way, pause, then");
    puts("     the opposite way. Optional, and the summary says if it was skipped.");
    puts("     If it has back and forward buttons, press them now.");
    puts("     Both are optional: the summary says which ones were skipped.");
    puts("  7. Press Escape or close the window when finished.");
#endif

    puts("");
    puts("Scroll signs depend on the device and on the system scroll settings, not only on");
    puts("the backend: a trackpad and a tilt wheel report opposite signs for what feels like");
    puts("the same direction. Compare runs made on the same machine, with the same device");
    puts("and the same settings, or the comparison measures the hardware instead.");
    puts("");
    puts("Progress is printed once per completed step. Errors are printed when detected.");
    fflush(stdout);
}

static void
fill_buffer(uint32_t *buffer) {
    for (unsigned y = 0; y < TEST_HEIGHT; ++y) {
        for (unsigned x = 0; x < TEST_WIDTH; ++x) {
            bool border = x < 8 || y < 8 || x >= TEST_WIDTH - 8 || y >= TEST_HEIGHT - 8;
            buffer[y * TEST_WIDTH + x] = border == true
                ? MFB_RGB(90, 140, 220)
                : MFB_RGB(18, 24, 32);
        }
    }
}

int
main(void) {
#if defined(__ANDROID__)
    redirect_output_to_log();
#endif

    print_instructions();

    uint32_t *buffer = (uint32_t *) malloc(TEST_WIDTH * TEST_HEIGHT * sizeof(uint32_t));
    if (buffer == NULL) {
        fputs("ERROR: could not allocate the test framebuffer\n", stdout);
        return EXIT_FAILURE;
    }
    fill_buffer(buffer);

    mfb_set_log_level(MFB_LOG_WARNING);

    MouseTest test = { 0 };
    test.window = mfb_open_ex("MiniFB mouse event contract test",
                              TEST_WIDTH, TEST_HEIGHT, MFB_WF_RESIZABLE);
    if (test.window == NULL) {
        fputs("ERROR: could not open the test window\n", stdout);
        free(buffer);
        return EXIT_FAILURE;
    }

    mfb_set_user_data(test.window, &test);
    test.expected_inside = mfb_is_mouse_inside(test.window);

    const uint8_t *initial_buttons = mfb_get_mouse_button_buffer(test.window);
    if (initial_buttons == NULL) {
        report_error(&test, ERROR_BUTTON_BUFFER_NULL,
                     "mfb_get_mouse_button_buffer returned NULL for a new window");
    }
    else {
        for (unsigned button = 0; button < TEST_MOUSE_BUTTON_COUNT; ++button) {
            test.expected_buttons[button] = initial_buttons[button] != 0;
        }
    }

    mfb_set_close_callback(test.window, window_close);
    mfb_set_keyboard_callback(test.window, keyboard);
    mfb_set_mouse_button_callback(test.window, mouse_button);
    mfb_set_mouse_move_callback(test.window, mouse_move);
    mfb_set_mouse_scroll_callback(test.window, mouse_scroll);
    mfb_set_mouse_enter_callback(test.window, mouse_enter);
    mfb_set_target_fps(60);

    for (;;) {
        // Both calls pump events, so each one gets its own counting window. Sharing one
        // would let a scroll delivered by mfb_wait_sync be checked against the next frame.
        test.scroll_callbacks_in_update = 0;
        test.notch_callbacks_in_update = 0;
        mfb_update_state state = mfb_update(test.window, buffer);
        if (state != MFB_STATE_OK) {
            if (state != MFB_STATE_EXIT) {
                report_error(&test, ERROR_UPDATE,
                             "mfb_update returned unexpected state %d", state);
            }
            test.window = NULL;
            break;
        }
        check_after_update(&test);

        test.scroll_callbacks_in_update = 0;
        test.notch_callbacks_in_update = 0;
        if (mfb_wait_sync(test.window) == false) {
            test.window = NULL;
            break;
        }
        check_after_update(&test);
    }

    finish_test(&test);
    free(buffer);
    return test.error_count == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
