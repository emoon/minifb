#include <android_native_app_glue.h>
#include <android/log.h>
#include <MiniFB.h>
#include <stdlib.h>
#include <math.h>

#define  LOG_TAG    "Noise"
#define  LOGV(...)  __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__)
#define  LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG,   LOG_TAG, __VA_ARGS__)
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,    LOG_TAG, __VA_ARGS__)
#define  LOGW(...)  __android_log_print(ANDROID_LOG_WARN,    LOG_TAG, __VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,   LOG_TAG, __VA_ARGS__)
#define  LOGF(...)  __android_log_print(ANDROID_LOG_FATAL,   LOG_TAG, __VA_ARGS__)

#define kUnused(var)        (void) var;

#define kTouchIdMask    0xf0000000
#define kTouchPosMask   0x0fffffff
#define kTouchIdShift   28

#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#define MAX(a,b) (((a) > (b)) ? (a) : (b))

typedef struct {
    bool    enabled;
    int     x, y;
} Pos;

static uint32_t  g_width  = 200;
static uint32_t  g_height = 100;
static uint32_t *g_buffer = 0x0;
static Pos      g_positions[16] = {};

void
active(struct mfb_window *window, bool isActive) {
    LOGI("active: %d", isActive);
}

void
resize(struct mfb_window *window, int width, int height) {
    LOGI("resize: %d, %d", width, height);
    g_width  = (width  >> 1);
    g_height = (height >> 1);
    g_buffer = realloc(g_buffer, g_width * g_height * 4);
}

void
keyboard(struct mfb_window *window, mfb_key key, mfb_key_mod mod, bool isPressed) {
    LOGI("keyboard:");
}

void
char_input(struct mfb_window *window, unsigned int charCode) {
    LOGI("char_input:");
}

void
mouse_btn(struct mfb_window *window, mfb_mouse_button button, mfb_key_mod mod, bool isPressed) {
    int x = (mfb_get_mouse_x(window) & kTouchPosMask) >> 1;
    int y = (mfb_get_mouse_y(window) & kTouchPosMask) >> 1;
    g_positions[button].enabled = isPressed;
    g_positions[button].x = x;
    g_positions[button].y = y;
    LOGI("mouse_btn: button: id %d=%d, x=%d, y=%d", (int)button, (int) isPressed, x, y);
}

void
mouse_move(struct mfb_window *window, int x, int y) {
    int id = (x & kTouchIdMask) >> kTouchIdShift;
    x = (x & kTouchPosMask) >> 1;
    y = (y & kTouchPosMask) >> 1;
    g_positions[id].enabled = true;
    g_positions[id].x = x;
    g_positions[id].y = y;
    LOGI("mouse_move: %d, %d [%d]", x, y, id);
}

void
mouse_scroll(struct mfb_window *window, mfb_key_mod mod, float deltaX, float deltaY) {
    LOGI("mouse_scroll:");
}

// I'm not sure that we can use this function name but it works
//-------------------------------------
int
main(int argc, char *argv[]) {
    kUnused(argc);
    kUnused(argv);
    uint32_t    i, noise, carry, seed = 0xbeef;

    struct mfb_window *window = mfb_open("Ignored", g_width, g_height);
    if(window == 0x0)
        return 0;

    mfb_set_active_callback(window, active);
    mfb_set_resize_callback(window, resize);
    mfb_set_keyboard_callback(window, keyboard);            // not working
    mfb_set_char_input_callback(window, char_input);        // not working
    mfb_set_mouse_button_callback(window, mouse_btn);
    mfb_set_mouse_move_callback(window, mouse_move);
    mfb_set_mouse_scroll_callback(window, mouse_scroll);    // not working

    g_buffer = (uint32_t *) malloc(g_width * g_height * 4);

    mfb_update_state state;
    do {
        bool isActive   = mfb_is_window_active(window);
        unsigned width  = mfb_get_window_width(window);
        unsigned height = mfb_get_window_height(window);
        int mouseX      = mfb_get_mouse_x(window);
        int mouseY      = mfb_get_mouse_y(window);
        float scrollX   = mfb_get_mouse_scroll_x(window);   // not working
        float scrollY   = mfb_get_mouse_scroll_y(window);   // not working
        const uint8_t *buttons = mfb_get_mouse_button_buffer(window);
        const uint8_t *keys = mfb_get_key_buffer(window);   // not working

        for (i = 0; i < g_width * g_height; ++i) {
            noise = seed;
            noise >>= 3;
            noise ^= seed;
            carry = noise & 1;
            noise >>= 1;
            seed >>= 1;
            seed |= (carry << 30);
            noise &= 0xFF;

            // Comment out to test appropriate colour channel
            //g_buffer[i] = MFB_RGB(noise, 0, 0);     // Test red channel
            //g_buffer[i] = MFB_RGB(0, noise, 0);     // Test green channel
            //g_buffer[i] = MFB_RGB(0, 0, noise);     // Test blue channel
            g_buffer[i] = MFB_RGB(noise, noise, noise);
        }

        for (int p = 0; p < 16; ++p) {
            if (g_positions[p].enabled) {
                int minX = MAX(g_positions[p].x - 16, 0);
                int maxX = MIN(g_positions[p].x + 16, g_width);
                int minY = MAX(g_positions[p].y - 16, 0);
                int maxY = MIN(g_positions[p].y + 16, g_height);
                for(int y=minY; y<maxY; ++y) {
                    i = y * g_width + minX;
                    for(int x=minX; x<maxX; ++x) {
                        g_buffer[i++] = 0;
                    }
                }
            }
        }

        state = mfb_update_ex(window, g_buffer, g_width, g_height);
        if (state != STATE_OK) {
            window = 0x0;
            break;
        }
    } while(mfb_wait_sync(window));}
