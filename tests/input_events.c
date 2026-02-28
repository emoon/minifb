#include <MiniFB.h>
#include <stdio.h>
#include <stdint.h>

//-------------------------------------
#define kUnused(var)    (void) var

//-------------------------------------
#define WIDTH      800
#define HEIGHT     600
static unsigned int g_buffer[WIDTH * HEIGHT];
static bool         g_active = true;
static unsigned int g_frame_count = 0;

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

    const char *key_name = mfb_get_key_name(KB_KEY_ESCAPE);
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
    fprintf(stdout, "  key_name(KB_KEY_ESCAPE): %s\n", key_name ? key_name : "(null)");
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
                key_buffer[KB_KEY_ESCAPE],
                key_buffer[KB_KEY_SPACE],
                key_buffer[KB_KEY_LEFT],
                key_buffer[KB_KEY_RIGHT],
                key_buffer[KB_KEY_UP],
                key_buffer[KB_KEY_DOWN]);
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
    uint32_t x = 0;
    uint32_t y = 0;
    const char *window_title = "";
    if (window) {
        window_title = (const char *) mfb_get_user_data(window);
    }

    fprintf(stdout, "%s > resize: %d, %d\n", window_title, width, height);
    if (width > WIDTH) {
        x = (width - WIDTH) >> 1;
        width = WIDTH;
    }
    if (height > HEIGHT) {
        y = (height - HEIGHT) >> 1;
        height = HEIGHT;
    }

    bool ok = mfb_set_viewport(window, x, y, width, height);
    fprintf(stdout, "%s > viewport: off(%u,%u) size(%d,%d) result=%d\n", window_title, x, y, width, height, ok ? 1 : 0);
}

//-------------------------------------
static bool
close(struct mfb_window *window) {
    const char* window_title = "";
    if (window) {
        window_title = (const char*)mfb_get_user_data(window);
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

    if (key == KB_KEY_SPACE) {
        print_getters(window);
    }

    if (key == KB_KEY_ESCAPE) {
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
    int x, y;
    if (window) {
        window_title = (const char *) mfb_get_user_data(window);
    }
    x = mfb_get_mouse_x(window);
    y = mfb_get_mouse_y(window);
    fprintf(stdout, "%s > mouse_btn: button: %d (pressed: %d) (at: %d, %d) [key_mod: %x]\n", window_title, button, is_pressed, x, y, mod);
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
    int noise, carry, seed = 0xbeef;

    struct mfb_window *window = mfb_open_ex("Input Events Test", WIDTH, HEIGHT, WF_RESIZABLE);
    if (!window) {
        return 0;
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

    mfb_set_viewport(window, 10, 10, WIDTH-20, HEIGHT-20);
    print_getters(window);

    do {
        int              i;
        mfb_update_state state;

        if (g_active) {
            for (i = 0; i < WIDTH * HEIGHT; ++i) {
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

        if (state != STATE_OK) {
            window = NULL;
            break;
        }

        g_frame_count++;
    } while(mfb_wait_sync(window));

    return 0;
}
