#include <MiniFB.h>
#include <stdio.h>
#include <stdint.h>

//-------------------------------------
#define kUnused(var)    (void) var
#define TEST_TAG        "fullscreen"

//-------------------------------------
#define WIDTH      800
#define HEIGHT     640
static unsigned int g_buffer[WIDTH * HEIGHT];
static bool         g_active = true;

void
draw_noise(struct mfb_window *window) {
    static int seed = 0xbeef;
    int i;
    int noise, carry;

    for (i = 0; i < WIDTH * HEIGHT; ++i) {
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
}

//-------------------------------------
int
main() {
    mfb_update_state state;

    MFB_LOGI(TEST_TAG, "Full Screen Window");
    struct mfb_window *window = mfb_open_ex("full screen auto", WIDTH, HEIGHT, MFB_WF_FULLSCREEN);
    if (!window) {
        MFB_LOGE(TEST_TAG, "Cannot create window");
        return 0;
    }

    mfb_set_viewport_best_fit(window, WIDTH, HEIGHT);

    do {
        if (g_active) {
            draw_noise(window);
            state = mfb_update(window, g_buffer);
        }
        else {
            state = mfb_update_events(window);
        }

        if (state != MFB_STATE_OK) {
            window = NULL;
            break;
        }
    } while (mfb_wait_sync(window));

    //---------------------------------
    MFB_LOGI(TEST_TAG, "Desktop Full Screen Window");
    window = mfb_open_ex("full screen auto", WIDTH, HEIGHT, MFB_WF_FULLSCREEN_DESKTOP);
    if (!window) {
        MFB_LOGE(TEST_TAG, "Cannot create window");
        return 0;
    }

    mfb_set_viewport_best_fit(window, WIDTH, HEIGHT);

    do {
        if (g_active) {
            draw_noise(window);
            state = mfb_update(window, g_buffer);
        }
        else {
            state = mfb_update_events(window);
        }

        if (state != MFB_STATE_OK) {
            window = NULL;
            break;
        }
    } while (mfb_wait_sync(window));

    return 0;
}
