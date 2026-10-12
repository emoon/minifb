#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <MiniFB.h>

#if defined(MINIFB_USE_MMGR)
    #include "mmgr.h"
#endif

#define TEST_TAG        "timer"

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
    char        title[64];

    struct mfb_window *window = mfb_open_ex("Timer Test", g_width, g_height, MFB_WF_RESIZABLE);
    if (!window) {
        return -1;
    }

    mfb_set_resize_callback(window, resize);

    mfb_set_viewport(window, 50, 50, g_width - 50 - 50, g_height - 50 - 50);
    resize(window, g_width - 100, g_height - 100);  // to resize buffer
    if (g_buffer == NULL) {
        mfb_close(window);
        mfb_update_events(window);
        return -1;
    }

    struct mfb_timer *timer = mfb_timer_create();
    if (timer == NULL) {
        mfb_close(window);
        mfb_update_events(window);
        free(g_buffer);
        return -1;
    }
    double time = 0;
    int frames = 0;

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
            g_buffer[i] = MFB_RGB(noise, 0, noise);
        }

        state = mfb_update_ex(window, g_buffer, g_width, g_height);
        if (state != MFB_STATE_OK) {
            window = NULL;
            break;
        }

        time += mfb_timer_delta(timer);
        ++frames;
        if (frames >= 60) {
            double fps = frames / time;
            snprintf(title, sizeof(title), "Timer Test - FPS: %.3f", fps);
            mfb_set_title(window, title);
            MFB_LOGI(TEST_TAG, "FPS: %.3f", fps);
            frames = 0;
            time = 0;

        }
    } while (mfb_wait_sync(window));

    mfb_timer_destroy(timer);
    free(g_buffer);
    g_buffer = NULL;

    return 0;
}
