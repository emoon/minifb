// press 'H' to hide, press 'S' to show

#include <MiniFB.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

static uint32_t  g_width  = 800;
static uint32_t  g_height = 600;
static uint32_t *g_buffer = 0x0;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void 
resize(struct mfb_window *window, int width, int height) {
    (void) window;
    g_width  = width;
    g_height = height;
    g_buffer = realloc(g_buffer, g_width * g_height * 4);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void
keyboard_event(struct mfb_window *window, mfb_key key, mfb_key_mod mod, bool isPressed) {
	if (key == KB_KEY_H) {
		mfb_show_cursor(window, false);
	} else if (key == KB_KEY_S) {
		mfb_show_cursor(window, true);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int 
main() {
    uint32_t    i, noise, carry, seed = 0xbeef;

    struct mfb_window *window = mfb_open_ex("H to hide, S to show", g_width, g_height, WF_RESIZABLE);
    if (!window)
        return 0;

    mfb_set_keyboard_callback(window, keyboard_event);

    g_buffer = (uint32_t *) malloc(g_width * g_height * 4);
    mfb_set_resize_callback(window, resize);

    mfb_set_viewport(window, 50, 50, g_width - 50 - 50, g_height - 50 - 50);
    resize(window, g_width - 100, g_height - 100);  // to resize buffer

    mfb_update_state state;
    do {
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

        state = mfb_update_ex(window, g_buffer, g_width, g_height);
        if (state != STATE_OK) {
            window = 0x0;
            break;
        }
    } while(mfb_wait_sync(window));

    return 0;
}
