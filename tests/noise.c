#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <MiniFB.h>

//-------------------------------------
static uint32_t  g_width  = 800;
static uint32_t  g_height = 600;
static uint32_t *g_buffer = NULL;

//-------------------------------------
void
resize(struct mfb_window *window, int width, int height) {
    printf("Resize:\n");
    printf("- Params: Width: %d, Height: %d\n",
        width,
        height
    );

    float scale_x = 0, scale_y = 0;
    mfb_get_monitor_scale(window, &scale_x, &scale_y);

    printf("- Scale X: %.2f, Scale Y: %.2f\n",
        scale_x,
        scale_y
    );
    printf("- Params Scaled: Width: %d, Height: %d\n",
        (uint32_t) (width  * scale_x),
        (uint32_t) (height * scale_y)
    );

    uint32_t window_width  = mfb_get_window_width(window);
    uint32_t window_height = mfb_get_window_height(window);
    printf("- Window: Width: %d, Height: %d\n",
        window_width,
        window_height
    );

    uint32_t offset_x    = mfb_get_drawable_offset_x(window);
    uint32_t offset_y    = mfb_get_drawable_offset_y(window);
    uint32_t draw_width  = mfb_get_drawable_width(window);
    uint32_t draw_height = mfb_get_drawable_height(window);
    printf("- Draw Area: x: %d, y: %d, Width: %d (%d), Height: %d (%d)\n",
        offset_x,
        offset_y,
        draw_width,
        window_width  - draw_width  - offset_x,
        draw_height,
        window_height - draw_height - offset_y
    );

    if ((g_width != draw_width) || (g_height != draw_height)) {
        g_width  = draw_width;
        g_height = draw_height;
        g_buffer = realloc(g_buffer, g_width * g_height * 4);
    }
    printf("----\n");
}

//-------------------------------------
int
main() {
    uint32_t    i, noise, carry, seed = 0xbeef;

    struct mfb_window *window = mfb_open_ex("Noise Test", g_width, g_height, WF_RESIZABLE);
    if (!window) {
        return -1;
    }

    g_buffer = (uint32_t *) malloc(g_width * g_height * 4);
    mfb_set_resize_callback(window, resize);

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
            g_buffer[i] = MFB_ARGB(0xff, noise, 0, 0);
        }

        state = mfb_update_ex(window, g_buffer, g_width, g_height);
        if (state != STATE_OK) {
            window = NULL;
            break;
        }
    } while(mfb_wait_sync(window));

    return 0;
}
