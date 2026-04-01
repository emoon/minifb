#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#include <mach/mach_time.h>
#include <math.h>
#include <unistd.h>

#include "iOSViewController.h"
#include "iOSView.h"
#include "iOSViewDelegate.h"
#include "WindowData_IOS.h"
#include <MiniFB.h>
#include <MiniFB_internal.h>
#include <WindowData.h>

static UIWindow *
get_application_window(void) {
    UIApplication *application = [UIApplication sharedApplication];
    if (application == nil) {
        return nil;
    }

#if defined(__IPHONE_OS_VERSION_MAX_ALLOWED) && (__IPHONE_OS_VERSION_MAX_ALLOWED >= 130000)
    if (@available(iOS 13.0, *)) {
        UIWindow *fallback_window = nil;

        for (UIScene *scene in application.connectedScenes) {
            if (![scene isKindOfClass:[UIWindowScene class]]) {
                continue;
            }

            UIWindowScene *window_scene = (UIWindowScene *) scene;
            bool is_foreground_scene =
                (scene.activationState == UISceneActivationStateForegroundActive) ||
                (scene.activationState == UISceneActivationStateForegroundInactive);

            if (is_foreground_scene) {
                if (@available(iOS 15.0, *)) {
                    if (window_scene.keyWindow != nil) {
                        return window_scene.keyWindow;
                    }
                }

                for (UIWindow *candidate in window_scene.windows) {
                    if (candidate.isKeyWindow) {
                        return candidate;
                    }
                    if (fallback_window == nil) {
                        fallback_window = candidate;
                    }
                }
            }
            else if (fallback_window == nil && window_scene.windows.count > 0) {
                fallback_window = window_scene.windows.firstObject;
            }
        }

        if (fallback_window != nil) {
            return fallback_window;
        }
    }
#endif

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    NSArray<UIWindow *> *windows = application.windows;
#pragma clang diagnostic pop

    if (windows.count > 0) {
        return windows.firstObject;
    }

    return nil;
}

//-------------------------------------
SWindowData *
create_window_data(unsigned width, unsigned height) {
    uint32_t buffer_stride = 0;
    size_t   total_bytes = 0;

    if (!calculate_buffer_layout(width, height, &buffer_stride, &total_bytes)) {
        MFB_LOG(MFB_LOG_ERROR, "iOSMiniFB: invalid window buffer size %ux%u.", width, height);
        return NULL;
    }

    SWindowData *window_data = malloc(sizeof(SWindowData));
    if(window_data == NULL) {
        MFB_LOG(MFB_LOG_ERROR, "iOSMiniFB: failed to allocate SWindowData.");
        return NULL;
    }
    memset(window_data, 0, sizeof(SWindowData));

    SWindowData_IOS *window_data_specific = malloc(sizeof(SWindowData_IOS));
    if(window_data_specific == NULL) {
        free(window_data);
        MFB_LOG(MFB_LOG_ERROR, "iOSMiniFB: failed to allocate SWindowData_IOS.");
        return NULL;
    }
    memset((void *) window_data_specific, 0, sizeof(SWindowData_IOS));
    window_data_specific->buffer_lock = OS_UNFAIR_LOCK_INIT;

    window_data->specific = window_data_specific;

    float scale = [UIScreen mainScreen].scale;

    window_data->window_width  = [UIScreen mainScreen].bounds.size.width  * scale;
    window_data->window_height = [UIScreen mainScreen].bounds.size.height * scale;

    calc_dst_factor(window_data, width, height);

    window_data->buffer_width  = width;
    window_data->buffer_height = height;
    window_data->buffer_stride = buffer_stride;

    window_data->is_cursor_visible = false;

    window_data->draw_buffer = malloc(total_bytes);
    if (!window_data->draw_buffer) {
        free(window_data_specific);
        free(window_data);
        MFB_LOG(MFB_LOG_ERROR, "iOSMiniFB: failed to allocate draw buffer (%zu bytes).", total_bytes);
        return NULL;
    }
    memset(window_data->draw_buffer, 0, total_bytes);

    return window_data;
}

//-------------------------------------
struct mfb_window *
mfb_open_ex(const char *title, unsigned width, unsigned height, unsigned flags) {
    UIWindow    *window;

    kUnused(title);

    if (width == 0 || height == 0) {
        MFB_LOG(MFB_LOG_ERROR, "iOSMiniFB: invalid window size %ux%u.", width, height);
        return NULL;
    }

    if (flags != 0u) {
        MFB_LOG(MFB_LOG_WARNING, "iOSMiniFB: window flags 0x%x are ignored by the iOS backend.", flags);
    }

    @autoreleasepool {
        SWindowData *window_data = create_window_data(width, height);
        if (window_data == NULL) {
            return NULL;
        }

        SWindowData_IOS *window_data_specific = (SWindowData_IOS *) window_data->specific;

        window_data_specific->timer = mfb_timer_create();
        if (window_data_specific->timer == NULL) {
            MFB_LOG(MFB_LOG_ERROR, "iOSMiniFB: failed to create frame timer.");
            free(window_data->draw_buffer);
            free(window_data_specific);
            free(window_data);
            return NULL;
        }

        window = get_application_window();
        if (window == nil) {
            // Notice that you need to set "Launch Screen File" in:
            // project > executable > general
            // to get the real size with [UIScreen mainScreen].bounds].
            window = [[UIWindow alloc] initWithFrame:[UIScreen mainScreen].bounds];
            MFB_LOG(MFB_LOG_DEBUG, "iOSMiniFB: UIApplication has no window, creating one (%f x %f).",
                    [UIScreen mainScreen].bounds.size.width,
                    [UIScreen mainScreen].bounds.size.height);
        }

        iOSViewController *controller = nil;
        if([window.rootViewController isKindOfClass:[iOSViewController class]] == false) {
            controller = [[iOSViewController alloc] initWithWindowData:window_data];
            [window setRootViewController:controller];

    #if !__has_feature(objc_arc)
            [controller release];
    #endif

            controller = (iOSViewController *) window.rootViewController;
        }
        else {
            controller = (iOSViewController *) window.rootViewController;
        }

        [controller attachWindowData:window_data];
        [window makeKeyAndVisible];

        window_data->is_initialized = true;

        MFB_LOG(MFB_LOG_DEBUG, "iOSMiniFB: window created (size=%ux%u, screen=%.0fx%.0f, scale=%.1f).",
                width, height,
                [UIScreen mainScreen].bounds.size.width,
                [UIScreen mainScreen].bounds.size.height,
                [UIScreen mainScreen].scale);

        return (struct mfb_window *) window_data;
    }
}

//-------------------------------------
static void
destroy_window_data(SWindowData *window_data) {
    if(window_data == NULL)
        return;

    release_cpp_stub((struct mfb_window *) window_data);

    @autoreleasepool {
        // Invalidate window_data pointers in UIKit objects before freeing
        UIWindow *app_window = get_application_window();
        if (app_window != nil) {
            UIViewController *vc = app_window.rootViewController;
            if ([vc isKindOfClass:[iOSViewController class]]) {
                iOSViewController *controller = (iOSViewController *) vc;
                if (controller->window_data == window_data) {
                    controller->window_data = NULL;
                }
                if ([controller isViewLoaded] && [controller.view isKindOfClass:[iOSView class]]) {
                    iOSView *view = (iOSView *) controller.view;
                    if (view->window_data == window_data) {
                        view->window_data = NULL;
                    }
                    // Disconnect the MTKView delegate before freeing to prevent
                    // drawInMTKView: from being called on a stale delegate.
                    view.delegate = nil;
                }
            }
        }

        SWindowData_IOS *window_data_specific = (SWindowData_IOS *) window_data->specific;
        if(window_data_specific != NULL) {

#if !__has_feature(objc_arc)
            if(window_data_specific->view_delegate != nil) {
                [window_data_specific->view_delegate release];
                window_data_specific->view_delegate = nil;
            }
#endif

            mfb_timer_destroy(window_data_specific->timer);
            window_data_specific->timer = NULL;

            memset((void *) window_data_specific, 0, sizeof(SWindowData_IOS));
            free(window_data_specific);
        }

        if (window_data->draw_buffer != NULL) {
            free(window_data->draw_buffer);
            window_data->draw_buffer = NULL;
        }

        memset(window_data, 0, sizeof(SWindowData));
        free(window_data);
    }
}

//-------------------------------------
mfb_update_state
mfb_update_ex(struct mfb_window *window, void *buffer, unsigned width, unsigned height) {
    SWindowData *window_data = (SWindowData *) window;
    uint32_t buffer_stride = 0;
    size_t total_bytes = 0;

    if(window_data == NULL) {
        MFB_LOG(MFB_LOG_DEBUG, "iOSMiniFB: mfb_update_ex called with an invalid window.");
        return MFB_STATE_INVALID_WINDOW;
    }

    if(window_data->close) {
        MFB_LOG(MFB_LOG_DEBUG, "iOSMiniFB: mfb_update_ex aborted because the window is marked for close.");
        destroy_window_data(window_data);
        return MFB_STATE_EXIT;
    }

    if(buffer == NULL) {
        MFB_LOG(MFB_LOG_ERROR, "iOSMiniFB: mfb_update_ex called with a null buffer.");
        return MFB_STATE_INVALID_BUFFER;
    }

    if (!calculate_buffer_layout(width, height, &buffer_stride, &total_bytes)) {
        MFB_LOG(MFB_LOG_ERROR, "iOSMiniFB: mfb_update_ex called with invalid buffer size %ux%u.", width, height);
        return MFB_STATE_INVALID_BUFFER;
    }

    SWindowData_IOS *window_data_specific = (SWindowData_IOS *) window_data->specific;
    if (window_data_specific == NULL) {
        MFB_LOG(MFB_LOG_ERROR, "iOSMiniFB: missing iOS-specific window data in mfb_update_ex.");
        return MFB_STATE_INVALID_WINDOW;
    }

    if(window_data->buffer_width != width || window_data->buffer_height != height) {
        void *new_draw_buffer = malloc(total_bytes);
        if (new_draw_buffer == NULL) {
            MFB_LOG(MFB_LOG_ERROR, "iOSMiniFB: failed to allocate resized draw buffer (%zu bytes).", total_bytes);
            return MFB_STATE_INTERNAL_ERROR;
        }

        unsigned previous_width  = window_data->buffer_width;
        unsigned previous_height = window_data->buffer_height;
        uint32_t previous_stride = window_data->buffer_stride;

        window_data->buffer_width  = width;
        window_data->buffer_height = height;
        window_data->buffer_stride = buffer_stride;

        if (window_data_specific->view_delegate == nil) {
            MFB_LOG(MFB_LOG_ERROR, "iOSMiniFB: Metal view delegate is missing during buffer resize.");
            window_data->buffer_width  = previous_width;
            window_data->buffer_height = previous_height;
            window_data->buffer_stride = previous_stride;
            free(new_draw_buffer);
            return MFB_STATE_INVALID_WINDOW;
        }

        if (![window_data_specific->view_delegate resizeTextures]) {
            MFB_LOG(MFB_LOG_ERROR, "iOSMiniFB: failed to resize Metal textures after framebuffer resize.");
            window_data->buffer_width  = previous_width;
            window_data->buffer_height = previous_height;
            window_data->buffer_stride = previous_stride;
            free(new_draw_buffer);
            return MFB_STATE_INTERNAL_ERROR;
        }

        // Swap draw_buffer under lock so drawInMTKView never reads a freed pointer.
        os_unfair_lock_lock(&window_data_specific->buffer_lock);
        free(window_data->draw_buffer);
        window_data->draw_buffer = new_draw_buffer;
    }
    else {
        os_unfair_lock_lock(&window_data_specific->buffer_lock);
    }

    if (window_data->draw_buffer == NULL) {
        os_unfair_lock_unlock(&window_data_specific->buffer_lock);
        MFB_LOG(MFB_LOG_ERROR, "iOSMiniFB: internal draw buffer is null in mfb_update_ex.");
        return MFB_STATE_INTERNAL_ERROR;
    }

    memcpy(window_data->draw_buffer, buffer, total_bytes);
    os_unfair_lock_unlock(&window_data_specific->buffer_lock);

    if (window_data->close) {
        MFB_LOG(MFB_LOG_DEBUG, "iOSMiniFB: mfb_update_ex detected close request.");
        destroy_window_data(window_data);
        return MFB_STATE_EXIT;
    }

    if (window_data->must_resize_context) {
        window_data->must_resize_context = false;
        kCall(resize_func, (int) window_data->window_width, (int) window_data->window_height);
    }

    return MFB_STATE_OK;
}

//-------------------------------------
mfb_update_state
mfb_update_events(struct mfb_window *window) {
    SWindowData *window_data = (SWindowData *) window;

    if(window_data == NULL) {
        MFB_LOG(MFB_LOG_DEBUG, "iOSMiniFB: mfb_update_events called with an invalid window.");
        return MFB_STATE_INVALID_WINDOW;
    }

    if (window_data->close) {
        MFB_LOG(MFB_LOG_DEBUG, "iOSMiniFB: mfb_update_events aborted because the window is marked for close.");
        destroy_window_data(window_data);
        return MFB_STATE_EXIT;
    }

    window_data->mouse_wheel_x = 0.0f;
    window_data->mouse_wheel_y = 0.0f;

    if (window_data->must_resize_context) {
        window_data->must_resize_context = false;
        kCall(resize_func, (int) window_data->window_width, (int) window_data->window_height);
    }

    return MFB_STATE_OK;
}

//-------------------------------------
extern double   g_time_for_frame;
extern bool     g_use_hardware_sync;

bool
mfb_wait_sync(struct mfb_window *window) {
    SWindowData *window_data = (SWindowData *) window;

    if(window_data == NULL) {
        MFB_LOG(MFB_LOG_DEBUG, "iOSMiniFB: mfb_wait_sync called with an invalid window.");
        return false;
    }

    if (window_data->close) {
        MFB_LOG(MFB_LOG_DEBUG, "iOSMiniFB: mfb_wait_sync aborted because the window is marked for close.");
        destroy_window_data(window_data);
        return false;
    }

    SWindowData_IOS *window_data_specific = (SWindowData_IOS *) window_data->specific;
    if (window_data_specific == NULL || window_data_specific->timer == NULL) {
        MFB_LOG(MFB_LOG_ERROR, "iOSMiniFB: mfb_wait_sync missing iOS timer state.");
        return false;
    }

    // Hardware sync: MTKView's CADisplayLink already handles vsync
    if (g_use_hardware_sync) {
        return mfb_update_events(window) == MFB_STATE_OK;
    }

    // No target FPS set: nothing to pace against
    if (g_time_for_frame == 0.0) {
        return mfb_update_events(window) == MFB_STATE_OK;
    }

    // Software pacing: sleep for the remaining frame time to avoid busy-waiting,
    // which would drain the battery on a mobile device.
    for (;;) {
        double elapsed_time = mfb_timer_now(window_data_specific->timer);
        if (elapsed_time >= g_time_for_frame) {
            break;
        }

        if (window_data->close) {
            MFB_LOG(MFB_LOG_DEBUG, "iOSMiniFB: mfb_wait_sync detected close request while waiting.");
            destroy_window_data(window_data);
            return false;
        }

        double remaining = g_time_for_frame - elapsed_time;
        usleep((useconds_t)(remaining * 0.5 * 1e6));
    }

    mfb_timer_compensated_reset(window_data_specific->timer);
    return true;
}

//-------------------------------------
bool
mfb_set_viewport(struct mfb_window *window, unsigned offset_x, unsigned offset_y, unsigned width, unsigned height) {
    SWindowData *window_data = (SWindowData *) window;

    if (!mfb_validate_viewport(window_data, offset_x, offset_y, width, height, "iOSMiniFB")) {
        return false;
    }

    window_data->dst_offset_x = offset_x;
    window_data->dst_offset_y = offset_y;
    window_data->dst_width    = width;
    window_data->dst_height   = height;
    calc_dst_factor(window_data, window_data->window_width, window_data->window_height);

    return true;
}

//-------------------------------------
void
mfb_set_title(struct mfb_window *window, const char *title) {
    (void) window;
    (void) title;
}

//-------------------------------------
extern double   g_timer_frequency;
extern double   g_timer_resolution;

uint64_t
mfb_timer_tick() {
    static mach_timebase_info_data_t    timebase = { 0 };

    if (timebase.denom == 0) {
        (void) mach_timebase_info(&timebase);
    }

    uint64_t time = mach_absolute_time();

    // Perform the arithmetic at 128-bit precision to avoid the overflow!
    uint64_t high     = (time >> 32) * timebase.numer;
    uint64_t high_rem = ((high % timebase.denom) << 32) / timebase.denom;
    uint64_t low      = (time & 0xFFFFFFFFull) * timebase.numer / timebase.denom;
    high /= timebase.denom;

    return (high << 32) + high_rem + low;
}

//-------------------------------------
void
mfb_timer_init() {
    g_timer_frequency  = 1e+9;
    g_timer_resolution = 1.0 / g_timer_frequency;
}

//-------------------------------------
void
mfb_get_monitor_scale(struct mfb_window *window, float *scale_x, float *scale_y) {
    kUnused(window);
    float scale = [[UIScreen mainScreen] scale];

    if (scale_x) {
        *scale_x = (scale != 0.0f) ? scale : 1.0f;
    }

    if (scale_y) {
        *scale_y = (scale != 0.0f) ? scale : 1.0f;
    }
}

//-------------------------------------
void
mfb_show_cursor(struct mfb_window *window, bool show) {
    kUnused(window);
    kUnused(show);
    // iOS has no visible cursor
}

//-------------------------------------
static BOOL
has_launch_screen(void) {
    static BOOL cached = NO;
    static BOOL checked = NO;

    if (!checked) {
        cached =
            [[NSBundle mainBundle] objectForInfoDictionaryKey:@"UILaunchStoryboardName"] != nil ||
            [[NSBundle mainBundle] objectForInfoDictionaryKey:@"UILaunchScreen"] != nil;
        checked = YES;
    }

    return cached;
}

//-------------------------------------
bool
mfb_get_display_cutout_insets(struct mfb_window *window, int *left, int *top, int *right, int *bottom) {
    if (left)   *left   = 0;
    if (top)    *top    = 0;
    if (right)  *right  = 0;
    if (bottom) *bottom = 0;

    if (window == NULL) {
        return false;
    }

    UIWindow *app_window = get_application_window();
    if (app_window == nil) {
        return false;
    }

    // Without a launch screen (UILaunchStoryboardName / UILaunchScreen) iOS may run
    // the app in compatibility mode: the window coordinate space can differ from the
    // physical screen, making safeAreaInsets unreliable for cutout detection.
    if (!has_launch_screen()) {
        MFB_LOG(MFB_LOG_WARNING, "mfb_get_display_cutout_insets: no launch screen configured - insets are unreliable in compatibility mode.");
        return true; // valid call, but all insets remain 0
    }

    UIEdgeInsets insets = app_window.safeAreaInsets;

    // iOS has no public API to query the physical cutout separately from system bars.
    // Heuristic: iPhones without a home button (iPhone X / Dynamic Island models) have
    // a non-zero bottom safe-area inset (home indicator) in portrait orientation.
    // On those devices the top/left/right safe-area insets are caused by the physical
    // cutout; the bottom inset is the home indicator and is intentionally excluded here.
    BOOL has_cutout = (UIDevice.currentDevice.userInterfaceIdiom == UIUserInterfaceIdiomPhone)
                   && (insets.bottom > 0.0f);

    if (!has_cutout) {
        return true; // valid answer: device has no physical cutout
    }

    CGFloat scale = UIScreen.mainScreen.scale;
    if (left)   *left   = (int) round(insets.left  * scale);
    if (top)    *top    = (int) round(insets.top   * scale);
    if (right)  *right  = (int) round(insets.right * scale);
    // bottom intentionally left 0: home indicator is not a physical screen obstruction

    return true;
}

//-------------------------------------
bool
mfb_get_display_safe_insets(struct mfb_window *window, int *left, int *top, int *right, int *bottom) {
    if (left)   *left   = 0;
    if (top)    *top    = 0;
    if (right)  *right  = 0;
    if (bottom) *bottom = 0;

    if (window == NULL) {
        return false;
    }

    UIWindow *app_window = get_application_window();
    if (app_window == nil) {
        return false;
    }

    if (!has_launch_screen()) {
        MFB_LOG(MFB_LOG_WARNING, "mfb_get_display_safe_insets: no launch screen configured - insets are unreliable in compatibility mode.");
        return true; // valid call, but all insets remain 0
    }

    UIEdgeInsets insets = app_window.safeAreaInsets;
    CGFloat scale = UIScreen.mainScreen.scale;

    if (left)   *left   = (int) round(insets.left   * scale);
    if (top)    *top    = (int) round(insets.top    * scale);
    if (right)  *right  = (int) round(insets.right  * scale);
    if (bottom) *bottom = (int) round(insets.bottom * scale);

    return true;
}
