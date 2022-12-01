#include <MiniFB.h>
#include <stdio.h>
#include <stdint.h>

#define kUnused(var)    (void) var;

#define WIDTH      800
#define HEIGHT     600
static unsigned int g_buffer[WIDTH * HEIGHT];

//#define kUseOldFunctions
//#define kUseLambdas

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class Events {
public:
    void active(struct mfb_window *window, bool isActive) {
        const char *window_title = "";
        if(window) {
            window_title = (const char *) mfb_get_user_data(window);
        }
        fprintf(stdout, "%s > active: %d\n", window_title, isActive);
    }

    void resize(struct mfb_window *window, int width, int height) {
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

    bool close(struct mfb_window *window) {
        const char* window_title = "";
        if (window) {
            window_title = (const char*) mfb_get_user_data(window);
        }
        fprintf(stdout, "%s > close\n", window_title);
        return true;    // true => confirm close
                        // false => don't close
    }

    void keyboard(struct mfb_window *window, mfb_key key, mfb_key_mod mod, bool isPressed) {
        const char *window_title = "";
        if(window) {
            window_title = (const char *) mfb_get_user_data(window);
        }
        fprintf(stdout, "%s > keyboard: key: %s (pressed: %d) [key_mod: %x]\n", window_title, mfb_get_key_name(key), isPressed, mod);
        if(key == KB_KEY_ESCAPE) {
            mfb_close(window);
        }
    }

    void char_input(struct mfb_window *window, unsigned int charCode) {
        const char *window_title = "";
        if(window) {
            window_title = (const char *) mfb_get_user_data(window);
        }
        fprintf(stdout, "%s > charCode: %d\n", window_title, charCode);
    }

    void mouse_button(struct mfb_window *window, mfb_mouse_button button, mfb_key_mod mod, bool isPressed) {
        const char  *window_title = "";
        int         x, y;

        if(window) {
            window_title = (const char *) mfb_get_user_data(window);
        }
        x = mfb_get_mouse_x(window);
        y = mfb_get_mouse_y(window);
        fprintf(stdout, "%s > mouse_button: button: %d (pressed: %d) (at: %d, %d) [key_mod: %x]\n", window_title, button, isPressed, x, y, mod);
    }

    void mouse_move(struct mfb_window *window, int x, int y) {
        kUnused(window);
        kUnused(x);
        kUnused(y);
        //const char *window_title = "";
        //if(window) {
        //    window_title = (const char *) mfb_get_user_data(window);
        //}
        //fprintf(stdout, "%s > mouse_move: %d, %d\n", window_title, x, y);
    }

    void mouse_scroll(struct mfb_window *window, mfb_key_mod mod, float deltaX, float deltaY) {
        const char *window_title = "";
        if(window) {
            window_title = (const char *) mfb_get_user_data(window);
        }
        fprintf(stdout, "%s > mouse_scroll: x: %f, y: %f [key_mod: %x]\n", window_title, deltaX, deltaY, mod);
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int
main()
{
    int noise, carry, seed = 0xbeef;

    struct mfb_window *window = mfb_open_ex("Input Events CPP Test", WIDTH, HEIGHT, WF_RESIZABLE);
    if (!window)
        return 0;

    Events e;

#if defined(kUseOldFunctions)

    mfb_set_active_callback(window, &e, &Events::active);
    mfb_set_resize_callback(window, &e, &Events::resize);
    mfb_set_close_callback(window, &e, &Events::close);
    mfb_set_keyboard_callback(window, &e, &Events::keyboard);
    mfb_set_char_input_callback(window, &e, &Events::char_input);
    mfb_set_mouse_button_callback(window, &e, &Events::mouse_button);
    mfb_set_mouse_move_callback(window, &e, &Events::mouse_move);
    mfb_set_mouse_scroll_callback(window, &e, &Events::mouse_scroll);

#elif defined(kUseLambdas)

    mfb_set_active_callback([](struct mfb_window *window, bool isActive) {
        const char *window_title = "";
        if(window) {
            window_title = (const char *) mfb_get_user_data(window);
        }
        fprintf(stdout, "%s > active: %d (lambda)\n", window_title, isActive);
    }, window);

    mfb_set_resize_callback([](struct mfb_window *window, int width, int height) {
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
    }, window);

    mfb_set_close_callback([](struct mfb_window *window) {
        const char* window_title = "";
        if (window) {
            window_title = (const char*) mfb_get_user_data(window);
        }
        fprintf(stdout, "%s > close\n", window_title);
        return true;    // true => confirm close
                        // false => don't close
    }, window);

    mfb_set_keyboard_callback([](struct mfb_window *window, mfb_key key, mfb_key_mod mod, bool isPressed) {
        const char *window_title = "";
        if(window) {
            window_title = (const char *) mfb_get_user_data(window);
        }
        fprintf(stdout, "%s > keyboard: key: %s (pressed: %d) [key_mod: %x]\n", window_title, mfb_get_key_name(key), isPressed, mod);
        if(key == KB_KEY_ESCAPE) {
            mfb_close(window);
        }
    }, window);

    mfb_set_char_input_callback([](struct mfb_window *window, unsigned int charCode) {
        const char *window_title = "";
        if(window) {
            window_title = (const char *) mfb_get_user_data(window);
        }
        fprintf(stdout, "%s > charCode: %d\n", window_title, charCode);
    }, window);

    mfb_set_mouse_button_callback([](struct mfb_window *window, mfb_mouse_button button, mfb_key_mod mod, bool isPressed) {
        const char  *window_title = "";
        int         x, y;

        if(window) {
            window_title = (const char *) mfb_get_user_data(window);
        }
        x = mfb_get_mouse_x(window);
        y = mfb_get_mouse_y(window);
        fprintf(stdout, "%s > mouse_button: button: %d (pressed: %d) (at: %d, %d) [key_mod: %x]\n", window_title, button, isPressed, x, y, mod);
    }, window);

    mfb_set_mouse_move_callback([](struct mfb_window *window, int x, int y) {
        kUnused(window);
        kUnused(x);
        kUnused(y);
        //const char *window_title = "";
        //if(window) {
        //    window_title = (const char *) mfb_get_user_data(window);
        //}
        //fprintf(stdout, "%s > mouse_move: %d, %d\n", window_title, x, y);
    }, window);

    mfb_set_mouse_scroll_callback([](struct mfb_window *window, mfb_key_mod mod, float deltaX, float deltaY) {
        const char *window_title = "";
        if(window) {
            window_title = (const char *) mfb_get_user_data(window);
        }
        fprintf(stdout, "%s > mouse_scroll: x: %f, y: %f [key_mod: %x]\n", window_title, deltaX, deltaY, mod);
    }, window);

#else

    using namespace std::placeholders;

    mfb_set_active_callback      (std::bind(&Events::active,       &e, _1, _2),         window);
    mfb_set_resize_callback      (std::bind(&Events::resize,       &e, _1, _2, _3),     window);
    mfb_set_close_callback       (std::bind(&Events::close,        &e, _1),             window);
    mfb_set_keyboard_callback    (std::bind(&Events::keyboard,     &e, _1, _2, _3, _4), window);
    mfb_set_char_input_callback  (std::bind(&Events::char_input,   &e, _1, _2),         window);
    mfb_set_mouse_button_callback(std::bind(&Events::mouse_button, &e, _1, _2, _3, _4), window);
    mfb_set_mouse_move_callback  (std::bind(&Events::mouse_move,   &e, _1, _2, _3),     window);
    mfb_set_mouse_scroll_callback(std::bind(&Events::mouse_scroll, &e, _1, _2, _3, _4), window);

#endif

    mfb_set_user_data(window, (void *) "Input Events CPP Test");

    do {
        int         i;
        mfb_update_state state;

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
        if (state != STATE_OK) {
            window = 0x0;
            break;
        }
    } while(mfb_wait_sync(window));

    return 0;
}
