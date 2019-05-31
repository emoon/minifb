#include <MiniFB.h>
#include <stdio.h>
#include <stdint.h>

#define kUnused(var)    (void) var;

#define WIDTH 800
#define HEIGHT 600
static unsigned int s_buffer[WIDTH * HEIGHT];

//-------------------------------------
// C interface
//-------------------------------------
void active(struct Window *window, bool isActive) {
    kUnused(window);
    fprintf(stdout, "active: %d\n", isActive);
}

void resize(struct Window *window, int width, int height) {
    uint32_t x = 0;
    uint32_t y = 0;

    fprintf(stdout, "resize: %d, %d\n", width, height);
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
    fprintf(stdout, "keyboard: key: %s (pressed: %d) [KeyMod: %x]\n", mfb_get_key_name(key), isPressed, mod);
    if(key == KB_KEY_ESCAPE) {
        mfb_close(window);
    }    
}

void char_input(struct Window *window, unsigned int charCode) {
    kUnused(window);
    fprintf(stdout, "charCode: %d\n", charCode);
}

void mouse_btn(struct Window *window, MouseButton button, KeyMod mod, bool isPressed) {
    kUnused(window);
    fprintf(stdout, "mouse_btn: button: %d (pressed: %d) [KeyMod: %x]\n", button, isPressed, mod);
}

void mouse_move(struct Window *window, int x, int y) {
    kUnused(window);
    kUnused(x);
    kUnused(y);
    //fprintf(stdout, "mouse_move: %d, %d\n", x, y);
}

void mouse_scroll(struct Window *window, KeyMod mod, float deltaX, float deltaY) {
    kUnused(window);
    fprintf(stdout, "mouse_scroll: x: %f, y: %f [KeyMod: %x]\n", deltaX, deltaY, mod);
}

//-------------------------------------
// C++ interface (calling C functions)
//-------------------------------------
#if defined(__cplusplus)

class Events {
public:
    void active(struct Window *window, bool isActive) {
        ::active(window, isActive);
    }

    void resize(struct Window *window, int width, int height) {
        ::resize(window, width, height);
    }

    void keyboard(struct Window *window, Key key, KeyMod mod, bool isPressed) {
        ::keyboard(window, key, mod, isPressed);
    }

    void char_input(struct Window *window, unsigned int charCode) {
        ::char_input(window, charCode);
    }

    void mouse_btn(struct Window *window, MouseButton button, KeyMod mod, bool isPressed) {
        ::mouse_btn(window, button, mod, isPressed);
    }

    void mouse_move(struct Window *window, int x, int y) {
        ::mouse_move(window, x, y);
    }

    void mouse_scroll(struct Window *window, KeyMod mod, float deltaX, float deltaY) {
        ::mouse_scroll(window, mod, deltaX, deltaY);
    }
};

#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int main()
{
    int noise, carry, seed = 0xbeef;

#if defined(__cplusplus)

    Events e;

    mfb_active_callback(&e, &Events::active);
    mfb_resize_callback(&e, &Events::resize);
    mfb_keyboard_callback(&e, &Events::keyboard);
    mfb_char_input_callback(&e, &Events::char_input);
    mfb_mouse_button_callback(&e, &Events::mouse_btn);
    mfb_mouse_move_callback(&e, &Events::mouse_move);
    mfb_mouse_scroll_callback(&e, &Events::mouse_scroll);

#else

    mfb_active_callback(active);
    mfb_resize_callback(resize);
    mfb_keyboard_callback(keyboard);
    mfb_char_input_callback(char_input);
    mfb_mouse_button_callback(mouse_btn);
    mfb_mouse_move_callback(mouse_move);
    mfb_mouse_scroll_callback(mouse_scroll);

#endif

    struct Window *window = mfb_open_ex("Noise Test", WIDTH, HEIGHT, WF_RESIZABLE);
    if (!window)
        return 0;

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
            s_buffer[i] = MFB_RGB(noise, noise, noise); 
        }

        state = mfb_update(window, s_buffer);

        if (state != STATE_OK)
            break;
    }

    mfb_close(window);

    return 0;
}
