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

void active(struct Window *window, bool isActive) {
    const char *window_title = "";
    if(window) {
        window_title = (const char *) mfb_get_user_data(window);
    }
    fprintf(stdout, "%s > active: %d\n", window_title, isActive);
}

void resize(struct Window *window, int width, int height) {
    const char *window_title = "";
    if(window) {
        window_title = (const char *) mfb_get_user_data(window);
    }

    fprintf(stdout, "%s > resize: %d, %d\n", window_title, width, height);
}

void keyboard(struct Window *window, Key key, KeyMod mod, bool isPressed) {
    const char *window_title = "";
    if(window) {
        window_title = (const char *) mfb_get_user_data(window);
    }
    fprintf(stdout, "%s > keyboard: key: %s (pressed: %d) [KeyMod: %x]\n", window_title, mfb_get_key_name(key), isPressed, mod);
    if(key == KB_KEY_ESCAPE) {
        mfb_close(window);
    }    
}

void char_input(struct Window *window, unsigned int charCode) {
    const char *window_title = "";
    if(window) {
        window_title = (const char *) mfb_get_user_data(window);
    }
    fprintf(stdout, "%s > charCode: %d\n", window_title, charCode);
}

void mouse_btn(struct Window *window, MouseButton button, KeyMod mod, bool isPressed) {
    const char *window_title = "";
    if(window) {
        window_title = (const char *) mfb_get_user_data(window);
    }
    fprintf(stdout, "%s > mouse_btn: button: %d (pressed: %d) [KeyMod: %x]\n", window_title, button, isPressed, mod);
}

void mouse_move(struct Window *window, int x, int y) {
    kUnused(window);
    kUnused(x);
    kUnused(y);
    // const char *window_title = "";
    // if(window) {
    //     window_t(const char *) itle = mfb_get_user_data(window);
    // }
    //fprintf(stdout, "%s > mouse_move: %d, %d\n", window_title, x, y);
}

void mouse_scroll(struct Window *window, KeyMod mod, float deltaX, float deltaY) {
    const char *window_title = "";
    if(window) {
        window_title = (const char *) mfb_get_user_data(window);
    }
    fprintf(stdout, "%s > mouse_scroll: x: %f, y: %f [KeyMod: %x]\n", window_title, deltaX, deltaY, mod);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int main()
{
    int noise, carry, seed = 0xbeef;

    struct Window *window_a = mfb_open_ex("Multiple Windows Test", WIDTH_A, HEIGHT_A, WF_RESIZABLE);
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

    //--
    struct Window *window_b = mfb_open_ex("Secondary Window", WIDTH_B, HEIGHT_B, WF_RESIZABLE);
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
        pallete[64*0 + c] = MFB_RGB(col,     0,       0);
        pallete[64*1 + c] = MFB_RGB(255,     col,     0);
        pallete[64*2 + c] = MFB_RGB(255-col, 255,     0);
        pallete[64*3 + c] = MFB_RGB(0,       255,     col);
        pallete[64*4 + c] = MFB_RGB(0,       255-col, 255);
        pallete[64*5 + c] = MFB_RGB(col,     0,       255);
        pallete[64*6 + c] = MFB_RGB(255,     0,       255-col);
        pallete[64*7 + c] = MFB_RGB(255-col, 0,       0);
    }

    //--
    float   time = 0;
    for (;;)
    {
        int         i, x, y;
        float       dx, dy, time_x, time_y;
        int         index;
        UpdateState state_a, state_b;

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
            g_buffer_a[i] = MFB_RGB(noise, noise, noise); 
        }

        //--
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
        state_a = mfb_update(window_a, g_buffer_a);
        if (state_a != STATE_OK) {
            window_a = 0x0;
        }

        //--
        state_b = mfb_update(window_b, g_buffer_b);
        if (state_b != STATE_OK) {
            window_b = 0x0;
        }

        if(window_a == 0x0 && window_b == 0x0) {
            break;
        }
    }

    return 0;
}
