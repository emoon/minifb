#include <android_native_app_glue.h>
#include <android/log.h>
#include <MiniFB.h>
#include <stdlib.h>
#include <math.h>

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
static void
print_getters(struct mfb_window *window) {
    if (!window) {
        return;
    }

    unsigned win_w = 0, win_h = 0;
    unsigned win_sw = 0, win_sh = 0;
    unsigned draw_offset_x = 0, draw_offset_y = 0, draw_w = 0, draw_h = 0;
    unsigned bounds_offset_x = 0, bounds_offset_y = 0, bounds_w = 0, bounds_h = 0;
    unsigned fps = 0;
    int mouse_x = 0, mouse_y = 0;
    float scroll_x = 0.0f, scroll_y = 0.0f;
    float scale_x = 0.0f, scale_y = 0.0f;
    bool is_active = false;

    const char *key_name = mfb_get_key_name(MFB_KB_KEY_ESCAPE);
    const uint8_t *key_buffer = mfb_get_key_buffer(window);

    is_active = mfb_is_window_active(window);
    win_w = mfb_get_window_width(window);
    win_h = mfb_get_window_height(window);
    mfb_get_window_size(window, &win_sw, &win_sh);
    if (win_w != win_sw) {
        MFB_LOGE(LOG_TAG, "Width does not match: %u != %u\n", win_w, win_sw);
    }
    if (win_h != win_sh) {
        MFB_LOGE(LOG_TAG, "Height does not match: %u != %u\n", win_h, win_sh);
    }

    draw_offset_x = mfb_get_drawable_offset_x(window);
    draw_offset_y = mfb_get_drawable_offset_y(window);
    draw_w = mfb_get_drawable_width(window);
    draw_h = mfb_get_drawable_height(window);
    mfb_get_drawable_bounds(window, &bounds_offset_x, &bounds_offset_y, &bounds_w, &bounds_h);

    int mouse_id = 0;
    mfb_decode_touch(mfb_get_mouse_x(window), &mouse_x, &mouse_id);
    mfb_decode_touch(mfb_get_mouse_y(window), &mouse_y, NULL);
    scroll_x = mfb_get_mouse_scroll_x(window);
    scroll_y = mfb_get_mouse_scroll_y(window);
    const uint8_t *mouse_buttons = mfb_get_mouse_button_buffer(window);

    mfb_get_monitor_scale(window, &scale_x, &scale_y);
    fps = mfb_get_target_fps();

    int cutout_left, cutout_right, cutout_top, cutout_bottom;
    int safe_left, safe_right, safe_top, safe_bottom;

    mfb_get_display_cutout_insets(window, &cutout_left, &cutout_top, &cutout_right, &cutout_bottom);
    mfb_get_display_safe_insets(window, &safe_left, &safe_top, &safe_right, &safe_bottom);

    MFB_LOGI(LOG_TAG, "[getters frame=%d]\n", g_frame_count);
    MFB_LOGI(LOG_TAG, "  key_name(MFB_KB_KEY_ESCAPE): %s\n", key_name ? key_name : "(null)");
    MFB_LOGI(LOG_TAG, "  is_window_active: %d\n", is_active);
    MFB_LOGI(LOG_TAG, "  target_fps: %u\n", fps);
    MFB_LOGI(LOG_TAG, "  monitor_scale: %f, %f\n", scale_x, scale_y);
    MFB_LOGI(LOG_TAG, "  window_size: %u x %u\n", win_w, win_h);
    MFB_LOGI(LOG_TAG, "  drawable_offsets: %u, %u\n", draw_offset_x, draw_offset_y);
    MFB_LOGI(LOG_TAG, "  drawable_size: %u x %u\n", draw_w, draw_h);
    MFB_LOGI(LOG_TAG, "  drawable_bounds: offset (%u, %u) size (%u, %u)\n", bounds_offset_x, bounds_offset_y, bounds_w, bounds_h);
    MFB_LOGI(LOG_TAG, "  cutout_insets: left %u, right %u, top %u, bottom %u", cutout_left, cutout_right, cutout_top, cutout_bottom);
    MFB_LOGI(LOG_TAG, "  safe_insets: left %u, right %u, top %u, bottom %u", safe_left, safe_right, safe_top, safe_bottom);
    MFB_LOGI(LOG_TAG, "  mouse_pos: %d, %d (touch %d)\n", mouse_x, mouse_y, mouse_id);
    MFB_LOGI(LOG_TAG, "  mouse_scroll: %f, %f\n", scroll_x, scroll_y);

    if (mouse_buttons) {
        MFB_LOGI(LOG_TAG, "  mouse_buttons[0..7]: %u %u %u %u %u %u %u %u\n",
                mouse_buttons[0], mouse_buttons[1], mouse_buttons[2], mouse_buttons[3],
                mouse_buttons[4], mouse_buttons[5], mouse_buttons[6], mouse_buttons[7]);
    }
    else {
        MFB_LOGI(LOG_TAG, "  mouse_buttons: (null)\n");
    }

    if (key_buffer) {
        MFB_LOGI(LOG_TAG, "  key_buffer sample [ESC=%u, SPACE=%u, LEFT=%u, RIGHT=%u, UP=%u, DOWN=%u]\n",
                key_buffer[MFB_KB_KEY_ESCAPE],
                key_buffer[MFB_KB_KEY_SPACE],
                key_buffer[MFB_KB_KEY_LEFT],
                key_buffer[MFB_KB_KEY_RIGHT],
                key_buffer[MFB_KB_KEY_UP],
                key_buffer[MFB_KB_KEY_DOWN]);
    }
    else {
        MFB_LOGI(LOG_TAG, "  key_buffer: (null)\n");
    }
}

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
    g_width  = (width  >> 1);
    g_height = (height >> 1);
    g_buffer = realloc(g_buffer, g_width * g_height * 4);

    float scale = 1.0f;
    mfb_get_monitor_scale(window, &scale, NULL);
    int32_t viewport_x = (int32_t) (20 * scale);
    int32_t viewport_y = (int32_t) (20 * scale);
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
    x >>= 1;
    y >>= 1;
    g_positions[button].enabled = is_pressed;
    g_positions[button].x = x;
    g_positions[button].y = y;
    MFB_LOGI(LOG_TAG, "mouse_btn: button: id %d=%d, x=%d, y=%d (touch %d)", (int) button, (int) is_pressed, x, y, id);
}

//-------------------------------------
void
mouse_move(struct mfb_window *window, int x, int y) {
    int id;
    mfb_decode_touch(x, &x, &id);
    mfb_decode_touch(y, &y, NULL);
    x >>= 1;
    y >>= 1;
    g_positions[id].enabled = true;
    g_positions[id].x = x;
    g_positions[id].y = y;
    MFB_LOGI(LOG_TAG, "mouse_move: %d, %d [%d]", x, y, id);
}

//-------------------------------------
void
mouse_scroll(struct mfb_window *window, mfb_key_mod mod, float delta_x, float delta_y) {
    MFB_LOGI(LOG_TAG, "mouse_scroll:");
}

// I'm not sure that we can use this function name but it works
//-------------------------------------
int
main(int argc, char *argv[]) {
    kUnused(argc);
    kUnused(argv);
    uint32_t    i, x, y, noise, carry, seed = 0xbeef;

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

    float scale = 1.0f;
    mfb_get_monitor_scale(window, &scale, NULL);
    int32_t viewport_x = (int32_t) (20 * scale);
    int32_t viewport_y = (int32_t) (20 * scale);
    int32_t viewport_width  = g_width  - 2 * viewport_x;
    int32_t viewport_height = g_height - 2 * viewport_y;

    mfb_set_viewport(window, viewport_x, viewport_y, viewport_width, viewport_height);

    print_getters(window);

    g_buffer = (uint32_t *) malloc(g_width * g_height * 4);

    mfb_update_state state;
    do {
        if (!g_is_active) {
            mfb_wait_sync(window);
            continue;
        }

        int safe_left = 0, safe_top = 0, safe_right = 0, safe_bottom = 0;
        mfb_get_display_safe_insets(window, &safe_left, &safe_top, &safe_right, &safe_bottom);

        uint32_t inset_left   = (uint32_t) MAX(0, safe_left);
        uint32_t inset_top    = (uint32_t) MAX(0, safe_top);
        uint32_t inset_right  = (uint32_t) MAX(0, safe_right);
        uint32_t inset_bottom = (uint32_t) MAX(0, safe_bottom);

        inset_left   = MIN(inset_left, g_width);
        inset_right  = MIN(inset_right, g_width);
        inset_top    = MIN(inset_top, g_height);
        inset_bottom = MIN(inset_bottom, g_height);

        bool has_insets = (inset_left > 0) || (inset_top > 0) || (inset_right > 0) || (inset_bottom > 0);

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

                bool in_inset = has_insets && (
                    (x < inset_left) ||
                    (x >= (g_width - inset_right)) ||
                    (y < inset_top) ||
                    (y >= (g_height - inset_bottom))
                );

                g_buffer[i++] = in_inset ? MFB_RGB(noise, 0, 0) : MFB_RGB(noise, noise, noise);
            }
        }

        state = mfb_update_ex(window, g_buffer, g_width, g_height);
        if (state != MFB_STATE_OK) {
            window = NULL;
            break;
        }

        g_frame_count++;
    } while (mfb_wait_sync(window));
}
