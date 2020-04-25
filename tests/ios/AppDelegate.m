//
//  AppDelegate.m
//  MiniFB
//
//  Created by Carlos Aragones on 22/04/2020.
//  Copyright © 2020 Carlos Aragones. All rights reserved.
//

#import "AppDelegate.h"
#include <MiniFB.h>
#include <iOSViewController.h>

static uint32_t     *g_buffer = 0x0;
static uint32_t     g_width   = 0;
static uint32_t     g_height  = 0;


//-------------------------------------
void
user_implemented_init(struct mfb_window *window) {
    g_width = mfb_get_window_width(window);
    g_height = mfb_get_window_height(window);
    g_buffer = malloc(g_width * g_height * 4);
    
//    mfb_set_viewport(window, 50, 50, 200, 200);
}

//-------------------------------------
void
user_implemented_update(struct mfb_window *window) {
    static int seed = 0xbeef;
    int noise, carry;

    if(g_buffer != 0x0) {
        for (uint32_t i = 0; i < g_width * g_height; ++i) {
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
    }
    
    mfb_update_state state = mfb_update(window, g_buffer);
    if (state != STATE_OK) {
        free(g_buffer);
        g_buffer = 0x0;
        g_width   = 0;
        g_height  = 0;
    }
}


@interface AppDelegate ()

@end

@implementation AppDelegate


- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
    // Override point for customization after application launch.
    return YES;
}

- (void) on_update {
    NSLog(@"si");
}

- (void) applicationWillResignActive:(UIApplication *)application {
    // Sent when the application is about to move from active to inactive state. This can occur for certain types of temporary interruptions (such as an incoming phone call or SMS message) or when the user quits the application and it begins the transition to the background state.
    // Use this method to pause ongoing tasks, disable timers, and invalidate graphics rendering callbacks. Games should use this method to pause the game.
}

- (void)applicationDidEnterBackground:(UIApplication *)application {
    // Use this method to release shared resources, save user data, invalidate timers, and store enough application state information to restore your application to its current state in case it is terminated later.
    // If your application supports background execution, this method is called instead of applicationWillTerminate: when the user quits.
}


- (void)applicationWillEnterForeground:(UIApplication *)application {
    // Called as part of the transition from the background to the active state; here you can undo many of the changes made on entering the background.
}


- (void)applicationDidBecomeActive:(UIApplication *)application {
    // Restart any tasks that were paused (or not yet started) while the application was inactive. If the application was previously in the background, optionally refresh the user interface.
    UIWindow    *window;
    NSArray     *pWindows;
    size_t      numWindows;

    pWindows = [[UIApplication sharedApplication] windows];

    numWindows   = [pWindows count];
    //iOSViewController *controller = [[iOSViewController alloc] initWithFrame: [UIScreen mainScreen].bounds];
    iOSViewController *controller = [[iOSViewController alloc] init];
    if(numWindows > 0)
    {
        window = [pWindows objectAtIndex:0];
    }
    else
    {
        // Notice that you need to set "Launch Screen File" in:
        // project > executable > general to get the real size
        window = [[UIWindow alloc] initWithFrame:[UIScreen mainScreen].bounds];
        NSLog(@"UIApplication has no window. We create one (%f, %f).", [UIScreen mainScreen].bounds.size.width, [UIScreen mainScreen].bounds.size.height);
    }
    [window setRootViewController:controller];
    [controller release];
    controller = (iOSViewController *) window.rootViewController;
    [window makeKeyAndVisible];

    mDisplayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(on_update)];
    [mDisplayLink addToRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
}


- (void)applicationWillTerminate:(UIApplication *)application {
    // Called when the application is about to terminate. Save data if appropriate. See also applicationDidEnterBackground:.
    [mDisplayLink invalidate];
}


@end
