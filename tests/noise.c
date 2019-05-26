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
void active(void *user_data, bool isActive) {
    kUnused(user_data);
    fprintf(stdout, "active: %d\n", isActive);
}

void resize(void *user_data, int width, int height) {
    uint32_t x = 0;
    uint32_t y = 0;

    kUnused(user_data);
    fprintf(stdout, "resize: %d, %d\n", width, height);
    if(width > WIDTH) {
        x = (width - WIDTH) >> 1;
        width = WIDTH;
    }
    if(height > HEIGHT) {
        y = (height - HEIGHT) >> 1;
        height = HEIGHT;
    }
    mfb_set_viewport(x, y, width, height);
}

void keyboard(void *user_data, Key key, KeyMod mod, bool isPressed) {
    kUnused(user_data);
    fprintf(stdout, "keyboard: key: %d (pressed: %d) [KeyMod: %x]\n", key, isPressed, mod);
    if(key == KB_KEY_ESCAPE) {
        mfb_close();
    }    
}

void char_input(void *user_data, unsigned int charCode) {
    kUnused(user_data);
    fprintf(stdout, "charCode: %d\n", charCode);
}

void mouse_btn(void *user_data, MouseButton button, KeyMod mod, bool isPressed) {
    kUnused(user_data);
    fprintf(stdout, "mouse_btn: button: %d (pressed: %d) [KeyMod: %x]\n", button, isPressed, mod);
}

void mouse_move(void *user_data, int x, int y) {
    kUnused(user_data);
    kUnused(x);
    kUnused(y);
    //fprintf(stdout, "mouse_move: %d, %d\n", x, y);
}

void mouse_scroll(void *user_data, KeyMod mod, float deltaX, float deltaY) {
    kUnused(user_data);
    fprintf(stdout, "mouse_scroll: x: %f, y: %f [KeyMod: %x]\n", deltaX, deltaY, mod);
}

//-------------------------------------
// C++ interface (calling C functions)
//-------------------------------------
#if defined(__cplusplus)

class Events {
public:
    void active(void *user_data, bool isActive) {
        ::active(user_data, isActive);
    }

    void resize(void *user_data, int width, int height) {
        ::resize(user_data, width, height);
    }

    void keyboard(void *user_data, Key key, KeyMod mod, bool isPressed) {
        ::keyboard(user_data, key, mod, isPressed);
    }

    void char_input(void *user_data, unsigned int charCode) {
        ::char_input(user_data, charCode);
    }

    void mouse_btn(void *user_data, MouseButton button, KeyMod mod, bool isPressed) {
        ::mouse_btn(user_data, button, mod, isPressed);
    }

    void mouse_move(void *user_data, int x, int y) {
        ::mouse_move(user_data, x, y);
    }

    void mouse_scroll(void *user_data, KeyMod mod, float deltaX, float deltaY) {
        ::mouse_scroll(user_data, mod, deltaX, deltaY);
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

    if (!mfb_open_ex("Noise Test", WIDTH, HEIGHT, WF_RESIZABLE))
        return 0;

    for (;;)
    {
        int i, state;

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

        state = mfb_update(s_buffer);

        if (state < 0)
            break;
    }

    mfb_close();

    return 0;
}
