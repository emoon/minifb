#include <MiniFB.h>
#include <stdio.h>
#include <stdint.h>

#define kUnused(var)    (void) var;

#define WIDTH_A      800
#define HEIGHT_A     600
static unsigned int g_buffer1[WIDTH_A * HEIGHT_A];

#define WIDTH_B      240
#define HEIGHT_B     120
static unsigned int g_buffer2[WIDTH_B * HEIGHT_B];

//-------------------------------------
// C interface
//-------------------------------------
void active(struct Window *window, bool isActive) {
    int id = 0;
    if(window) {
        id = *(int *) mfb_get_user_data(window);
    }
    fprintf(stdout, "active %d: %d\n", id, isActive);
}

void resize(struct Window *window, int width, int height) {
    uint32_t x = 0;
    uint32_t y = 0;
    int id = 0;
    if(window) {
        id = *(int *) mfb_get_user_data(window);
    }

    fprintf(stdout, "resize %d: %d, %d\n", id, width, height);
    if(width > WIDTH_A) {
        x = (width - WIDTH_A) >> 1;
        width = WIDTH_A;
    }
    if(height > HEIGHT_A) {
        y = (height - HEIGHT_A) >> 1;
        height = HEIGHT_A;
    }
    mfb_set_viewport(window, x, y, width, height);
}

void keyboard(struct Window *window, Key key, KeyMod mod, bool isPressed) {
    int id = 0;
    if(window) {
        id = *(int *) mfb_get_user_data(window);
    }
    fprintf(stdout, "keyboard %d: key: %s (pressed: %d) [KeyMod: %x]\n", id, mfb_get_key_name(key), isPressed, mod);
    if(key == KB_KEY_ESCAPE) {
        mfb_close(window);
    }    
}

void char_input(struct Window *window, unsigned int charCode) {
    int id = 0;
    if(window) {
        id = *(int *) mfb_get_user_data(window);
    }
    fprintf(stdout, "charCode %d: %d\n", id, charCode);
}

void mouse_btn(struct Window *window, MouseButton button, KeyMod mod, bool isPressed) {
    int id = 0;
    if(window) {
        id = *(int *) mfb_get_user_data(window);
    }
    fprintf(stdout, "mouse_btn %d: button: %d (pressed: %d) [KeyMod: %x]\n", id, button, isPressed, mod);
}

void mouse_move(struct Window *window, int x, int y) {
    kUnused(window);
    kUnused(x);
    kUnused(y);
    // int id = 0;
    // if(window) {
    //     id = *(int *) mfb_get_user_data(window);
    // }
    //fprintf(stdout, "mouse_move %d: %d, %d\n", id, x, y);
}

void mouse_scroll(struct Window *window, KeyMod mod, float deltaX, float deltaY) {
    int id = 0;
    if(window) {
        id = *(int *) mfb_get_user_data(window);
    }
    fprintf(stdout, "mouse_scroll %d: x: %f, y: %f [KeyMod: %x]\n", id, deltaX, deltaY, mod);
}

//--
void active2(struct Window *window, bool isActive) {
    kUnused(window);
    fprintf(stdout, "active 2: %d\n", isActive);
}

void resize2(struct Window *window, int width, int height) {
    uint32_t x = 0;
    uint32_t y = 0;

    fprintf(stdout, "resize 2: %d, %d\n", width, height);
    if(width > WIDTH_A) {
        x = (width - WIDTH_A) >> 1;
        width = WIDTH_A;
    }
    if(height > HEIGHT_A) {
        y = (height - HEIGHT_A) >> 1;
        height = HEIGHT_A;
    }
    mfb_set_viewport(window, x, y, width, height);
}

void keyboard2(struct Window *window, Key key, KeyMod mod, bool isPressed) {
    fprintf(stdout, "keyboard 2: key: %s (pressed: %d) [KeyMod: %x]\n", mfb_get_key_name(key), isPressed, mod);
    if(key == KB_KEY_ESCAPE) {
        mfb_close(window);
    }    
}

void char_input2(struct Window *window, unsigned int charCode) {
    kUnused(window);
    fprintf(stdout, "charCode 2: %d\n", charCode);
}

void mouse_btn2(struct Window *window, MouseButton button, KeyMod mod, bool isPressed) {
    kUnused(window);
    fprintf(stdout, "mouse_btn 2: button: %d (pressed: %d) [KeyMod: %x]\n", button, isPressed, mod);
}

void mouse_move2(struct Window *window, int x, int y) {
    kUnused(window);
    kUnused(x);
    kUnused(y);
    //fprintf(stdout, "mouse_move: %d, %d\n", x, y);
}

void mouse_scroll2(struct Window *window, KeyMod mod, float deltaX, float deltaY) {
    kUnused(window);
    fprintf(stdout, "mouse_scroll 2: x: %f, y: %f [KeyMod: %x]\n", deltaX, deltaY, mod);
}

//-------------------------------------
// C++ interface (calling C functions)
//-------------------------------------
#if defined(__cplusplus)

class Events {
public:
    void active(struct Window *window, bool isActive) {
        printf("\nEvents 1 - ");
        ::active(window, isActive);
    }

    void resize(struct Window *window, int width, int height) {
        printf("Events 1 - ");
        ::resize(window, width, height);
    }

    void keyboard(struct Window *window, Key key, KeyMod mod, bool isPressed) {
        printf("Events 1 - ");
        ::keyboard(window, key, mod, isPressed);
    }

    void char_input(struct Window *window, unsigned int charCode) {
        printf("Events 1 - ");
        ::char_input(window, charCode);
    }

    void mouse_btn(struct Window *window, MouseButton button, KeyMod mod, bool isPressed) {
        printf("Events 1 - ");
        ::mouse_btn(window, button, mod, isPressed);
    }

    void mouse_move(struct Window *window, int x, int y) {
        //printf("Events 1 - ");
        ::mouse_move(window, x, y);
    }

    void mouse_scroll(struct Window *window, KeyMod mod, float deltaX, float deltaY) {
        printf("Events 1 - ");
        ::mouse_scroll(window, mod, deltaX, deltaY);
    }
};

class Events2 {
public:
    void active(struct Window *window, bool isActive) {
        printf("\nEvents 2 - ");
        ::active(window, isActive);
    }

    void resize(struct Window *window, int width, int height) {
        printf("Events 2 - ");
        ::resize(window, width, height);
    }

    void keyboard(struct Window *window, Key key, KeyMod mod, bool isPressed) {
        printf("Events 2 - ");
        ::keyboard(window, key, mod, isPressed);
    }

    void char_input(struct Window *window, unsigned int charCode) {
        printf("Events 2 - ");
        ::char_input(window, charCode);
    }

    void mouse_btn(struct Window *window, MouseButton button, KeyMod mod, bool isPressed) {
        printf("Events 2 - ");
        ::mouse_btn(window, button, mod, isPressed);
    }

    void mouse_move(struct Window *window, int x, int y) {
        //printf("Events 2 - ");
        ::mouse_move(window, x, y);
    }

    void mouse_scroll(struct Window *window, KeyMod mod, float deltaX, float deltaY) {
        printf("Events 2 - ");
        ::mouse_scroll(window, mod, deltaX, deltaY);
    }
};

#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int main()
{
    int noise, carry, seed = 0xbeef;

    struct Window *window1 = mfb_open_ex("Noise Test", WIDTH_A, HEIGHT_A, WF_RESIZABLE);
    if (!window1)
        return 0;

#if defined(__cplusplus)

    Events e;

    mfb_active_callback(window1, &e, &Events::active);
    mfb_resize_callback(window1, &e, &Events::resize);
    mfb_keyboard_callback(window1, &e, &Events::keyboard);
    mfb_char_input_callback(window1, &e, &Events::char_input);
    mfb_mouse_button_callback(window1, &e, &Events::mouse_btn);
    mfb_mouse_move_callback(window1, &e, &Events::mouse_move);
    mfb_mouse_scroll_callback(window1, &e, &Events::mouse_scroll);

#else

    mfb_active_callback(window1, active);
    mfb_resize_callback(window1, resize);
    mfb_keyboard_callback(window1, keyboard);
    mfb_char_input_callback(window1, char_input);
    mfb_mouse_button_callback(window1, mouse_btn);
    mfb_mouse_move_callback(window1, mouse_move);
    mfb_mouse_scroll_callback(window1, mouse_scroll);

#endif

    struct Window *window2 = mfb_open_ex("Noise Test", WIDTH_B, HEIGHT_B, WF_RESIZABLE);
    if (!window2)
        return 0;

#if defined(__cplusplus)

    Events2 e2;

    mfb_active_callback(window2, &e2, &Events2::active);
    // mfb_resize_callback(window2, &e2, &Events::resize);
    // mfb_keyboard_callback(window2, &e2, &Events::keyboard);
    // mfb_char_input_callback(window2, &e2, &Events::char_input);
    // mfb_mouse_button_callback(window2, &e2, &Events::mouse_btn);
    // mfb_mouse_move_callback(window2, &e2, &Events::mouse_move);
    // mfb_mouse_scroll_callback(window2, &e2, &Events::mouse_scroll);

#else

    mfb_active_callback(window2, active2);
    mfb_resize_callback(window2, resize2);
    mfb_keyboard_callback(window2, keyboard2);
    mfb_char_input_callback(window2, char_input2);
    mfb_mouse_button_callback(window2, mouse_btn2);
    mfb_mouse_move_callback(window2, mouse_move2);
    mfb_mouse_scroll_callback(window2, mouse_scroll2);

#endif

    int id1 = 1, id2 = 2;
    mfb_set_user_data(window1, &id1);
    mfb_set_user_data(window2, &id2);

    for (;;)
    {
        int         i;
        UpdateState state1, state2;

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
            g_buffer1[i] = MFB_RGB(noise, noise, noise); 
        }

        for (i = 0; i < WIDTH_B * HEIGHT_B; ++i)
        {
            noise = seed;
            noise >>= 3;
            noise ^= seed;
            carry = noise & 1;
            noise >>= 1;
            seed >>= 1;
            seed |= (carry << 30);
            noise &= 0xFF;
            g_buffer2[i] = MFB_RGB(noise, (~noise) & 0xff, 255 - noise); 
        }

        state1 = mfb_update(window1, g_buffer1);
        state2 = mfb_update(window2, g_buffer2);
        if (state1 != STATE_OK) {
            window1 = 0x0;
        }
        if (state2 != STATE_OK) {
            window2 = 0x0;
        }
        if (state1 != STATE_OK && state2 != STATE_OK) {
            break;
        }
    }

    mfb_close(window1);

    return 0;
}
