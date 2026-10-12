#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <MiniFB.h>

#if defined(MINIFB_USE_MMGR)
    #include "mmgr.h"
#endif

#define TEST_TAG        "noise"

//-------------------------------------
static uint32_t  g_width  = 800;
static uint32_t  g_height = 600;
static uint32_t *g_buffer = NULL;

//-------------------------------------
void
resize(struct mfb_window *window, int width, int height) {
    if (width <= 0 || height <= 0) {
        return;
    }
    if ((uint32_t) width > UINT32_MAX / sizeof(*g_buffer) ||
        (size_t) height > SIZE_MAX / sizeof(*g_buffer) / (size_t) width) {
        mfb_close(window);
        return;
    }

    uint32_t *buffer = realloc(g_buffer, (size_t) width * height * sizeof(*g_buffer));
    if (buffer == NULL) {
        mfb_close(window);
        return;
    }

    g_buffer = buffer;
    g_width  = (uint32_t) width;
    g_height = (uint32_t) height;
    MFB_LOGI(TEST_TAG, "resize: %dx%d", width, height);
}

//-------------------------------------
int
main() {
#if defined(MINIFB_USE_MMGR)
    // mmgr replaces the global operator new, so system libraries also allocate through it from their own threads
    m_enableMultithread();
    m_setLogFiles(MINIFB_MMGR_MEMORY_LOG, MINIFB_MMGR_LEAKS_LOG);
#endif

    size_t      i;
    uint32_t    noise, carry, seed = 0xbeef;

    struct mfb_window *window = mfb_open_ex("Noise Test", g_width, g_height, MFB_WF_RESIZABLE);
    if (!window) {
        return -1;
    }

    mfb_set_resize_callback(window, resize);

    g_buffer = (uint32_t *) malloc((size_t) g_width * g_height * sizeof(*g_buffer));
    if (g_buffer == NULL) {
        mfb_close(window);
        mfb_update_events(window);
        return -1;
    }

    mfb_update_state state;
    do {
        for (i = 0; i < (size_t) g_width * g_height; ++i) {
            noise = seed;
            noise >>= 3;
            noise ^= seed;
            carry = noise & 1;
            noise >>= 1;
            seed >>= 1;
            seed |= (carry << 30);
            noise &= 0xFF;
            g_buffer[i] = MFB_RGB(noise, 0, 0);
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
