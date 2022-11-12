#include <MiniFB.h>
#include <stdio.h>
#include <stdint.h>

#define kUnused(var)    (void) var

#define WIDTH      960
#define HEIGHT     640
static unsigned int g_buffer[WIDTH * HEIGHT];
static bool         g_active = true;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


int
main()
{
    int noise, carry, seed = 0xbeef;

    struct mfb_window *window = mfb_open_ex("full screen auto", WIDTH, HEIGHT, WF_FULLSCREEN);
    if (!window)
        return 0;

    mfb_set_viewport_best_fit(window, WIDTH, HEIGHT);

    do {
        int              i;
        mfb_update_state state;

        if(g_active)
        {
            for (i = 0; i < WIDTH * HEIGHT; ++i)
            {
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

            state = mfb_update(window, g_buffer);
        }
        else {
            state = mfb_update_events(window);
        }
        if (state != STATE_OK) {
            window = 0x0;
            break;
        }
    } while(mfb_wait_sync(window));

    return 0;
}
