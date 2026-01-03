#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <MiniFB.h>

//-------------------------------------
static uint32_t  g_initial_width  = 800;
static uint32_t  g_initial_height = 600;
static uint32_t  g_draw_width  = 800;
static uint32_t  g_draw_height = 600;
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

    if ((g_draw_width != draw_width) || (g_draw_height != draw_height)) {
        g_draw_width  = draw_width;
        g_draw_height = draw_height;
        g_buffer = realloc(g_buffer, g_draw_width * g_draw_height * 4);
    }
    printf("----\n");
}

//-------------------------------------
void
dpi_callback(struct mfb_window *window, float scale_x, float scale_y) {
    printf("DPI Changed:\n- Scale X: %.2f, Scale Y: %.2f\n",
        scale_x,
        scale_y
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
    printf("----\n");
}

//-------------------------------------
int
main() {
    struct mfb_window *window = mfb_open("DPI Change Test", g_initial_width, g_initial_height);
    if (!window) {
        return -1;
    }

    printf("Before Resize:\n");
    printf("- Window: Width: %d, Height: %d\n",
        mfb_get_window_width(window),
        mfb_get_window_height(window)
    );
    printf("- Draw Area: x: %d, y: %d, Width: %d, Height: %d\n",
        mfb_get_drawable_offset_x(window),
        mfb_get_drawable_offset_y(window),
        mfb_get_drawable_width(window),
        mfb_get_drawable_height(window)
    );
    printf("----\n");

    g_buffer = (uint32_t *) malloc(g_draw_width * g_draw_height * 4);

    // Set callbacks
    mfb_set_dpi_callback(window, dpi_callback);
    mfb_set_resize_callback(window, resize);

    // Reduce 50 pixeles on each margin
    mfb_set_viewport(window, 50, 50, g_draw_width - 50 - 50, g_draw_height - 50 - 50);
    // to resize buffer and do not scale
    resize(window, g_draw_width - 100, g_draw_height - 100);

    mfb_update_state state = STATE_OK;

    printf("Move the window between monitors with different DPI settings\n");
    printf("or change system DPI settings to trigger DPI change events.\n\n");

    do {
        for (uint32_t i = 0; i < (g_draw_width * g_draw_height); i++) {
            g_buffer[i] = 0xff0000ff;  // Red
        }
        state = mfb_update_ex(window, g_buffer, g_draw_width, g_draw_height);
        if (state != STATE_OK) {
            window = NULL;
            break;
        }
    } while(mfb_wait_sync(window));

    return 0;
}
