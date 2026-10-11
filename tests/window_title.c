#include <MiniFB.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(MINIFB_USE_MMGR)
    #include "mmgr.h"
#endif

#define TEST_WIDTH              320u
#define TEST_HEIGHT             240u
#define TEST_OPEN_TITLE         "MiniFB title test"

static int g_failures = 0;

//-------------------------------------
static void
check_title(struct mfb_window *window, const char *label, const char *expected) {
    const char *title = mfb_get_title(window);

    if (title != NULL && strcmp(title, expected) == 0) {
        fprintf(stdout, "PASS  %s\n", label);
        return;
    }

    fprintf(stdout, "FAIL  %s: expected \"%s\", got \"%s\"\n",
            label,
            expected,
            title != NULL ? title : "(null)");
    ++g_failures;
}

//-------------------------------------
static void
set_and_check(struct mfb_window *window, const char *label, const char *title, const char *expected) {
    mfb_set_title(window, title);
    check_title(window, label, expected);
}

//-------------------------------------
int
main(void) {
#if defined(MINIFB_USE_MMGR)
    m_setLogFiles(MINIFB_MMGR_MEMORY_LOG, MINIFB_MMGR_LEAKS_LOG);
#endif

    struct mfb_window *window = mfb_open(TEST_OPEN_TITLE, TEST_WIDTH, TEST_HEIGHT);
    if (window == NULL) {
        fprintf(stderr, "Cannot open the test window.\n");
        return EXIT_FAILURE;
    }

    check_title(window, "title given to mfb_open", TEST_OPEN_TITLE);

    set_and_check(window, "ASCII", "Hello, World!", "Hello, World!");
    set_and_check(window, "2-byte UTF-8", "Caf\xC3\xA9", "Caf\xC3\xA9");
    set_and_check(window, "3-byte UTF-8", "\xE4\xB8\x96\xE7\x95\x8C", "\xE4\xB8\x96\xE7\x95\x8C");
    set_and_check(window, "4-byte UTF-8", "Hi \xF0\x9F\x98\x8A", "Hi \xF0\x9F\x98\x8A");
    set_and_check(window, "stored title passed back in", mfb_get_title(window), "Hi \xF0\x9F\x98\x8A");
    set_and_check(window, "invalid UTF-8 keeps only ASCII", "Caf\xE9 ok", "Caf ok");
    set_and_check(window, "empty title", "", "");

    mfb_set_title(window, "Kept");
    set_and_check(window, "NULL title is ignored", NULL, "Kept");

    check_title(NULL, "NULL window", "");

    // mfb_close only marks the window; the next update releases it, stored title included.
    mfb_close(window);
    mfb_update_events(window);

    fprintf(stdout, "\n%s: %d check(s) failed\n", g_failures == 0 ? "PASS" : "FAIL", g_failures);

    return g_failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
