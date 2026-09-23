#include <MiniFB.h>

#include <stdbool.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define TEST_WIDTH              640u
#define TEST_HEIGHT             480u
#define TEST_FPS                60u

#define AUX_WIDTH               160u
#define AUX_HEIGHT              120u

#define SKIP_KEY                MFB_KB_KEY_ESCAPE
#define CONFIRM_KEY             MFB_KB_KEY_SPACE

#define ERROR_ACTIVE_GETTER            (UINT64_C(1) << 0)
#define ERROR_ACTIVE_DUPLICATE         (UINT64_C(1) << 1)
#define ERROR_ACTIVE_SNAPSHOT          (UINT64_C(1) << 2)
#define ERROR_RESIZE_SAME_SIZE         (UINT64_C(1) << 3)
#define ERROR_RESIZE_EMPTY             (UINT64_C(1) << 4)
#define ERROR_RESIZE_GETTER            (UINT64_C(1) << 5)
#define ERROR_DRAWABLE_OUTSIDE         (UINT64_C(1) << 6)
#define ERROR_SIZE_SNAPSHOT            (UINT64_C(1) << 7)
#define ERROR_CLOSE_REFUSAL_IGNORED    (UINT64_C(1) << 8)
#define ERROR_CLOSE_ACCEPT_IGNORED     (UINT64_C(1) << 9)
#define ERROR_CLOSE_AFTER_MFB_CLOSE    (UINT64_C(1) << 10)
#define ERROR_MFB_CLOSE_IGNORED        (UINT64_C(1) << 11)
#define ERROR_UPDATE                   (UINT64_C(1) << 12)

#if defined(__EMSCRIPTEN__)
    #define TEST_WEB_PLATFORM 1
#else
    #define TEST_WEB_PLATFORM 0
#endif

// A web page has no close button and cannot be moved, minimized or maximized. Accepting the
// close request ends the run, so that step goes last although it is required.
typedef enum {
    STEP_FOCUS,
    STEP_RESIZE,
#if !TEST_WEB_PLATFORM
    STEP_MOVE,
    STEP_CLOSE_REFUSED,
    STEP_MINIMIZE,
    STEP_MAXIMIZE,
    STEP_CLOSE_ACCEPTED,
#endif
    STEP_COUNT
} TestStepId;

typedef struct {
    bool        required;
    bool        confirmed_by_key;
    const char *label;
    const char *action;
    const char *done_text;
} TestStep;

static const TestStep g_steps[STEP_COUNT] = {
#if TEST_WEB_PLATFORM
    { true,  false, "focus",
             "Click outside the canvas, then click the canvas again.",
             "inactive and active again" },
    { true,  false, "resize",
             "Resize the browser window so that the canvas changes size.",
             "new size reported" },
#else
    { true,  false, "focus",
             "Click another application, then click this window again.",
             "inactive and active again" },
    { true,  false, "resize",
             "Drag an edge or a corner of the window to change its size.",
             "new size reported" },
    { true,  true,  "move",
             "Drag the window by its title bar without changing its size, then press Space.",
             "moved" },
    { true,  false, "close refused",
             "Click the close button of the window. The test refuses this request, so the window "
             "stays open.",
             "request refused, window still open" },
    { false, true,  "minimize and restore",
             "Minimize the window, restore it, then press Space.",
             "minimized and restored" },
    { false, true,  "maximize and restore",
             "Maximize the window, restore it, then press Space.",
             "maximized and restored" },
    { true,  false, "close accepted",
             "Click the close button again. This time the test accepts it and the run ends.",
             "request accepted, window closed" },
#endif
};

#define TEST_FINISH_ACTION     "Press Escape"

#define TEST_MAX_LOGGED_ERRORS (STEP_COUNT + 16u)
#define TEST_ERROR_TEXT        160u

// Guidance goes to stderr and the record to stdout, so redirecting stdout to a file keeps the
// steps on screen. On the web stderr reaches console.error, which makes the browser render
// the whole Asyncify await chain on every line, so both share stdout there.
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

typedef struct {
    unsigned inactive;
    unsigned active;
    unsigned resizes;
} StepEvents;

typedef struct {
    struct mfb_window *window;
    bool               finalized;
    bool               first_pump_done;
    bool               expected_active;
    bool               active_at_open;
    bool               saw_inactive;
    bool               saw_reactivated;
    bool               saw_new_size;
    bool               close_refused;
    bool               refusal_survived;
    bool               close_accepted;
    bool               closed_by_code;
    bool               mfb_close_checked;
    bool               mfb_close_called_callback;
    bool               mfb_close_left_open;
    bool               close_after_accept_ended_late;
    bool               skipped_required;
    unsigned           open_width;
    unsigned           open_height;
    unsigned           open_drawable[4];
    float              monitor_scale_x;
    float              monitor_scale_y;
    unsigned           last_width;
    unsigned           last_height;
    unsigned           first_pump_active;
    unsigned           first_pump_resizes;
    unsigned           same_size_resizes;
    const char        *first_same_size_when;
    StepEvents         step_events[STEP_COUNT];
    bool               step_done[STEP_COUNT];
    bool               step_skipped[STEP_COUNT];
    bool               step_confirmed[STEP_COUNT];
    unsigned           error_count;
    uint64_t           reported_errors;
    char               error_log[TEST_MAX_LOGGED_ERRORS][TEST_ERROR_TEXT];
} WindowTest;

static void
record_error(WindowTest *test, const char *text) {
    if (test->error_count < TEST_MAX_LOGGED_ERRORS) {
        snprintf(test->error_log[test->error_count], TEST_ERROR_TEXT, "%s", text);
    }

    test->error_count++;
    guide("ERROR: %s", text);
}

static void
report_error(WindowTest *test, uint64_t error, const char *format, ...) {
    if ((test->reported_errors & error) != 0) {
        return;
    }

    test->reported_errors |= error;

    char    text[TEST_ERROR_TEXT];
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(text, sizeof(text), format, arguments);
    va_end(arguments);

    record_error(test, text);
}

static void
report_missing(WindowTest *test, const char *action) {
    char text[TEST_ERROR_TEXT];

    snprintf(text, sizeof(text), "required action was not observed: %s", action);
    record_error(test, text);
}

static void
evaluate_steps(const WindowTest *test, bool *done) {
    done[STEP_FOCUS] = test->saw_reactivated;
    done[STEP_RESIZE] = test->saw_new_size;
#if !TEST_WEB_PLATFORM
    done[STEP_MOVE] = test->step_confirmed[STEP_MOVE];
    done[STEP_CLOSE_REFUSED] = test->refusal_survived;
    done[STEP_MINIMIZE] = test->step_confirmed[STEP_MINIMIZE];
    done[STEP_MAXIMIZE] = test->step_confirmed[STEP_MAXIMIZE];
    done[STEP_CLOSE_ACCEPTED] = test->close_accepted;
#endif
}

static int
pending_step(const WindowTest *test) {
    for (unsigned step = 0; step < STEP_COUNT; ++step) {
        if (test->step_done[step] == false && test->step_skipped[step] == false) {
            return (int) step;
        }
    }

    return -1;
}

static void
announce_next_step(const WindowTest *test) {
    int step = pending_step(test);

    guide("");
    if (step < 0) {
        guide("NEXT: nothing left. %s to finish.", TEST_FINISH_ACTION);
        return;
    }

    guide("NEXT: %s (step %d of %d, %s).",
          g_steps[step].action,
          step + 1, (int) STEP_COUNT,
          g_steps[step].required == true ? "required" : "optional"
    );
    guide("      Escape skips this step.");
}

static void
update_progress(WindowTest *test) {
    bool done[STEP_COUNT] = { false };
    bool advanced = false;

    evaluate_steps(test, done);

    for (unsigned step = 0; step < STEP_COUNT; ++step) {
        if (done[step] == true && test->step_done[step] == false &&
            test->step_skipped[step] == false) {
            test->step_done[step] = true;
            guide("DONE: %s, %s", g_steps[step].label, g_steps[step].done_text);
            advanced = true;
        }
    }

    if (advanced == true && test->close_accepted == false) {
        announce_next_step(test);
    }
}

static StepEvents *
current_step_events(WindowTest *test) {
    int step = pending_step(test);

    return step < 0 ? NULL : &test->step_events[step];
}

static const char *
current_step_label(const WindowTest *test) {
    int step = pending_step(test);

    if (test->first_pump_done == false) {
        return "first event pump";
    }

    return step < 0 ? "after the last step" : g_steps[step].label;
}

static WindowTest *
get_test(struct mfb_window *window) {
    return (WindowTest *) mfb_get_user_data(window);
}

static void
window_active(struct mfb_window *window, bool is_active) {
    WindowTest *test = get_test(window);
    bool getter_active = mfb_is_window_active(window);

    printf("ACTIVE: %s\n", is_active == true ? "true" : "false");
    fflush(stdout);

    if (getter_active != is_active) {
        report_error(test, ERROR_ACTIVE_GETTER,
                     "mfb_is_window_active is %d inside an active callback that reports %d",
                     getter_active, is_active);
    }
    if (test->expected_active == is_active) {
        report_error(test, ERROR_ACTIVE_DUPLICATE,
                     "active callback reported %s twice without the opposite transition",
                     is_active == true ? "true" : "false");
    }
    test->expected_active = is_active;

    if (test->first_pump_done == false) {
        test->first_pump_active++;
    }

    StepEvents *events = current_step_events(test);
    if (events != NULL) {
        if (is_active == true) {
            events->active++;
        }
        else {
            events->inactive++;
        }
    }

    if (pending_step(test) == STEP_FOCUS) {
        if (is_active == false) {
            test->saw_inactive = true;
        }
        else if (test->saw_inactive == true) {
            test->saw_reactivated = true;
        }
    }

    update_progress(test);
}

static void
window_resize(struct mfb_window *window, int width, int height) {
    WindowTest *test = get_test(window);
    unsigned    offset_x, offset_y, drawable_width, drawable_height;
    unsigned    getter_width, getter_height;

    mfb_get_drawable_bounds(window, &offset_x, &offset_y, &drawable_width, &drawable_height);
    mfb_get_window_size(window, &getter_width, &getter_height);

    printf("RESIZE: %dx%d, drawable %u,%u %ux%u\n",
           width, height, offset_x, offset_y, drawable_width, drawable_height);
    fflush(stdout);

    if (test->first_pump_done == false) {
        test->first_pump_resizes++;
    }

    StepEvents *events = current_step_events(test);
    if (events != NULL) {
        events->resizes++;
    }

    if (width <= 0 || height <= 0) {
        report_error(test, ERROR_RESIZE_EMPTY,
                     "resize callback reported an empty size %dx%d (%s)",
                     width, height, current_step_label(test));
        update_progress(test);
        return;
    }

    if ((unsigned) width == test->last_width && (unsigned) height == test->last_height) {
        if (test->same_size_resizes == 0) {
            test->first_same_size_when = current_step_label(test);
        }
        test->same_size_resizes++;
        report_error(test, ERROR_RESIZE_SAME_SIZE,
                     "resize callback repeated the current size %dx%d (%s)",
                     width, height, current_step_label(test));
    }
    // A resize before the step asks for one, such as opening the browser developer tools,
    // proves nothing about the person resizing the window.
    else if (pending_step(test) == STEP_RESIZE) {
        test->saw_new_size = true;
    }

    if (getter_width != (unsigned) width || getter_height != (unsigned) height ||
        mfb_get_window_width(window) != (unsigned) width ||
        mfb_get_window_height(window) != (unsigned) height) {
        report_error(test, ERROR_RESIZE_GETTER,
                     "window size getters report %ux%u inside a resize callback that reports %dx%d",
                     getter_width, getter_height, width, height);
    }

    if (offset_x + drawable_width > (unsigned) width ||
        offset_y + drawable_height > (unsigned) height) {
        report_error(test, ERROR_DRAWABLE_OUTSIDE,
                     "drawable area %u,%u %ux%u does not fit in the %dx%d window",
                     offset_x, offset_y, drawable_width, drawable_height, width, height);
    }

    test->last_width = (unsigned) width;
    test->last_height = (unsigned) height;

    update_progress(test);
}

static bool
window_close(struct mfb_window *window) {
    WindowTest *test = get_test(window);
    bool        accept = true;

    if (test->closed_by_code == true) {
        test->mfb_close_called_callback = true;
        report_error(test, ERROR_CLOSE_AFTER_MFB_CLOSE,
                     "close callback was called after mfb_close, which does not ask for confirmation");
    }

#if !TEST_WEB_PLATFORM
    // Any other step accepts, so a person who wants out is never trapped in the run.
    if (pending_step(test) == STEP_CLOSE_REFUSED) {
        accept = false;
        test->close_refused = true;
    }
    else if (pending_step(test) == STEP_CLOSE_ACCEPTED) {
        test->close_accepted = true;
    }
#endif

    printf("CLOSE: request %s\n", accept == true ? "accepted" : "refused");
    fflush(stdout);

    update_progress(test);
    return accept;
}

static void
keyboard(struct mfb_window *window, mfb_key key, mfb_key_mod mod, bool is_pressed) {
    (void) mod;

    if (is_pressed == true) {
        return;
    }

    WindowTest *test = get_test(window);
    int         step = pending_step(test);

    if (key == CONFIRM_KEY && step >= 0 && g_steps[step].confirmed_by_key == true) {
        test->step_confirmed[step] = true;
        update_progress(test);
        return;
    }

    if (key != SKIP_KEY) {
        return;
    }

    if (step >= 0) {
        test->step_skipped[step] = true;
        guide("SKIPPED: %s", g_steps[step].action);
        announce_next_step(test);
        return;
    }

    test->closed_by_code = true;
    test->mfb_close_checked = true;
    mfb_close(window);
}

// Only after mfb_update: macOS with Metal and Wayland hold the resize callback until the next
// mfb_update, so a size that changes during mfb_wait_sync is reported one call later.
static void
check_after_update(WindowTest *test) {
    bool     active = mfb_is_window_active(test->window);
    unsigned width, height;

    if (active != test->expected_active) {
        report_error(test, ERROR_ACTIVE_SNAPSHOT,
                     "mfb_is_window_active changed from %d to %d without a callback",
                     test->expected_active, active);
    }

    mfb_get_window_size(test->window, &width, &height);
    if (width != test->last_width || height != test->last_height) {
        report_error(test, ERROR_SIZE_SNAPSHOT,
                     "window size changed from %ux%u to %ux%u without a resize callback",
                     test->last_width, test->last_height, width, height);
        test->last_width = width;
        test->last_height = height;
    }
}

// The call that runs the close request may still return normally, since the request can
// arrive after that call checked for it. The next one has to end the run.
static void
check_call_result(WindowTest *test, bool window_open,
                  bool refused_before, bool accepted_before, bool closed_before) {
    if (window_open == false) {
        if (test->close_refused == true && test->refusal_survived == false) {
            report_error(test, ERROR_CLOSE_REFUSAL_IGNORED,
                         "the window closed although the close callback refused the request");
        }
        return;
    }

    if (refused_before == true && test->refusal_survived == false) {
        test->refusal_survived = true;
        update_progress(test);
    }
    if (accepted_before == true) {
        test->close_after_accept_ended_late = true;
        report_error(test, ERROR_CLOSE_ACCEPT_IGNORED,
                     "the window stayed open after the close callback accepted the request");
    }
    if (closed_before == true) {
        test->mfb_close_left_open = true;
        report_error(test, ERROR_MFB_CLOSE_IGNORED,
                     "the window stayed open after mfb_close");
    }
}

static const char *
step_result(const WindowTest *test, TestStepId step, const bool *done) {
    if (test->step_skipped[step] == true) {
        return "SKIPPED";
    }
    if (done[step] == true) {
        return g_steps[step].done_text;
    }

    return g_steps[step].required == true ? "NOT DONE" : "not reached";
}

#if !TEST_WEB_PLATFORM
static void
print_step_events_line(const WindowTest *test, const char *label, TestStepId step,
                       const bool *done) {
    const StepEvents *events = &test->step_events[step];

    if (done[step] == false) {
        printf("  %s %s\n", label, step_result(test, step, done));
        return;
    }

    printf("  %s %u inactive / %u active / %u resize\n",
           label, events->inactive, events->active, events->resizes);
}
#endif

// One line per contract point, in a fixed order and wording, so two runs on two backends can
// be compared with diff.
static void
print_summary(const WindowTest *test) {
    bool done[STEP_COUNT] = { false };

    evaluate_steps(test, done);

    puts("");
    puts("CONTRACT SUMMARY");
    for (unsigned step = 0; step < STEP_COUNT; ++step) {
        if (g_steps[step].required == true) {
            printf("  %-24s %s\n", g_steps[step].label,
                   step_result(test, (TestStepId) step, done));
        }
    }

    printf("  window at open .......... %ux%u (asked for %ux%u)\n",
           test->open_width, test->open_height, TEST_WIDTH, TEST_HEIGHT);
    printf("  monitor scale ........... %.2f x %.2f\n",
           test->monitor_scale_x, test->monitor_scale_y);
    printf("  drawable at open ........ %u,%u %ux%u\n",
           test->open_drawable[0], test->open_drawable[1],
           test->open_drawable[2], test->open_drawable[3]);
    printf("  active at open .......... %s\n", test->active_at_open == true ? "true" : "false");
    printf("  first event pump ........ %u active / %u resize\n",
           test->first_pump_active, test->first_pump_resizes);

    if (test->same_size_resizes == 0) {
        puts("  resize with same size ... none");
    }
    else {
        printf("  resize with same size ... %u, first one in %s\n",
               test->same_size_resizes, test->first_same_size_when);
    }

#if !TEST_WEB_PLATFORM
    print_step_events_line(test, "minimize and restore ....", STEP_MINIMIZE, done);
    print_step_events_line(test, "maximize and restore ....", STEP_MAXIMIZE, done);
    printf("  close refused ........... %s\n",
           test->refusal_survived == true ? "window stayed open" : "NOT DONE");
    if (test->close_accepted == false) {
        puts("  close accepted .......... NOT DONE");
    }
    else {
        printf("  close accepted .......... %s\n",
               test->close_after_accept_ended_late == true ? "window stayed open" : "run ended");
    }
#endif

    if (test->mfb_close_checked == false) {
        puts("  mfb_close ............... NOT DONE");
    }
    else {
        printf("  mfb_close ............... %s, close callback %s\n",
               test->mfb_close_left_open == true ? "window stayed open" : "window closed",
               test->mfb_close_called_callback == true ? "called" : "not called");
    }
    fflush(stdout);
}

static void
finish_test(WindowTest *test) {
    bool     done[STEP_COUNT] = { false };
    unsigned skipped = 0;

    if (test->finalized == true) {
        return;
    }
    test->finalized = true;
    evaluate_steps(test, done);

    for (unsigned step = 0; step < STEP_COUNT; ++step) {
        if (g_steps[step].required == false || test->step_skipped[step] == true) {
            continue;
        }
        if (done[step] == false) {
            report_missing(test, g_steps[step].action);
        }
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

#if !TEST_WEB_PLATFORM
static bool
auxiliary_close(struct mfb_window *window) {
    bool *called = (bool *) mfb_get_user_data(window);

    *called = true;
    return true;
}

// Accepting a close request and mfb_close both end the run, so mfb_close is checked on a
// window of its own before the steps start. Web ends every run with mfb_close instead.
static void
check_mfb_close(WindowTest *test, uint32_t *buffer) {
    bool               callback_called = false;
    struct mfb_window *window = mfb_open_ex("MiniFB mfb_close check", AUX_WIDTH, AUX_HEIGHT, 0);

    if (window == NULL) {
        guide("WARNING: could not open the window for the mfb_close check");
        return;
    }

    mfb_set_user_data(window, &callback_called);
    mfb_set_close_callback(window, auxiliary_close);

    mfb_close(window);
    test->mfb_close_checked = true;

    unsigned calls = 0;
    while (mfb_update(window, buffer) == MFB_STATE_OK) {
        test->mfb_close_left_open = true;
        if (++calls >= 10u) {
            break;
        }
    }

    printf("MFB_CLOSE: second window %s, close callback %s\n",
           test->mfb_close_left_open == true ? "stayed open" : "closed",
           callback_called == true ? "called" : "not called");
    fflush(stdout);

    if (test->mfb_close_left_open == true) {
        report_error(test, ERROR_MFB_CLOSE_IGNORED,
                     "mfb_update did not end the window after mfb_close");
    }
    if (callback_called == true) {
        test->mfb_close_called_callback = true;
        report_error(test, ERROR_CLOSE_AFTER_MFB_CLOSE,
                     "close callback was called after mfb_close, which does not ask for confirmation");
    }
}
#endif

static void
print_instructions(void) {
    guide("MiniFB interactive window event contract test");
    guide("");
    guide("The test asks for one action at a time. Do what the NEXT line says, or press Escape");
    guide("to skip a step you cannot perform. Steps that end with Space cannot be detected by");
    guide("the test, so press Space once you have done them.");
    guide("");
    guide("Steps and progress go to stderr, the event log and the summary go to stdout, so");
    guide("running it as \"window_events_test > log.txt\" keeps this panel on screen.");
}

static void
fill_buffer(uint32_t *buffer) {
    for (unsigned y = 0; y < TEST_HEIGHT; ++y) {
        for (unsigned x = 0; x < TEST_WIDTH; ++x) {
            bool border = x < 8 || y < 8 || x >= TEST_WIDTH - 8 || y >= TEST_HEIGHT - 8;
            buffer[y * TEST_WIDTH + x] = border == true
                ? MFB_RGB(220, 160, 60)
                : MFB_RGB(18, 24, 32);
        }
    }
}

int
main(void) {
    print_instructions();

    uint32_t *buffer = (uint32_t *) malloc(TEST_WIDTH * TEST_HEIGHT * sizeof(uint32_t));
    if (buffer == NULL) {
        fputs("ERROR: could not allocate the test framebuffer\n", stderr);
        return EXIT_FAILURE;
    }
    fill_buffer(buffer);

    mfb_set_log_level(MFB_LOG_WARNING);

    WindowTest test = { 0 };
#if !TEST_WEB_PLATFORM
    check_mfb_close(&test, buffer);
#endif
    test.window = mfb_open_ex("MiniFB window event contract test",
                              TEST_WIDTH, TEST_HEIGHT, MFB_WF_RESIZABLE);
    if (test.window == NULL) {
        fputs("ERROR: could not open the test window\n", stderr);
        free(buffer);
        return EXIT_FAILURE;
    }

    mfb_set_user_data(test.window, &test);

    mfb_get_window_size(test.window, &test.open_width, &test.open_height);
    mfb_get_drawable_bounds(test.window,
                            &test.open_drawable[0], &test.open_drawable[1],
                            &test.open_drawable[2], &test.open_drawable[3]);
    mfb_get_monitor_scale(test.window, &test.monitor_scale_x, &test.monitor_scale_y);
    test.active_at_open = mfb_is_window_active(test.window);
    test.expected_active = test.active_at_open;
    test.last_width = test.open_width;
    test.last_height = test.open_height;

    mfb_set_active_callback(test.window, window_active);
    mfb_set_resize_callback(test.window, window_resize);
    mfb_set_close_callback(test.window, window_close);
    mfb_set_keyboard_callback(test.window, keyboard);
    mfb_set_target_fps(TEST_FPS);

    announce_next_step(&test);

    for (;;) {
        bool refused_before  = test.close_refused;
        bool accepted_before = test.close_accepted;
        bool closed_before   = test.closed_by_code;

        mfb_update_state state = mfb_update(test.window, buffer);
        test.first_pump_done = true;
        if (state != MFB_STATE_OK) {
            if (state != MFB_STATE_EXIT) {
                report_error(&test, ERROR_UPDATE,
                             "mfb_update returned unexpected state %d", state);
            }
            check_call_result(&test, false, refused_before, accepted_before, closed_before);
            test.window = NULL;
            break;
        }
        check_call_result(&test, true, refused_before, accepted_before, closed_before);
        check_after_update(&test);

        refused_before  = test.close_refused;
        accepted_before = test.close_accepted;
        closed_before   = test.closed_by_code;

        bool window_open = mfb_wait_sync(test.window);
        check_call_result(&test, window_open, refused_before, accepted_before, closed_before);
        if (window_open == false) {
            test.window = NULL;
            break;
        }
    }

    finish_test(&test);
    free(buffer);
    return test.error_count == 0 && test.skipped_required == false
         ? EXIT_SUCCESS
         : EXIT_FAILURE;
}
