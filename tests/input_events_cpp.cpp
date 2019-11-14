#include <MiniFB.h>
#include <stdio.h>
#include <stdint.h>

#define kUnused(var)    (void) var;

#define WIDTH      800
#define HEIGHT     600
static unsigned int g_buffer[WIDTH * HEIGHT];

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class Events {
public:
    void active(struct Window *window, bool isActive) {
        const char *window_title = "";
        if(window) {
            window_title = (const char *) mfb_get_user_data(window);
        }
        fprintf(stdout, "%s > active: %d\n", window_title, isActive);
    }

    void resize(struct Window *window, int width, int height) {
        uint32_t x = 0;
        uint32_t y = 0;
        const char *window_title = "";
        if(window) {
            window_title = (const char *) mfb_get_user_data(window);
        }

        fprintf(stdout, "%s > resize: %d, %d\n", window_title, width, height);
        if(width > WIDTH) {
            x = (width - WIDTH) >> 1;
            width = WIDTH;
        }
        if(height > HEIGHT) {
            y = (height - HEIGHT) >> 1;
            height = HEIGHT;
        }
        mfb_set_viewport(window, x, y, width, height);
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
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int main()
{
    int noise, carry, seed = 0xbeef;

    struct Window *window = mfb_open_ex("Input Events CPP Test", WIDTH, HEIGHT, WF_RESIZABLE);
    if (!window)
        return 0;

    Events e;

    mfb_set_active_callback(window, &e, &Events::active);
    mfb_set_resize_callback(window, &e, &Events::resize);
    mfb_set_keyboard_callback(window, &e, &Events::keyboard);
    mfb_set_char_input_callback(window, &e, &Events::char_input);
    mfb_set_mouse_button_callback(window, &e, &Events::mouse_btn);
    mfb_set_mouse_move_callback(window, &e, &Events::mouse_move);
    mfb_set_mouse_scroll_callback(window, &e, &Events::mouse_scroll);

    mfb_set_user_data(window, (void *) "Input Events CPP Test");

    for (;;)
    {
        int         i;
        UpdateState state;

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
        if (state != STATE_OK) {
            window = 0x0;
            break;
        }
    }

    return 0;
}
