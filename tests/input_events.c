#include <MiniFB.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

//-------------------------------------
#define kUnused(var)    (void) var

//-------------------------------------
static uint32_t g_width       = 800;
static uint32_t g_height      = 600;
static uint32_t *g_buffer     = NULL;
static bool     g_active      = true;
static uint32_t g_frame_count = 0;
// Convert phisical pixels to logical pixels
static float    g_to_logic_width;
static float    g_to_logic_height;

//-------------------------------------
static void
print_getters(struct mfb_window *window) {
    if (!window) {
        return;
    }

    unsigned win_w = 0, win_h = 0;
    unsigned win_sw = 0, win_sh = 0;
    unsigned draw_off_x = 0, draw_off_y = 0, draw_w = 0, draw_h = 0;
    unsigned bounds_off_x = 0, bounds_off_y = 0, bounds_w = 0, bounds_h = 0;
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
        fprintf(stderr, "Width does not match: %u != %u\n", win_w, win_sw);
    }
    if (win_h != win_sh) {
        fprintf(stderr, "Height does not match: %u != %u\n", win_h, win_sh);
    }

    draw_off_x = mfb_get_drawable_offset_x(window);
    draw_off_y = mfb_get_drawable_offset_y(window);
    draw_w = mfb_get_drawable_width(window);
    draw_h = mfb_get_drawable_height(window);
    mfb_get_drawable_bounds(window, &bounds_off_x, &bounds_off_y, &bounds_w, &bounds_h);

    mouse_x = mfb_get_mouse_x(window);
    mouse_y = mfb_get_mouse_y(window);
    scroll_x = mfb_get_mouse_scroll_x(window);
    scroll_y = mfb_get_mouse_scroll_y(window);
    const uint8_t *mouse_buttons = mfb_get_mouse_button_buffer(window);

    mfb_get_monitor_scale(window, &scale_x, &scale_y);
    fps = mfb_get_target_fps();

    fprintf(stdout, "[getters frame=%d]\n", g_frame_count);
    fprintf(stdout, "  key_name(MFB_KB_KEY_ESCAPE): %s\n", key_name ? key_name : "(null)");
    fprintf(stdout, "  is_window_active: %d\n", is_active);
    fprintf(stdout, "  target_fps: %u\n", fps);
    fprintf(stdout, "  monitor_scale: %f, %f\n", scale_x, scale_y);
    fprintf(stdout, "  window_size: %u x %u\n", win_w, win_h);
    fprintf(stdout, "  drawable_offsets: %u, %u\n", draw_off_x, draw_off_y);
    fprintf(stdout, "  drawable_size: %u x %u\n", draw_w, draw_h);
    fprintf(stdout, "  drawable_bounds: off(%u,%u) size(%u,%u)\n", bounds_off_x, bounds_off_y, bounds_w, bounds_h);
    fprintf(stdout, "  mouse_pos: %d, %d\n", mouse_x, mouse_y);
    fprintf(stdout, "  mouse_scroll: %f, %f\n", scroll_x, scroll_y);

    if (mouse_buttons) {
        fprintf(stdout, "  mouse_buttons[0..7]: %u %u %u %u %u %u %u %u\n",
                mouse_buttons[0], mouse_buttons[1], mouse_buttons[2], mouse_buttons[3],
                mouse_buttons[4], mouse_buttons[5], mouse_buttons[6], mouse_buttons[7]);
    }
    else {
        fprintf(stdout, "  mouse_buttons: (null)\n");
    }

    if (key_buffer) {
        fprintf(stdout, "  key_buffer sample [ESC=%u, SPACE=%u, LEFT=%u, RIGHT=%u, UP=%u, DOWN=%u]\n",
                key_buffer[MFB_KB_KEY_ESCAPE],
                key_buffer[MFB_KB_KEY_SPACE],
                key_buffer[MFB_KB_KEY_LEFT],
                key_buffer[MFB_KB_KEY_RIGHT],
                key_buffer[MFB_KB_KEY_UP],
                key_buffer[MFB_KB_KEY_DOWN]);
    }
    else {
        fprintf(stdout, "  key_buffer: (null)\n");
    }
}

//-------------------------------------
static void
active(struct mfb_window *window, bool is_active) {
    const char *window_title = "";
    if (window) {
        window_title = (const char *) mfb_get_user_data(window);
    }
    fprintf(stdout, "%s > active: %d\n", window_title, is_active);
    g_active = is_active;
}

//-------------------------------------
static void
resize(struct mfb_window *window, int width, int height) {
    const char *window_title = "";
    if (window) {
        window_title = (const char *) mfb_get_user_data(window);
    }

    fprintf(stdout, "%s > resize: %d, %d\n", window_title, width, height);

    // Convert phisical pixels to logical pixels
    g_to_logic_width  = 1.0f / width;
    g_to_logic_height = 1.0f / height;
}

//-------------------------------------
static bool
close(struct mfb_window *window) {
    const char *window_title = "";
    if (window) {
        window_title = (const char *) mfb_get_user_data(window);
    }
    fprintf(stdout, "%s > close\n", window_title);
    return true;    // true => confirm close
                    // false => don't close
}

//-------------------------------------
static void
keyboard(struct mfb_window *window, mfb_key key, mfb_key_mod mod, bool is_pressed) {
    const char *window_title = "";
    if (window) {
        window_title = (const char *) mfb_get_user_data(window);
    }
    fprintf(stdout, "%s > keyboard: key: %s (pressed: %d) [key_mod: %x]\n", window_title, mfb_get_key_name(key), is_pressed, mod);

    if (key == MFB_KB_KEY_SPACE) {
        print_getters(window);
    }

    if (key == MFB_KB_KEY_LEFT_CONTROL) {
        mfb_show_cursor(window, true);
    }

    if (key == MFB_KB_KEY_RIGHT_CONTROL) {
        mfb_show_cursor(window, false);
    }

    if (key == MFB_KB_KEY_ESCAPE) {
        mfb_close(window);
    }
}

//-------------------------------------
static void
char_input(struct mfb_window *window, unsigned int char_code) {
    const char *window_title = "";
    if (window) {
        window_title = (const char *) mfb_get_user_data(window);
    }
    fprintf(stdout, "%s > char_code: %d\n", window_title, char_code);
}

//-------------------------------------
static void
mouse_btn(struct mfb_window *window, mfb_mouse_button button, mfb_key_mod mod, bool is_pressed) {
    const char *window_title = "";
    int x, y, xl, yl;
    if (window) {
        window_title = (const char *) mfb_get_user_data(window);
    }

    x = mfb_get_mouse_x(window);
    y = mfb_get_mouse_y(window);
    xl = x * (g_width  * g_to_logic_width);
    yl = y * (g_height * g_to_logic_height);
    fprintf(stdout, "%s > mouse_btn: button: %d (pressed: %d) (at: [phys: %d, %d] [log: %d, %d]) [key_mod: %x]\n",
            window_title,
            button,
            is_pressed,
            x, y,
            xl, yl,
            mod);
}

//-------------------------------------
static void
mouse_move(struct mfb_window *window, int x, int y) {
    kUnused(window);
    kUnused(x);
    kUnused(y);
    //fprintf(stdout, "mouse_move: %d, %d\n", x, y);
}

//-------------------------------------
static void
mouse_scroll(struct mfb_window *window, mfb_key_mod mod, float delta_x, float delta_y) {
    const char *window_title = "";
    if (window) {
        window_title = (const char *) mfb_get_user_data(window);
    }
    fprintf(stdout, "%s > mouse_scroll: x: %f, y: %f [key_mod: %x]\n", window_title, delta_x, delta_y, mod);
}

//-------------------------------------
int
main() {
    int32_t noise, carry, seed = 0xbeef;

    g_buffer = (uint32_t *) malloc(g_width * g_height * sizeof(uint32_t));
    if (!g_buffer) {
        fprintf(stderr, "malloc failed (%u x %u)\n", g_width, g_height);
        return -1;
    }

    // Convert phisical pixels to logical pixels
    g_to_logic_width  = 1.0f / g_width;
    g_to_logic_height = 1.0f / g_height;

    mfb_set_log_level(MFB_LOG_TRACE);

    struct mfb_window *window = mfb_open_ex("Input Events Test", g_width, g_height, MFB_WF_RESIZABLE);
    if (!window) {
        return -1;
    }

    mfb_set_active_callback(window, active);
    mfb_set_resize_callback(window, resize);
    mfb_set_close_callback(window, close);
    mfb_set_keyboard_callback(window, keyboard);
    mfb_set_char_input_callback(window, char_input);
    mfb_set_mouse_button_callback(window, mouse_btn);
    mfb_set_mouse_move_callback(window, mouse_move);
    mfb_set_mouse_scroll_callback(window, mouse_scroll);

    mfb_set_user_data(window, (void *) "Input Events Test");

    mfb_set_viewport(window, 10, 10, g_width-20, g_height-20);
    print_getters(window);

    uint32_t         i;
    mfb_update_state state;

    do {
        if (g_active) {
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

            state = mfb_update(window, g_buffer);
        }
        else {
            state = mfb_update_events(window);
        }

        if (state != MFB_STATE_OK) {
            window = NULL;
            break;
        }

        g_frame_count++;
    } while (mfb_wait_sync(window));

    return 0;
}
