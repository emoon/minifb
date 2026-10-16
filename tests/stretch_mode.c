#include <MiniFB_internal.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(__EMSCRIPTEN__)
    #include <emscripten.h>
#endif

#if defined(MINIFB_USE_MMGR)
    #include "mmgr.h"
#endif

static int failures = 0;
static const char *titles[2] = { "MiniFB stretch mode A", "MiniFB stretch mode B" };
static uint32_t pixels[4] = { 0xff000000, 0xffffffff, 0xff000000, 0xffffffff };

static void
check(bool condition, const char *message) {
    if (condition == false) {
        fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}

static void
close_window(struct mfb_window *window) {
    if (window != NULL) {
        mfb_close(window);
        check(mfb_update_events(window) == MFB_STATE_EXIT, "close window");
    }
}

static void
present(struct mfb_window *window) {
    check(mfb_update_ex(window, pixels, 2, 2) == MFB_STATE_OK, "present frame");
}

#if defined(__EMSCRIPTEN__)
EM_JS(int, canvas_has_style, (const char *title, const char *value, int important), {
    const style = document.getElementById(UTF8ToString(title)).style;
    return style.getPropertyValue("image-rendering") === UTF8ToString(value) &&
           style.getPropertyPriority("image-rendering") === (important ? "important" : "");
});

EM_JS(void, set_canvas_style, (const char *title, const char *value, int important), {
    document.getElementById(UTF8ToString(title)).style.setProperty("image-rendering", UTF8ToString(value), important ? "important" : "");
});
#endif

int
main(void) {
#if defined(MINIFB_USE_MMGR)
    m_enableMultithread();
    m_setLogFiles(MINIFB_MMGR_MEMORY_LOG, MINIFB_MMGR_LEAKS_LOG);
#endif

    struct mfb_window *windows[2];
    windows[0] = mfb_open(titles[0], 320, 240);
    windows[1] = mfb_open(titles[1], 320, 240);
    if (windows[0] == NULL || windows[1] == NULL) {
        fprintf(stderr, "FAIL: cannot open the test windows\n");
        close_window(windows[0]);
        close_window(windows[1]);
        return EXIT_FAILURE;
    }

    SWindowData *window_data[2] = { (SWindowData *) windows[0], (SWindowData *) windows[1] };
    check(window_data[0]->stretch_mode == MFB_STRETCH_NEAREST && window_data[1]->stretch_mode == MFB_STRETCH_NEAREST, "nearest default");
    check(mfb_set_stretch_mode(NULL, MFB_STRETCH_BILINEAR) == false, "reject null window");
    check(mfb_set_stretch_mode(windows[0], MFB_STRETCH_BILINEAR), "select bilinear");
    check(mfb_set_stretch_mode(windows[0], MFB_STRETCH_BILINEAR), "repeat current mode");
    check(window_data[0]->stretch_mode == MFB_STRETCH_BILINEAR && window_data[1]->stretch_mode == MFB_STRETCH_NEAREST, "independent windows");
    check(mfb_set_stretch_mode(windows[0], (mfb_stretch_mode) -1) == false, "reject negative mode");
    check(mfb_set_stretch_mode(windows[0], (mfb_stretch_mode) 99) == false, "reject unknown mode");
    check(window_data[0]->stretch_mode == MFB_STRETCH_BILINEAR, "invalid mode leaves filter unchanged");
    check(mfb_set_stretch_mode(windows[1], MFB_STRETCH_BILINEAR), "select bilinear in second window");
    check(mfb_set_stretch_mode(windows[0], MFB_STRETCH_NEAREST), "switch back to nearest");
    check(window_data[0]->stretch_mode == MFB_STRETCH_NEAREST && window_data[1]->stretch_mode == MFB_STRETCH_BILINEAR, "independent switch back");
    present(windows[0]);
    present(windows[1]);

#if defined(__EMSCRIPTEN__)
    check(canvas_has_style(titles[0], "pixelated", 0), "nearest CSS");
    // stretch_mode_test.html gives the second canvas an inline image-rendering style. The window is bilinear, so
    // MiniFB would have written "auto"
    check(canvas_has_style(titles[1], "pixelated", 0), "preserve initial application CSS");

    close_window(windows[0]);
    check(canvas_has_style(titles[0], "", 0), "remove MiniFB CSS on close");
    windows[0] = mfb_open(titles[0], 320, 240);
    if (windows[0] == NULL) {
        close_window(windows[1]);
        return EXIT_FAILURE;
    }
    check(mfb_set_stretch_mode(windows[0], MFB_STRETCH_BILINEAR), "select bilinear after reopen");
    present(windows[0]);
    check(canvas_has_style(titles[0], "auto", 0), "bilinear CSS after reopen");

    set_canvas_style(titles[0], "auto", 1);
    check(mfb_set_stretch_mode(windows[0], MFB_STRETCH_NEAREST), "nearest with application CSS");
    present(windows[0]);
    check(canvas_has_style(titles[0], "auto", 1), "preserve application CSS priority");
    close_window(windows[0]);
    windows[0] = NULL;
    check(canvas_has_style(titles[0], "auto", 1), "preserve application CSS on close");

    set_canvas_style(titles[0], "", 0);
    windows[0] = mfb_open(titles[0], 320, 240);
    if (windows[0] == NULL) {
        close_window(windows[1]);
        return EXIT_FAILURE;
    }
    // Differs from the "pixelated" that MiniFB wrote for nearest, so it counts as an application style
    set_canvas_style(titles[0], "auto", 0);
    close_window(windows[0]);
    windows[0] = NULL;
    check(canvas_has_style(titles[0], "auto", 0), "preserve CSS changed before close");
#endif

    close_window(windows[0]);
    close_window(windows[1]);
    printf("Stretch mode checks: %d failures.\n", failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
