// press 'H' to hide, press 'S' to show

#include <MiniFB.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#if defined(MINIFB_USE_MMGR)
    #include "mmgr.h"
#endif

static uint32_t  g_width  = 800;
static uint32_t  g_height = 600;
static uint32_t *g_buffer = NULL;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void
resize(struct mfb_window *window, int width, int height) {
    (void) window;
    g_width  = width;
    g_height = height;
    g_buffer = realloc(g_buffer, g_width * g_height * 4);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void
keyboard_event(struct mfb_window *window, mfb_key key, mfb_key_mod mod, bool is_pressed) {
    if (key == MFB_KB_KEY_H) {
        mfb_show_cursor(window, false);
    }
    else if (key == MFB_KB_KEY_S) {
        mfb_show_cursor(window, true);
    }
    else if (key == MFB_KB_KEY_ESCAPE) {
        mfb_close(window);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int
main() {
#if defined(MINIFB_USE_MMGR)
    // mmgr replaces the global operator new, so system libraries also allocate through it from their own threads
    m_enableMultithread();
    m_setLogFiles(MINIFB_MMGR_MEMORY_LOG, MINIFB_MMGR_LEAKS_LOG);
#endif

    uint32_t    i, noise, carry, seed = 0xbeef;

    struct mfb_window *window = mfb_open_ex("H to hide, S to show", g_width, g_height, MFB_WF_RESIZABLE);
    if (!window)
        return 0;

    mfb_set_keyboard_callback(window, keyboard_event);

    g_buffer = (uint32_t *) malloc(g_width * g_height * 4);
    mfb_set_resize_callback(window, resize);

    mfb_set_viewport(window, 50, 50, g_width - 50 - 50, g_height - 50 - 50);
    resize(window, g_width - 100, g_height - 100);  // to resize buffer

    mfb_update_state state;
    do {
        for (i = 0; i < g_width * g_height; ++i) {
            noise = seed;
            noise >>= 3;
            noise ^= seed;
            carry = noise & 1;
            noise >>= 1;
            seed >>= 1;
            seed |= (carry << 30);
            noise &= 0xFF;
            g_buffer[i] = MFB_RGB(noise, noise, noise);
        }

        state = mfb_update_ex(window, g_buffer, g_width, g_height);
        if (state != MFB_STATE_OK) {
            window = NULL;
            break;
        }
    } while (mfb_wait_sync(window));

    free(g_buffer);
    g_buffer = NULL;

    return 0;
}
