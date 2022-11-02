#include <MiniFB.h>
#include <stdio.h>
#include <stdint.h>

#define kUnused(var)    (void) var

#define WIDTH      1000
#define HEIGHT     2080
static unsigned int g_buffer[WIDTH * HEIGHT];
static bool         g_active = true;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void
calculate_viewport(struct mfb_window *window) {
    unsigned width  = WIDTH;
    unsigned height = HEIGHT;
    unsigned winWidth  = mfb_get_window_width(window);
    unsigned winHeight = mfb_get_window_height(window);
    unsigned x = 0, y = 0;
    float    scaleX = 1.0f, scaleY = 1.0f;
    mfb_get_monitor_scale(window, &scaleX, &scaleY);

    // Select type
    unsigned type = 0;
    if (width < winWidth) {
        type += 3;
    }
    else if (width > winWidth) {
        type += 6;
    }
    if (height < winHeight) {
        type += 1;
    }
    else if (height > winHeight) {
        type += 2;
    }
    printf("Type: %d\n", type);

    // Fix types: W: < - H: < && W: > - H: >
    if (type == 4 || type == 8) {
        float ratio1 = (float)width / winWidth;
        float ratio2 = (float)height / winHeight;
        if (ratio1 > ratio2) {
            width = winWidth;
            height /= ratio1;
            type = 1;
        }
        else if (ratio1 < ratio2) {
            width /= ratio2;
            height = winHeight;
            type = 3;
        }
        else {
            width = winWidth;
            height = winHeight;
            type = 0;
        }
    }

    // Manage types
    switch (type) {
        // W: = - H: <
        case 1: {
            y = (winHeight - height) >> 1;
            winHeight = height;
        }
        break;

        // W: = - H: >
        case 2: 
        // W: < - H: >
        case 5:
        {
            unsigned aux = width * ((float)winHeight / height);
            x = (winWidth - aux) >> 1;
            winWidth = aux;
        }
        break;

        // W: < - H: =
        case 3: {
            x = (winWidth - width) >> 1;
            winWidth = width;
        }
        break;

        // W: > - H: =
        case 6: 
        // W: > - H: <
        case 7: {
            unsigned aux = height * ((float)winWidth / width);
            y = (winHeight - aux) >> 1;
            winHeight = aux;
        }
        break;
    }

    // Apply changes
    mfb_set_viewport(window, x / scaleX, y / scaleY, winWidth / scaleX, winHeight / scaleY);
    printf("x: %d, y: %d, width: %d, height: %d\n", x, y, winWidth, winHeight);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int
main()
{
    int noise, carry, seed = 0xbeef;

    struct mfb_window *window = mfb_open_ex("full screen auto", WIDTH, HEIGHT, WF_FULLSCREEN);
    if (!window)
        return 0;

    calculate_viewport(window);
    

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
