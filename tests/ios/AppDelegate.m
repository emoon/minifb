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

//-------------------------------------
struct mfb_window   *g_window = 0x0;
uint32_t            *g_buffer = 0x0;
uint32_t            g_width   = 0;
uint32_t            g_height  = 0;
float               g_scale   = 1;

//-------------------------------------
@interface AppDelegate ()

@end

//-------------------------------------
void
mouse_btn(struct mfb_window *window, mfb_mouse_button button, mfb_key_mod mod, bool isPressed) {
    kUnused(mod);
    NSLog(@"Touch: %d at %d, %d is %d", (int)button - MOUSE_BTN_0, mfb_get_mouse_x(window), mfb_get_mouse_y(window), (int) isPressed);
}

//-------------------------------------
void
mouse_move(struct mfb_window *window, int x, int y) {
    kUnused(window);
    NSLog(@"Touch moved %d, %d", x, y);
}

void
resize(struct mfb_window *window, int width, int height) {
    kUnused(window);
    g_width  = width;
    g_height = height;
    g_buffer = realloc(g_buffer, g_width * g_height * 4);
    NSLog(@"Resize %d, %d", width, height);
}

//-------------------------------------
@implementation AppDelegate

//-------------------------------------
- (void) OnUpdateFrame {
    static int seed = 0xbeef;
    int noise, carry;
    int dis = 0;

    if(g_buffer != 0x0) {
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
                noise &= 0xFF >> dis;
                g_buffer[i++] = MFB_RGB(noise, noise, noise);
            }
            if((y & 0x07) == 0x07)
                dis ^= 0x01;
        }
    }

    mfb_update_state state = mfb_update_ex(g_window, g_buffer, g_width, g_height);
    if (state != STATE_OK) {
        free(g_buffer);
        g_buffer = 0x0;
        g_width   = 0;
        g_height  = 0;
    }
}

//-------------------------------------
- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
    // Override point for customization after application launch.
    kUnused(application);
    kUnused(launchOptions);

    if(g_window == 0x0) {
        mfb_get_monitor_scale(0x0, &g_scale, 0x0);
        //g_scale  = [UIScreen mainScreen].scale;
        g_width  = [UIScreen mainScreen].bounds.size.width  * g_scale;
        g_height = [UIScreen mainScreen].bounds.size.height * g_scale;
        g_window = mfb_open("noise", g_width, g_height);
        if(g_window != 0x0) {
            g_width  -= 100;
            g_height -= 100;
            mfb_set_viewport(g_window, 50, 50, g_width, g_height);
            g_buffer = malloc(g_width * g_height * 4);
            mfb_set_mouse_move_callback(g_window, mouse_move);
            mfb_set_mouse_button_callback(g_window, mouse_btn);
            mfb_set_resize_callback(g_window, resize);
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
- (void)applicationDidEnterBackground:(UIApplication *)application {
    // Use this method to release shared resources, save user data, invalidate timers, and store enough application state information to restore your application to its current state in case it is terminated later.
    // If your application supports background execution, this method is called instead of applicationWillTerminate: when the user quits.
    kUnused(application);
    [mDisplayLink invalidate];
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

    [mDisplayLink invalidate];
    mfb_close(g_window);
}

@end
