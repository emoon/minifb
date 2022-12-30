#include <MiniFB.h>
#include <stdio.h>
#include <stdint.h>
#define _USE_MATH_DEFINES
#include <math.h>

#define kPI             3.14159265358979323846f
#define kUnused(var)    (void) var;

#define WIDTH_A      800
#define HEIGHT_A     600
static unsigned int g_buffer_a[WIDTH_A * HEIGHT_A];

#define WIDTH_B      320
#define HEIGHT_B     240
static unsigned int g_buffer_b[WIDTH_B * HEIGHT_B];

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void 
active(struct mfb_window *window, bool isActive) {
    const char *window_title = "";
    if(window) {
        window_title = (const char *) mfb_get_user_data(window);
    }
    fprintf(stdout, "%s > active: %d\n", window_title, isActive);
}

void 
resize(struct mfb_window *window, int width, int height) {
    const char *window_title = "";
    if(window) {
        window_title = (const char *) mfb_get_user_data(window);
    }

    fprintf(stdout, "%s > resize: %d, %d\n", window_title, width, height);
}

void 
keyboard(struct mfb_window *window, mfb_key key, mfb_key_mod mod, bool isPressed) {
    const char *window_title = "";
    if(window) {
        window_title = (const char *) mfb_get_user_data(window);
    }
    fprintf(stdout, "%s > keyboard: key: %s (pressed: %d) [key_mod: %x]\n", window_title, mfb_get_key_name(key), isPressed, mod);
    if(key == KB_KEY_ESCAPE) {
        mfb_close(window);
    }    
}

void 
char_input(struct mfb_window *window, unsigned int charCode) {
    const char *window_title = "";
    if(window) {
        window_title = (const char *) mfb_get_user_data(window);
    }
    fprintf(stdout, "%s > charCode: %d\n", window_title, charCode);
}

void 
mouse_btn(struct mfb_window *window, mfb_mouse_button button, mfb_key_mod mod, bool isPressed) {
    const char *window_title = "";
    if(window) {
        window_title = (const char *) mfb_get_user_data(window);
    }
    fprintf(stdout, "%s > mouse_btn: button: %d (pressed: %d) [key_mod: %x]\n", window_title, button, isPressed, mod);
}

void 
mouse_move(struct mfb_window *window, int x, int y) {
    kUnused(window);
    kUnused(x);
    kUnused(y);
    // const char *window_title = "";
    // if(window) {
    //     window_t(const char *) itle = mfb_get_user_data(window);
    // }
    //fprintf(stdout, "%s > mouse_move: %d, %d\n", window_title, x, y);
}

void 
mouse_scroll(struct mfb_window *window, mfb_key_mod mod, float deltaX, float deltaY) {
    const char *window_title = "";
    if(window) {
        window_title = (const char *) mfb_get_user_data(window);
    }
    fprintf(stdout, "%s > mouse_scroll: x: %f, y: %f [key_mod: %x]\n", window_title, deltaX, deltaY, mod);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int 
main()
{
    int noise, carry, seed = 0xbeef;

    struct mfb_window *window_a = mfb_open_ex("Multiple Windows Test", WIDTH_A, HEIGHT_A, WF_RESIZABLE);
    if (!window_a)
        return 0;

    mfb_set_active_callback(window_a, active);
    mfb_set_resize_callback(window_a, resize);
    mfb_set_keyboard_callback(window_a, keyboard);
    mfb_set_char_input_callback(window_a, char_input);
    mfb_set_mouse_button_callback(window_a, mouse_btn);
    mfb_set_mouse_move_callback(window_a, mouse_move);
    mfb_set_mouse_scroll_callback(window_a, mouse_scroll);

    mfb_set_user_data(window_a, (void *) "Window A");
    mfb_set_viewport(window_a, 25, 25, WIDTH_A-50, HEIGHT_A-50);

    //--
    struct mfb_window *window_b = mfb_open_ex("Secondary Window", WIDTH_B, HEIGHT_B, WF_RESIZABLE);
    if (!window_b)
        return 0;

    mfb_set_active_callback(window_b, active);
    mfb_set_resize_callback(window_b, resize);
    mfb_set_keyboard_callback(window_b, keyboard);
    mfb_set_char_input_callback(window_b, char_input);
    mfb_set_mouse_button_callback(window_b, mouse_btn);
    mfb_set_mouse_move_callback(window_b, mouse_move);
    mfb_set_mouse_scroll_callback(window_b, mouse_scroll);

    mfb_set_user_data(window_b, (void *) "Window B");

    // Generate pallete for plasma effect
    uint32_t    pallete[512];
    float       inc = 90.0f / 64.0f;
    for(uint32_t c=0; c<64; ++c) {
        int32_t col = (int32_t) ((255.0f * sinf(c * inc * kPI / 180.0f)) + 0.5f);
        pallete[64*0 + c] = MFB_ARGB(255, col,     0,       0);
        pallete[64*1 + c] = MFB_ARGB(255, 255,     col,     0);
        pallete[64*2 + c] = MFB_ARGB(255, 255-col, 255,     0);
        pallete[64*3 + c] = MFB_ARGB(255, 0,       255,     col);
        pallete[64*4 + c] = MFB_ARGB(255, 0,       255-col, 255);
        pallete[64*5 + c] = MFB_ARGB(255, col,     0,       255);
        pallete[64*6 + c] = MFB_ARGB(255, 255,     0,       255-col);
        pallete[64*7 + c] = MFB_ARGB(255, 255-col, 0,       0);
    }

    mfb_set_target_fps(10);

    //--
    float   time = 0;
    for (;;)
    {
        int      i, x, y;
        float    dx, dy, time_x, time_y;
        int      index;
        
        mfb_update_state state_a, state_b;

        if(window_a != 0x0) {
            for (i = 0; i < WIDTH_A * HEIGHT_A; ++i)
            {
                noise = seed;
                noise >>= 3;
                noise ^= seed;
                carry = noise & 1;
                noise >>= 1;
                seed >>= 1;
                seed |= (carry << 30);
                noise &= 0xFF;
                g_buffer_a[i] = MFB_ARGB(255, noise, noise, noise);
            }

            //--
            state_a = mfb_update(window_a, g_buffer_a);
            if (state_a != STATE_OK) {
                window_a = 0x0;
            }
        }

        //--
        if(window_b != 0x0) {
            time_x = sinf(time * kPI / 180.0f);
            time_y = cosf(time * kPI / 180.0f);
            i = 0;
            for(y=0; y<HEIGHT_B; ++y) {
                dy = cosf((y * time_y) * kPI / 180.0f);                // [-1, 1]
                for(x=0; x<WIDTH_B; ++x) {
                    dx = sinf((x * time_x) * kPI / 180.0f);            // [-1, 1]

                    index = (int) ((2.0f + dx + dy) * 0.25f * 511.0f);  // [0, 511]
                    g_buffer_b[i++] = pallete[index];
                }
            }
            time += 0.1f;

            //--
            state_b = mfb_update(window_b, g_buffer_b);
            if (state_b != STATE_OK) {
                window_b = 0x0;
            }
        }

        if(window_a == 0x0 && window_b == 0x0) {
            break;
        }

        // Don't need to do this for both windows in the same thread
        if(window_a != 0x0) {
            if(mfb_wait_sync(window_a) == false) {
                window_a = 0x0;
            }
        }
        else if(window_b != 0x0) {
            if(mfb_wait_sync(window_b) == false) {
                window_b = 0x0;
            }
        }
    }

    return 0;
}
