#include <MiniFB.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

//-------------------------------------
static uint32_t  g_width  = 800;
static uint32_t  g_height = 600;
static uint32_t *g_buffer = NULL;

//-------------------------------------
void
resize(struct mfb_window *window, int width, int height) {
    (void) window;
    g_width  = width;
    g_height = height;
    g_buffer = realloc(g_buffer, g_width * g_height * 4);
}

//-------------------------------------
int
main() {
    uint32_t    i, noise, carry, seed = 0xbeef;

    struct mfb_window *window = mfb_open_ex("Timer Test", g_width, g_height, WF_RESIZABLE);
    if (!window) {
        return 0;
    }

    g_buffer = (uint32_t *) malloc(g_width * g_height * 4);
    mfb_set_resize_callback(window, resize);

    mfb_set_viewport(window, 50, 50, g_width - 50 - 50, g_height - 50 - 50);
    resize(window, g_width - 100, g_height - 100);  // to resize buffer

    struct mfb_timer *timer = mfb_timer_create();
    double time = 0;
    int frames = 0;

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
            g_buffer[i] = MFB_ARGB(0xff, noise, noise, noise);
        }

        state = mfb_update_ex(window, g_buffer, g_width, g_height);
        if (state != STATE_OK) {
            window = NULL;
            break;
        }

        time += mfb_timer_delta(timer);
        ++frames;
        if (frames >= 60) {
            printf("FPS: %.3f\n", (frames / time));
            frames = 0;
            time = 0;

        }
    } while(mfb_wait_sync(window));

    return 0;
}
