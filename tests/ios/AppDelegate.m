//
//  AppDelegate.m
//  MiniFB
//
//  Created by Carlos Aragones on 22/04/2020.
//  Copyright © 2020 Carlos Aragones. All rights reserved.
//

#import "AppDelegate.h"
#include <MiniFB.h>

//-------------------------------------
#define kUnused(var)        (void) var;
#define APP_TAG             "noise"

//-------------------------------------
struct mfb_window   *g_window = 0x0;
uint32_t            *g_buffer = 0x0;
uint32_t            g_width   = 0;
uint32_t            g_height  = 0;
float               g_scale   = 1;
static uint32_t     g_frame_count = 0;
bool                g_is_active = false;

//-------------------------------------
@interface AppDelegate ()

@end

//-------------------------------------
static void
print_getters(struct mfb_window *window) {
    if (!window) {
        return;
    }

    unsigned win_w = 0, win_h = 0;
    unsigned win_sw = 0, win_sh = 0;
    unsigned draw_offset_x = 0, draw_offset_y = 0, draw_w = 0, draw_h = 0;
    unsigned bounds_offset_x = 0, bounds_offset_y = 0, bounds_w = 0, bounds_h = 0;
    unsigned fps = 0;
    int mouse_x = 0, mouse_y = 0;
    float scroll_x = 0.0f, scroll_y = 0.0f;
    float scale_x = 0.0f, scale_y = 0.0f;
    bool is_active = false;

    const char *key_name = mfb_get_key_name(MFB_KB_KEY_ESCAPE);
    const uint8_t *key_buffer = mfb_get_key_buffer(window);

    is_active = mfb_is_window_active(window);
    win_w = mfb_get_window_width(window);
    win_h = mfb_get_window_height(window);
    mfb_get_window_size(window, &win_sw, &win_sh);
    if (win_w != win_sw) {
        MFB_LOGE(APP_TAG, "Width does not match: %u != %u", win_w, win_sw);
    }
    if (win_h != win_sh) {
        MFB_LOGE(APP_TAG, "Height does not match: %u != %u", win_h, win_sh);
    }

    draw_offset_x = mfb_get_drawable_offset_x(window);
    draw_offset_y = mfb_get_drawable_offset_y(window);
    draw_w = mfb_get_drawable_width(window);
    draw_h = mfb_get_drawable_height(window);
    mfb_get_drawable_bounds(window, &bounds_offset_x, &bounds_offset_y, &bounds_w, &bounds_h);

    int mouse_id = 0;
    mfb_decode_touch(mfb_get_mouse_x(window), &mouse_x, &mouse_id);
    mfb_decode_touch(mfb_get_mouse_y(window), &mouse_y, NULL);
    scroll_x = mfb_get_mouse_scroll_x(window);
    scroll_y = mfb_get_mouse_scroll_y(window);
    const uint8_t *mouse_buttons = mfb_get_mouse_button_buffer(window);

    mfb_get_monitor_scale(window, &scale_x, &scale_y);
    fps = mfb_get_target_fps();

    int cutout_left, cutout_right, cutout_top, cutout_bottom;
    int safe_left, safe_right, safe_top, safe_bottom;

    mfb_get_display_cutout_insets(window, &cutout_left, &cutout_top, &cutout_right, &cutout_bottom);
    mfb_get_display_safe_insets(window, &safe_left, &safe_top, &safe_right, &safe_bottom);

    MFB_LOGD(APP_TAG, "[getters frame=%d]", g_frame_count);
    MFB_LOGD(APP_TAG, "  key_name(MFB_KB_KEY_ESCAPE): %s", key_name ? key_name : "(null)");
    MFB_LOGD(APP_TAG, "  is_window_active: %d", is_active);
    MFB_LOGD(APP_TAG, "  target_fps: %u", fps);
    MFB_LOGD(APP_TAG, "  monitor_scale: %f, %f", scale_x, scale_y);
    MFB_LOGD(APP_TAG, "  window_size: %u x %u", win_w, win_h);
    MFB_LOGD(APP_TAG, "  drawable_offsets: %u, %u", draw_offset_x, draw_offset_y);
    MFB_LOGD(APP_TAG, "  drawable_size: %u x %u", draw_w, draw_h);
    MFB_LOGD(APP_TAG, "  drawable_bounds: offset (%u, %u) size (%u, %u)", bounds_offset_x, bounds_offset_y, bounds_w, bounds_h);
    MFB_LOGD(APP_TAG, "  cutout_insets: left %u, right %u, top %u, bottom %u", cutout_left, cutout_right, cutout_top, cutout_bottom);
    MFB_LOGD(APP_TAG, "  safe_insets: left %u, right %u, top %u, bottom %u", safe_left, safe_right, safe_top, safe_bottom);
    MFB_LOGD(APP_TAG, "  mouse_pos: %d, %d (touch %d)", mouse_x, mouse_y, mouse_id);
    MFB_LOGD(APP_TAG, "  mouse_scroll: %f, %f", scroll_x, scroll_y);

    if (mouse_buttons) {
        MFB_LOGD(APP_TAG, "  mouse_buttons[0..7]: %u %u %u %u %u %u %u %u",
                mouse_buttons[0], mouse_buttons[1], mouse_buttons[2], mouse_buttons[3],
                mouse_buttons[4], mouse_buttons[5], mouse_buttons[6], mouse_buttons[7]);
    }
    else {
        MFB_LOGD(APP_TAG, "  mouse_buttons: (null)");
    }

    if (key_buffer) {
        MFB_LOGD(APP_TAG, "  key_buffer sample [ESC=%u, SPACE=%u, LEFT=%u, RIGHT=%u, UP=%u, DOWN=%u]",
                key_buffer[MFB_KB_KEY_ESCAPE],
                key_buffer[MFB_KB_KEY_SPACE],
                key_buffer[MFB_KB_KEY_LEFT],
                key_buffer[MFB_KB_KEY_RIGHT],
                key_buffer[MFB_KB_KEY_UP],
                key_buffer[MFB_KB_KEY_DOWN]);
    }
    else {
        MFB_LOGD(APP_TAG, "  key_buffer: (null)");
    }
}

//-------------------------------------
static void
mouse_btn(struct mfb_window *window, mfb_mouse_button button, mfb_key_mod mod, bool is_pressed) {
    kUnused(mod);
    int x, y, id;
    mfb_decode_touch(mfb_get_mouse_x(window), &x, &id);
    mfb_decode_touch(mfb_get_mouse_y(window), &y, NULL);
    MFB_LOGD(APP_TAG, "Touch: %d at %d, %d is %d (touch %d)", (int)button - MFB_MOUSE_BTN_0, x, y,
        (int) is_pressed, id);
}

//-------------------------------------
static void
mouse_move(struct mfb_window *window, int x, int y) {
    kUnused(window);
    int id;
    mfb_decode_touch(x, &x, &id);
    mfb_decode_touch(y, &y, NULL);
    MFB_LOGD(APP_TAG, "Touch moved %d, %d (touch %d)", x, y, id);
}

//-------------------------------------
static void
active_changed(struct mfb_window *window, bool is_active) {
    kUnused(window);
    MFB_LOGD(APP_TAG, "Active: %s", is_active ? "true" : "false");
    g_is_active = is_active;
}

//-------------------------------------
static bool
window_closing(struct mfb_window *window) {
    kUnused(window);
    MFB_LOGD(APP_TAG, "Window closing");
    return true;
}

//-------------------------------------
static void
resize(struct mfb_window *window, int width, int height) {
    kUnused(window);

    if (width <= 0 || height <= 0) {
        free(g_buffer);
        g_buffer = 0x0;
        g_width  = 0;
        g_height = 0;
        MFB_LOGD(APP_TAG, "Resize %d, %d", width, height);
        return;
    }

    size_t new_size = (size_t) width * (size_t) height * sizeof(uint32_t);
    uint32_t *new_buffer = realloc(g_buffer, new_size);
    if (new_buffer == 0x0) {
        MFB_LOGE(APP_TAG, "Resize %d, %d failed: out of memory", width, height);
        return;
    }

    g_width  = (uint32_t) width;
    g_height = (uint32_t) height;
    g_buffer = new_buffer;
    MFB_LOGD(APP_TAG, "Resize %d, %d", width, height);
}

//-------------------------------------
@implementation AppDelegate

//-------------------------------------
- (void) OnUpdateFrame {
    static int seed = 0xbeef;
    int noise, carry;

    if (g_is_active == false) {
        return;
    }

    if(g_window == 0x0) {
        return;
    }

    if(g_buffer != 0x0) {
        g_frame_count++;

        int safe_left = 0, safe_top = 0, safe_right = 0, safe_bottom = 0;
        mfb_get_display_safe_insets(g_window, &safe_left, &safe_top, &safe_right, &safe_bottom);

        uint32_t inset_left   = (safe_left   > 0) ? (uint32_t) safe_left   : 0;
        uint32_t inset_top    = (safe_top    > 0) ? (uint32_t) safe_top    : 0;
        uint32_t inset_right  = (safe_right  > 0) ? (uint32_t) safe_right  : 0;
        uint32_t inset_bottom = (safe_bottom > 0) ? (uint32_t) safe_bottom : 0;

        if (inset_left   > g_width)  inset_left   = g_width;
        if (inset_right  > g_width)  inset_right  = g_width;
        if (inset_top    > g_height) inset_top    = g_height;
        if (inset_bottom > g_height) inset_bottom = g_height;

        bool has_insets = (inset_left > 0) || (inset_top > 0) || (inset_right > 0) || (inset_bottom > 0);

        uint32_t i = 0;
        for (uint32_t y = 0; y < g_height; ++y) {
            for (uint32_t x = 0; x < g_width; ++x) {
                noise = seed;
                noise >>= 3;
                noise ^= seed;
                carry = noise & 1;
                noise >>= 1;
                seed >>= 1;
                seed |= (carry << 30);
                noise &= 0xFF;

                bool in_inset = has_insets && (
                    (x < inset_left) ||
                    (x >= (g_width - inset_right)) ||
                    (y < inset_top) ||
                    (y >= (g_height - inset_bottom))
                );
                g_buffer[i++] = in_inset
                    ? MFB_ARGB(0xFF, noise, 0, 0)
                    : MFB_ARGB(0xFF, noise, noise, noise);
            }
        }
    }

    mfb_update_state state = mfb_update_ex(g_window, g_buffer, g_width, g_height);
    if (state == MFB_STATE_EXIT) {
        g_window = 0x0;
        free(g_buffer);
        g_buffer = 0x0;
        g_width   = 0;
        g_height  = 0;
    }
    else if (state != MFB_STATE_OK) {
        MFB_LOGE(APP_TAG, "mfb_update_ex returned state=%d", state);
    }
}

//-------------------------------------
- (BOOL) application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
    // Override point for customization after application launch.
    kUnused(application);
    kUnused(launchOptions);

    if(g_window == 0x0) {
        mfb_get_monitor_scale(0x0, &g_scale, 0x0);
        // Use physical pixels so cutout insets (also in pixels) map 1:1 to buffer coords.
        g_width  = (uint32_t)([UIScreen mainScreen].bounds.size.width  * g_scale);
        g_height = (uint32_t)([UIScreen mainScreen].bounds.size.height * g_scale);
        g_window = mfb_open("noise", g_width, g_height);
        if(g_window != 0x0) {
            // Do not delete. It is here to test the viewport
            //g_width -= 100;
            //g_height -= 100;
            //mfb_set_viewport(g_window, 50, 50, g_width, g_height);

            g_buffer = malloc(g_width * g_height * 4);
            mfb_set_active_callback(g_window, active_changed);
            mfb_set_close_callback(g_window, window_closing);
            mfb_set_mouse_move_callback(g_window, mouse_move);
            mfb_set_mouse_button_callback(g_window, mouse_btn);
            mfb_set_resize_callback(g_window, resize);

            print_getters(g_window);
        }
    }

    return YES;
}

//-------------------------------------
- (void) applicationWillResignActive:(UIApplication *)application {
    // Sent when the application is about to move from active to inactive state. This can occur for certain types of temporary interruptions (such as an incoming phone call or SMS message) or when the user quits the application and it begins the transition to the background state.
    // Use this method to pause ongoing tasks, disable timers, and invalidate graphics rendering callbacks. Games should use this method to pause the game.
    kUnused(application);
}

//-------------------------------------
- (void) applicationDidEnterBackground:(UIApplication *)application {
    // Use this method to release shared resources, save user data, invalidate timers, and store enough application state information to restore your application to its current state in case it is terminated later.
    // If your application supports background execution, this method is called instead of applicationWillTerminate: when the user quits.
    kUnused(application);
    [mDisplayLink invalidate];
    mDisplayLink = nil;
}

//-------------------------------------
- (void)applicationWillEnterForeground:(UIApplication *)application {
    // Called as part of the transition from the background to the active state; here you can undo many of the changes made on entering the background.
    kUnused(application);
}

//-------------------------------------
- (void)applicationDidBecomeActive:(UIApplication *)application {
    // Restart any tasks that were paused (or not yet started) while the application was inactive. If the application was previously in the background, optionally refresh the user interface.
    kUnused(application);

    mDisplayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(OnUpdateFrame)];
    [mDisplayLink addToRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
}

//-------------------------------------
- (void)applicationWillTerminate:(UIApplication *)application {
    // Called when the application is about to terminate. Save data if appropriate. See also applicationDidEnterBackground:.
    kUnused(application);

    if (mDisplayLink != nil) {
        [mDisplayLink invalidate];
        mDisplayLink = nil;
    }

    if (g_window != 0x0) {
        mfb_close(g_window);
        g_window = 0x0;
    }
}

@end
