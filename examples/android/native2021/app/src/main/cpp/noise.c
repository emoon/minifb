#include <android_native_app_glue.h>
#include <android/log.h>
#include <MiniFB.h>
#include <stdlib.h>
#include <math.h>

#if defined(MINIFB_USE_MMGR)
    #include "mmgr.h"
#endif

//-------------------------------------
#define  LOG_TAG    "Noise"

//-------------------------------------
#define kUnused(var)        (void) var;

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

//-------------------------------------
typedef struct {
    bool    enabled;
    int     x, y;
} Pos;

//-------------------------------------
static uint32_t  g_width  = 200;
static uint32_t  g_height = 100;
static uint32_t *g_buffer = NULL;
static Pos       g_positions[16] = {};
static uint32_t  g_frame_count = 0;
static bool      g_is_active = false;

//-------------------------------------
void
active(struct mfb_window *window, bool is_active) {
    MFB_LOGI(LOG_TAG, "active: %d", is_active);
    g_is_active = is_active;
}

//-------------------------------------
void
resize(struct mfb_window *window, int width, int height) {
    MFB_LOGI(LOG_TAG, "resize: %d, %d", width, height);
    if (width <= 1 || height <= 1) {
        return;
    }

    uint32_t buffer_width  = (uint32_t) width  / 2;
    uint32_t buffer_height = (uint32_t) height / 2;
    if (buffer_width > UINT32_MAX / sizeof(*g_buffer) ||
        (size_t) buffer_height > SIZE_MAX / sizeof(*g_buffer) / buffer_width) {
        mfb_close(window);
        return;
    }

    uint32_t *buffer = realloc(g_buffer, (size_t) buffer_width * buffer_height * sizeof(*g_buffer));
    if (buffer == NULL) {
        mfb_close(window);
        return;
    }

    g_buffer = buffer;
    g_width  = buffer_width;
    g_height = buffer_height;

    float scale = 1.0f;
    mfb_get_monitor_scale(window, &scale, NULL);
    int32_t viewport_x = (int32_t) (20 * scale);
    int32_t viewport_y = (int32_t) (20 * scale);
    viewport_x = MIN(viewport_x, (width  - 1) / 2);
    viewport_y = MIN(viewport_y, (height - 1) / 2);
    int32_t viewport_width  = width - 2 * viewport_x;
    int32_t viewport_height = height - 2 * viewport_y;

    mfb_set_viewport(window, viewport_x, viewport_y, viewport_width, viewport_height);
}

//-------------------------------------
void
keyboard(struct mfb_window *window, mfb_key key, mfb_key_mod mod, bool is_pressed) {
    MFB_LOGI(LOG_TAG, "keyboard:");
}

//-------------------------------------
void
char_input(struct mfb_window *window, unsigned int char_code) {
    MFB_LOGI(LOG_TAG, "char_input:");
}

//-------------------------------------
void
mouse_btn(struct mfb_window *window, mfb_mouse_button button, mfb_key_mod mod, bool is_pressed) {
    int x, y, id;
    mfb_decode_touch(mfb_get_mouse_x(window), &x, &id);
    mfb_decode_touch(mfb_get_mouse_y(window), &y, NULL);
    // A finger reports its pointer id in button, and a mouse its real button.
    int pointer_id = (int) button;
    if (id == MFB_POINTER_ID_MOUSE && pointer_id != MFB_POINTER_ID_MOUSE) {
        if (button != MFB_MOUSE_LEFT) {
            return;
        }
        pointer_id = MFB_POINTER_ID_MOUSE;
    }
    if (pointer_id < 0 || pointer_id >= 16) {
        return;
    }
    g_positions[pointer_id].enabled = is_pressed;
    // Cancellation reports each touch id in button without updating the saved coordinates.
    if (is_pressed == true) {
        g_positions[pointer_id].x = x;
        g_positions[pointer_id].y = y;
    }
    MFB_LOGI(LOG_TAG, "mouse_btn: button: id %d=%d, x=%d, y=%d (touch %d)", (int) button, (int) is_pressed, x, y, id);
}

//-------------------------------------
void
mouse_move(struct mfb_window *window, int x, int y) {
    int id;
    mfb_decode_touch(x, &x, &id);
    mfb_decode_touch(y, &y, NULL);
    if (id < 0 || id >= 16) {
        return;
    }
    g_positions[id].x = x;
    g_positions[id].y = y;
    MFB_LOGI(LOG_TAG, "mouse_move: %d, %d [%d]", x, y, id);
}

//-------------------------------------
void
mouse_scroll(struct mfb_window *window, mfb_key_mod mod, float delta_x, float delta_y) {
    MFB_LOGI(LOG_TAG, "mouse_scroll:");
}

//-------------------------------------
static void
draw_positions(struct mfb_window *window) {
    unsigned offset_x, offset_y, draw_width, draw_height;
    mfb_get_drawable_bounds(window, &offset_x, &offset_y, &draw_width, &draw_height);
    if (draw_width == 0 || draw_height == 0) {
        return;
    }

    for (int p = 0; p < 16; ++p) {
        if (g_positions[p].enabled == true) {
            int px = (int) floor(((double) g_positions[p].x - offset_x) * g_width  / draw_width);
            int py = (int) floor(((double) g_positions[p].y - offset_y) * g_height / draw_height);
            int min_x = MAX(px - 16, 0);
            int max_x = MIN(px + 16, (int) g_width);
            int min_y = MAX(py - 16, 0);
            int max_y = MIN(py + 16, (int) g_height);
            for (int y = min_y; y < max_y; ++y) {
                for (int x = min_x; x < max_x; ++x) {
                    g_buffer[(size_t) y * g_width + x] = MFB_RGB(0, 0, 0);
                }
            }
        }
    }
}

// I'm not sure that we can use this function name but it works
//-------------------------------------
int
main(int argc, char *argv[]) {
#if defined(MINIFB_USE_MMGR)
    // When Android recreates the activity, the new android_main thread can start before the old one ends
    m_enableMultithread();
    m_setLogFiles(MINIFB_MMGR_MEMORY_LOG, MINIFB_MMGR_LEAKS_LOG);
#endif

    kUnused(argc);
    kUnused(argv);
    size_t      i;
    uint32_t    x, y, noise, carry, seed = 0xbeef;

    struct mfb_window *window = mfb_open("Ignored", g_width, g_height);
    if (window == NULL)
        return 0;

    mfb_set_active_callback(window, active);
    mfb_set_resize_callback(window, resize);
    mfb_set_keyboard_callback(window, keyboard);            // not working
    mfb_set_char_input_callback(window, char_input);        // not working
    mfb_set_mouse_button_callback(window, mouse_btn);
    mfb_set_mouse_move_callback(window, mouse_move);
    mfb_set_mouse_scroll_callback(window, mouse_scroll);    // not working

    resize(window, (int) mfb_get_window_width(window), (int) mfb_get_window_height(window));
    if (g_buffer == NULL) {
        mfb_close(window);
        mfb_update_events(window);
        return -1;
    }

    mfb_update_state state;
    do {
        if (!g_is_active) {
            // continue runs mfb_wait_sync in the loop condition; calling it here too would use a released window
            continue;
        }

        bool is_active   = mfb_is_window_active(window);
        unsigned width  = mfb_get_window_width(window);
        unsigned height = mfb_get_window_height(window);
        int mouse_x      = mfb_get_mouse_x(window);
        int mouse_y      = mfb_get_mouse_y(window);
        float scrollX   = mfb_get_mouse_scroll_x(window);   // not working
        float scrollY   = mfb_get_mouse_scroll_y(window);   // not working
        const uint8_t *buttons = mfb_get_mouse_button_buffer(window);
        const uint8_t *keys = mfb_get_key_buffer(window);   // not working

        i = 0;
        for (y = 0; y < g_height; ++y) {
            for (x = 0; x < g_width; ++x) {
                noise = seed;
                noise >>= 3;
                noise ^= seed;
                carry = noise & 1;
                noise >>= 1;
                seed >>= 1;
                seed |= (carry << 30);
                noise &= 0xFF;

                // Comment out to test appropriate colour channel
                //g_buffer[i] = MFB_RGB(noise, 0, 0);     // Test red channel
                //g_buffer[i] = MFB_RGB(0, noise, 0);     // Test green channel
                //g_buffer[i] = MFB_RGB(0, 0, noise);     // Test blue channel
                g_buffer[i++] = MFB_RGB(noise, noise, noise);
            }
        }

        draw_positions(window);

        state = mfb_update_ex(window, g_buffer, g_width, g_height);
        if (state != MFB_STATE_OK) {
            window = NULL;
            break;
        }

        g_frame_count++;
    } while (mfb_wait_sync(window));

    free(g_buffer);
    g_buffer = NULL;

#if defined(MINIFB_USE_MMGR)
    // An Android app usually ends without calling exit(), so mmgr may never write its leak report at exit
    m_dumpLeakReport();
#endif
}
