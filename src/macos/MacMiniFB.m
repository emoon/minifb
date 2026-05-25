#include <Cocoa/Cocoa.h>
#if defined(USE_METAL_API)
    #include <Carbon/Carbon.h>
    #include <MetalKit/MetalKit.h>
#endif
#include <unistd.h>
#include <sched.h>
#include <mach/mach_time.h>
#include <stdint.h>

#include "OSXWindow.h"
#include "OSXView.h"
#include "OSXViewDelegate.h"
#include "WindowData_OSX.h"
#include <MiniFB.h>
#include <MiniFB_internal.h>
#include <MiniFB_enums.h>

//-------------------------------------
void     init_keycodes();

//-------------------------------------
static void
destroy_window_data(SWindowData *window_data) {
    if (window_data == NULL)
        return;

    release_cpp_stub((struct mfb_window *) window_data);

    @autoreleasepool {
        SWindowData_OSX   *window_data_specific = (SWindowData_OSX *) window_data->specific;
        if (window_data_specific != NULL) {
            OSXWindow   *window = window_data_specific->window;

#if defined(USE_METAL_API)
            // Nil the MTKView delegate before closing the window to prevent
            // use-after-free if the display link fires during teardown.
            if (window != nil && window_data_specific->viewController != nil) {
                for (NSView *subview in window.contentView.subviews) {
                    if ([subview isKindOfClass:[MTKView class]]) {
                        MTKView *mtkView = (MTKView *) subview;
                        mtkView.paused = YES;
                        mtkView.delegate = nil;
                        break;
                    }
                }
            }
#endif

            if (window != nil) {
                [window removeWindowData];
                [window setDelegate:nil];
                [window close];
                [window release];
                window_data_specific->window = nil;
            }

#if defined(USE_METAL_API)
            if (window_data_specific->viewController != nil) {
                [window_data_specific->viewController release];
                window_data_specific->viewController = nil;
            }
#endif

            mfb_timer_destroy(window_data_specific->timer);
            window_data_specific->timer = NULL;

            memset(window_data_specific, 0, sizeof(SWindowData_OSX));
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
SWindowData *
create_window_data(unsigned width, unsigned height) {
    SWindowData *window_data;
    uint32_t buffer_stride = 0;
    size_t buffer_total_bytes = 0;

    if (!calculate_buffer_layout(width, height, &buffer_stride, &buffer_total_bytes)) {
        MFB_LOG(MFB_LOG_ERROR, "MacMiniFB: invalid window buffer size %ux%u.", width, height);
        return NULL;
    }

    window_data = malloc(sizeof(SWindowData));
    if (window_data == NULL) {
        MFB_LOG(MFB_LOG_ERROR, "MacMiniFB: failed to allocate SWindowData.");
        return NULL;
    }
    memset(window_data, 0, sizeof(SWindowData));

    SWindowData_OSX *window_data_specific = malloc(sizeof(SWindowData_OSX));
    if (window_data_specific == NULL) {
        free(window_data);
        MFB_LOG(MFB_LOG_ERROR, "MacMiniFB: failed to allocate SWindowData_OSX.");
        return NULL;
    }
    memset(window_data_specific, 0, sizeof(SWindowData_OSX));

    window_data->specific = window_data_specific;

    calc_dst_factor(window_data, width, height);

    window_data->buffer_width  = width;
    window_data->buffer_height = height;
    window_data->buffer_stride = buffer_stride;
    window_data->draw_buffer   = malloc(buffer_total_bytes);
    if (!window_data->draw_buffer) {
        free(window_data_specific);
        free(window_data);
        MFB_LOG(MFB_LOG_ERROR, "MacMiniFB: failed to allocate draw buffer (%zu bytes).", buffer_total_bytes);
        return NULL;
    }
    memset(window_data->draw_buffer, 0, buffer_total_bytes);
    window_data->is_cursor_visible = true;

    return window_data;
}

//-------------------------------------
enum { kMaxEventsPerMode = 64 };

//-------------------------------------
static void
update_events_for_mode(NSString *mode) {
    NSUInteger processed = 0;
    NSEvent *event;

    // Keep the frame loop responsive during live resize by bounding work per call.
    while (processed < kMaxEventsPerMode &&
           (event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                       untilDate:[NSDate distantPast]
                                          inMode:mode
                                         dequeue:YES])) {
        [NSApp sendEvent:event];
        ++processed;
    }
}

//-------------------------------------
static inline void
update_events(SWindowData *window_data) {
    if (window_data != NULL) {
        window_data->mouse_wheel_x = 0.0f;
        window_data->mouse_wheel_y = 0.0f;
    }

    @autoreleasepool {
        update_events_for_mode(NSDefaultRunLoopMode);
        update_events_for_mode(NSEventTrackingRunLoopMode);
        update_events_for_mode(NSModalPanelRunLoopMode);
    }
}

//-------------------------------------
static inline void
dispatch_pending_resize(SWindowData *window_data) {
#if defined(USE_METAL_API)
    if (window_data != NULL && window_data->must_resize_context) {
        window_data->must_resize_context = false;
        kCall(resize_func, (int) window_data->window_width, (int) window_data->window_height);
    }
#else
    kUnused(window_data);
#endif
}

//-------------------------------------
struct mfb_window *
mfb_open_ex(const char *title, unsigned width, unsigned height, unsigned flags) {
    @autoreleasepool {
        const unsigned known_flags = MFB_WF_RESIZABLE | MFB_WF_FULLSCREEN | MFB_WF_FULLSCREEN_DESKTOP | MFB_WF_BORDERLESS | MFB_WF_ALWAYS_ON_TOP;
        unsigned effective_flags = flags;
        const char *window_title_c = (title != NULL && title[0] != '\0') ? title : "minifb";

        if (width == 0 || height == 0) {
            MFB_LOG(MFB_LOG_ERROR, "MacMiniFB: invalid window size %ux%u.", width, height);
            return NULL;
        }

        if ((effective_flags & ~known_flags) != 0u) {
            MFB_LOG(MFB_LOG_WARNING, "MacMiniFB: unknown window flags 0x%x will be ignored.", effective_flags & ~known_flags);
        }

        if ((effective_flags & MFB_WF_FULLSCREEN) && (effective_flags & MFB_WF_FULLSCREEN_DESKTOP)) {
            MFB_LOG(MFB_LOG_WARNING, "MacMiniFB: MFB_WF_FULLSCREEN and MFB_WF_FULLSCREEN_DESKTOP were both requested; MFB_WF_FULLSCREEN takes precedence.");
            effective_flags &= ~MFB_WF_FULLSCREEN_DESKTOP;
        }

        SWindowData *window_data = create_window_data(width, height);
        if (window_data == NULL) {
            return NULL;
        }
        SWindowData_OSX *window_data_specific = (SWindowData_OSX *) window_data->specific;

        init_keycodes();

        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

        NSRect              rectangle, frameRect;
        NSWindowStyleMask   styles = 0;
        bool                request_maximized_desktop = false;

        if (effective_flags & MFB_WF_BORDERLESS) {
            styles |= NSWindowStyleMaskBorderless;
        }
        else {
            styles |= NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskTitled;
        }

        if (effective_flags & MFB_WF_RESIZABLE)
            styles |= NSWindowStyleMaskResizable;

        if (effective_flags & MFB_WF_FULLSCREEN) {
            styles = NSWindowStyleMaskFullScreen;
            NSScreen *mainScreen = [NSScreen mainScreen];
            if (mainScreen == nil) {
                MFB_LOG(MFB_LOG_WARNING, "MacMiniFB: main screen unavailable for MFB_WF_FULLSCREEN; using requested size %ux%u.", width, height);
                window_data->window_width  = width;
                window_data->window_height = height;
            }
            else {
                NSRect screenRect = [mainScreen frame];
                window_data->window_width  = screenRect.size.width;
                window_data->window_height = screenRect.size.height;
            }
            rectangle = NSMakeRect(0, 0, window_data->window_width, window_data->window_height);
            frameRect = rectangle;
        }
        else if (effective_flags & MFB_WF_FULLSCREEN_DESKTOP) {
            request_maximized_desktop = true;
            styles |= NSWindowStyleMaskResizable;
            window_data->window_width  = width;
            window_data->window_height = height;
            rectangle = NSMakeRect(0, 0, window_data->window_width, window_data->window_height);
            frameRect = [NSWindow frameRectForContentRect:rectangle styleMask:styles];
        }
        else {
            window_data->window_width  = width;
            window_data->window_height = height;
            rectangle = NSMakeRect(0, 0, window_data->window_width, window_data->window_height);
            frameRect = [NSWindow frameRectForContentRect:rectangle styleMask:styles];
        }

        window_data_specific->window = [[OSXWindow alloc] initWithContentRect:frameRect styleMask:styles backing:NSBackingStoreBuffered defer:NO windowData:window_data];
        if (!window_data_specific->window) {
            MFB_LOG(MFB_LOG_ERROR, "MacMiniFB: failed to create OSXWindow.");
            destroy_window_data(window_data);
            return NULL;
        }
        [window_data_specific->window setReleasedWhenClosed:NO];
        if (effective_flags & MFB_WF_ALWAYS_ON_TOP) {
            [window_data_specific->window setLevel:NSFloatingWindowLevel];
        }

    #if defined(USE_METAL_API)
        window_data_specific->viewController = [[OSXViewDelegate alloc] initWithWindowData:window_data];
        if (window_data_specific->viewController == nil || window_data_specific->viewController->metal_device == nil) {
            MFB_LOG(MFB_LOG_ERROR, "MacMiniFB: failed to initialize Metal view controller.");
            destroy_window_data(window_data);
            return NULL;
        }

        MTKView* view = [[MTKView alloc] initWithFrame:rectangle];
        if (view == nil) {
            MFB_LOG(MFB_LOG_ERROR, "MacMiniFB: failed to create MTKView.");
            destroy_window_data(window_data);
            return NULL;
        }
        view.device   = window_data_specific->viewController->metal_device;
        if (view.device == nil) {
            MFB_LOG(MFB_LOG_ERROR, "MacMiniFB: MTKView has no Metal device.");
            [view release];
            destroy_window_data(window_data);
            return NULL;
        }
        view.delegate = window_data_specific->viewController;
        view.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        [window_data_specific->window.contentView addSubview:view];
        [view release];

        //[window_data->window updateSize];
    #endif

        NSString *window_title = [NSString stringWithUTF8String:window_title_c];
        if (window_title == nil) {
            MFB_LOG(MFB_LOG_WARNING, "MacMiniFB: window title is not valid UTF-8; falling back to default title.");
            window_title = @"minifb";
        }
        [window_data_specific->window setTitle:window_title];
        [window_data_specific->window performSelectorOnMainThread:@selector(makeKeyAndOrderFront:) withObject:nil waitUntilDone:YES];
        [window_data_specific->window setAcceptsMouseMovedEvents:YES];

        [window_data_specific->window center];
        if (request_maximized_desktop) {
            [window_data_specific->window performSelectorOnMainThread:@selector(performZoom:) withObject:nil waitUntilDone:YES];
        }
        window_data_specific->timer = mfb_timer_create();
        if (window_data_specific->timer == NULL) {
            MFB_LOG(MFB_LOG_ERROR, "MacMiniFB: failed to create frame timer.");
            destroy_window_data(window_data);
            return NULL;
        }

        [NSApp activateIgnoringOtherApps:YES];
        [NSApp finishLaunching];

        mfb_set_keyboard_callback((struct mfb_window *) window_data, keyboard_default);

#if defined(USE_METAL_API)
        MFB_LOG(MFB_LOG_DEBUG, "MacMiniFB: window created using Metal API (title='%s', size=%ux%u, flags=0x%x).",
                window_title_c, width, height, flags);
#else
        MFB_LOG(MFB_LOG_DEBUG, "MacMiniFB: window created using Cocoa API (title='%s', size=%ux%u, flags=0x%x).",
                window_title_c, width, height, flags);
#endif

        window_data->is_initialized = true;
        return (struct mfb_window *) window_data;
    }
}

//-------------------------------------
mfb_update_state
mfb_update_ex(struct mfb_window *window, void *buffer, unsigned width, unsigned height) {
    SWindowData *window_data = (SWindowData *) window;
    uint32_t buffer_stride = 0;
    size_t total_bytes = 0;

    if (window_data == NULL) {
        MFB_LOG(MFB_LOG_DEBUG, "MacMiniFB: mfb_update_ex called with an invalid window.");
        return MFB_STATE_INVALID_WINDOW;
    }

    // Early exit
    if (window_data->close) {
        MFB_LOG(MFB_LOG_DEBUG, "MacMiniFB: mfb_update_ex aborted because the window is marked for close.");
        destroy_window_data(window_data);
        return MFB_STATE_EXIT;
    }

    if (buffer == NULL) {
        MFB_LOG(MFB_LOG_ERROR, "MacMiniFB: mfb_update_ex called with a null buffer.");
        return MFB_STATE_INVALID_BUFFER;
    }
    if (!calculate_buffer_layout(width, height, &buffer_stride, &total_bytes)) {
        MFB_LOG(MFB_LOG_ERROR, "MacMiniFB: mfb_update_ex called with invalid buffer size %ux%u.", width, height);
        return MFB_STATE_INVALID_BUFFER;
    }

    SWindowData_OSX *window_data_specific = (SWindowData_OSX *) window_data->specific;
    if (window_data_specific ==  NULL) {
        MFB_LOG(MFB_LOG_ERROR, "MacMiniFB: missing macOS-specific window data in mfb_update_ex.");
        return MFB_STATE_INVALID_WINDOW;
    }
    if (window_data_specific->window == nil) {
        MFB_LOG(MFB_LOG_ERROR, "MacMiniFB: mfb_update_ex has a null OSXWindow handle.");
        return MFB_STATE_INVALID_WINDOW;
    }

    if (window_data->buffer_width != width || window_data->buffer_height != height) {
        void *new_draw_buffer = malloc(total_bytes);
        if (new_draw_buffer == NULL) {
            MFB_LOG(MFB_LOG_ERROR, "MacMiniFB: failed to allocate resized draw buffer (%zu bytes).", total_bytes);
            return MFB_STATE_INTERNAL_ERROR;
        }

#if defined(USE_METAL_API)
        unsigned previous_width  = window_data->buffer_width;
        unsigned previous_height = window_data->buffer_height;
        uint32_t previous_stride = window_data->buffer_stride;

        if (window_data_specific->viewController == nil) {
            MFB_LOG(MFB_LOG_ERROR, "MacMiniFB: Metal view controller is missing during buffer resize.");
            free(new_draw_buffer);
            return MFB_STATE_INVALID_WINDOW;
        }

        // Pause the MTKView to stop CVDisplayLink callbacks while we
        // replace the draw buffer and update dimensions.
        MTKView *mtkView = nil;
        OSXWindow *window = window_data_specific->window;
        if (window != nil) {
            for (NSView *subview in window.contentView.subviews) {
                if ([subview isKindOfClass:[MTKView class]]) {
                    mtkView = (MTKView *) subview;
                    mtkView.paused = YES;
                    break;
                }
            }
        }

        window_data->buffer_width  = width;
        window_data->buffer_height = height;
        window_data->buffer_stride = buffer_stride;

        if (![window_data_specific->viewController resizeTextures]) {
            MFB_LOG(MFB_LOG_ERROR, "MacMiniFB: failed to resize Metal textures after framebuffer resize.");
            window_data->buffer_width  = previous_width;
            window_data->buffer_height = previous_height;
            window_data->buffer_stride = previous_stride;
            if (mtkView != nil) {
                mtkView.paused = NO;
            }
            free(new_draw_buffer);
            return MFB_STATE_INTERNAL_ERROR;
        }

        free(window_data->draw_buffer);
        window_data->draw_buffer = new_draw_buffer;

        if (mtkView != nil) {
            mtkView.paused = NO;
        }
#else
        free(window_data->draw_buffer);
        window_data->draw_buffer = new_draw_buffer;

        window_data->buffer_width  = width;
        window_data->buffer_stride = buffer_stride;
        window_data->buffer_height = height;
#endif
    }

    if (window_data->draw_buffer == NULL) {
        MFB_LOG(MFB_LOG_ERROR, "MacMiniFB: internal draw buffer is null in mfb_update_ex.");
        return MFB_STATE_INTERNAL_ERROR;
    }
    // NOTE: The non-Metal path used to do zero-copy (draw_buffer = buffer), but memcpy is
    // intentional: drawRect: reads draw_buffer asynchronously via CGDataProvider, so we need
    // our own copy to avoid tearing or use-after-free on the user's buffer.
    memcpy(window_data->draw_buffer, buffer, total_bytes);

    update_events(window_data);
    if (window_data->close) {
        MFB_LOG(MFB_LOG_DEBUG, "MacMiniFB: mfb_update_ex detected close request after event processing.");
        destroy_window_data(window_data);
        return MFB_STATE_EXIT;
    }
    dispatch_pending_resize(window_data);

#if !defined(USE_METAL_API)
    // In non-Metal mode, signal the OSXView that it should redraw via drawRect:.
    // Metal rendering is driven by the MTKView render loop; setNeedsDisplay has no effect there.
    NSView *root_view = [window_data_specific->window rootContentView];
    if (root_view != nil) {
        [root_view setNeedsDisplay:YES];
    }
#endif

    return MFB_STATE_OK;
}

//-------------------------------------
mfb_update_state
mfb_update_events(struct mfb_window *window) {
    SWindowData *window_data = (SWindowData *) window;
    if (window_data == NULL) {
        MFB_LOG(MFB_LOG_DEBUG, "MacMiniFB: mfb_update_events called with an invalid window.");
        return MFB_STATE_INVALID_WINDOW;
    }
    if (window_data->close) {
        MFB_LOG(MFB_LOG_DEBUG, "MacMiniFB: mfb_update_events aborted because the window is marked for close.");
        destroy_window_data(window_data);
        return MFB_STATE_EXIT;
    }

    update_events(window_data);
    if (window_data->close) {
        MFB_LOG(MFB_LOG_DEBUG, "MacMiniFB: mfb_update_events detected close request after event processing.");
        destroy_window_data(window_data);
        return MFB_STATE_EXIT;
    }
    dispatch_pending_resize(window_data);

    SWindowData_OSX *window_data_specific = (SWindowData_OSX *) window_data->specific;
    if (window_data_specific == NULL || window_data_specific->window == nil) {
        MFB_LOG(MFB_LOG_ERROR, "MacMiniFB: mfb_update_events has invalid macOS window state.");
        return MFB_STATE_INVALID_WINDOW;
    }

#if !defined(USE_METAL_API)
    // In non-Metal mode, signal the OSXView that it should redraw via drawRect:.
    // Metal rendering is driven by the MTKView render loop; setNeedsDisplay has no effect there.
    NSView *root_view = [window_data_specific->window rootContentView];
    if (root_view != nil) {
        [root_view setNeedsDisplay:YES];
    }
#endif

    return MFB_STATE_OK;
}

//-------------------------------------
extern double   g_time_for_frame;
extern bool     g_use_hardware_sync;

//-------------------------------------
bool
mfb_wait_sync(struct mfb_window *window) {
    SWindowData *window_data = (SWindowData *) window;
    if (window_data == NULL) {
        MFB_LOG(MFB_LOG_DEBUG, "MacMiniFB: mfb_wait_sync called with an invalid window.");
        return false;
    }
    if (window_data->close) {
        MFB_LOG(MFB_LOG_DEBUG, "MacMiniFB: mfb_wait_sync aborted because the window is marked for close.");
        destroy_window_data(window_data);
        return false;
    }

    SWindowData_OSX *window_data_specific = (SWindowData_OSX *) window_data->specific;
    if (window_data_specific == NULL) {
        MFB_LOG(MFB_LOG_ERROR, "MacMiniFB: mfb_wait_sync missing macOS-specific window data.");
        return false;
    }
    if (window_data_specific->timer == NULL) {
        MFB_LOG(MFB_LOG_ERROR, "MacMiniFB: mfb_wait_sync missing frame timer state.");
        return false;
    }

    update_events(window_data);
    if (window_data->close) {
        MFB_LOG(MFB_LOG_DEBUG, "MacMiniFB: mfb_wait_sync detected close request after event processing.");
        destroy_window_data(window_data);
        return false;
    }

    // Hardware sync: no software pacing
    if (g_use_hardware_sync) {
        return true;
    }

    @autoreleasepool {
        // Software pacing: wait only the remaining time; wake on input
        for (;;) {
            double elapsed_time = mfb_timer_now(window_data_specific->timer);
            if (elapsed_time >= g_time_for_frame)
                break;

            double remaining_ms = (g_time_for_frame - elapsed_time) * 1000.0;

            if (remaining_ms > 1.5) {
                // Coarse wait with event pumping via RunLoop; leave ~1 ms margin
                CFTimeInterval timeout_s = (remaining_ms - 1.0) / 1000.0;
                if (timeout_s < 0.0)
                    timeout_s = 0.0;

                CFRunLoopRunInMode(kCFRunLoopDefaultMode, timeout_s, true);
            }
            else {
                sched_yield(); // small cooperative yield
            }

            update_events(window_data);
            if (window_data->close) {
                MFB_LOG(MFB_LOG_DEBUG, "MacMiniFB: mfb_wait_sync detected close request while waiting for frame sync.");
                destroy_window_data(window_data);
                return false;
            }
        }

        mfb_timer_compensated_reset(window_data_specific->timer);
        return true;
    }
}

//-------------------------------------
extern short int g_keycodes[MFB_MAX_KEYS];

//-------------------------------------
void
init_keycodes() {
    static bool s_initialized = false;
    if (s_initialized) {
        return;
    }
    s_initialized = true;

    for (size_t i = 0; i < MFB_MAX_KEYS; ++i) {
        g_keycodes[i] = MFB_KB_KEY_UNKNOWN;
    }

    g_keycodes[0x1D] = MFB_KB_KEY_0;
    g_keycodes[0x12] = MFB_KB_KEY_1;
    g_keycodes[0x13] = MFB_KB_KEY_2;
    g_keycodes[0x14] = MFB_KB_KEY_3;
    g_keycodes[0x15] = MFB_KB_KEY_4;
    g_keycodes[0x17] = MFB_KB_KEY_5;
    g_keycodes[0x16] = MFB_KB_KEY_6;
    g_keycodes[0x1A] = MFB_KB_KEY_7;
    g_keycodes[0x1C] = MFB_KB_KEY_8;
    g_keycodes[0x19] = MFB_KB_KEY_9;
    g_keycodes[0x00] = MFB_KB_KEY_A;
    g_keycodes[0x0B] = MFB_KB_KEY_B;
    g_keycodes[0x08] = MFB_KB_KEY_C;
    g_keycodes[0x02] = MFB_KB_KEY_D;
    g_keycodes[0x0E] = MFB_KB_KEY_E;
    g_keycodes[0x03] = MFB_KB_KEY_F;
    g_keycodes[0x05] = MFB_KB_KEY_G;
    g_keycodes[0x04] = MFB_KB_KEY_H;
    g_keycodes[0x22] = MFB_KB_KEY_I;
    g_keycodes[0x26] = MFB_KB_KEY_J;
    g_keycodes[0x28] = MFB_KB_KEY_K;
    g_keycodes[0x25] = MFB_KB_KEY_L;
    g_keycodes[0x2E] = MFB_KB_KEY_M;
    g_keycodes[0x2D] = MFB_KB_KEY_N;
    g_keycodes[0x1F] = MFB_KB_KEY_O;
    g_keycodes[0x23] = MFB_KB_KEY_P;
    g_keycodes[0x0C] = MFB_KB_KEY_Q;
    g_keycodes[0x0F] = MFB_KB_KEY_R;
    g_keycodes[0x01] = MFB_KB_KEY_S;
    g_keycodes[0x11] = MFB_KB_KEY_T;
    g_keycodes[0x20] = MFB_KB_KEY_U;
    g_keycodes[0x09] = MFB_KB_KEY_V;
    g_keycodes[0x0D] = MFB_KB_KEY_W;
    g_keycodes[0x07] = MFB_KB_KEY_X;
    g_keycodes[0x10] = MFB_KB_KEY_Y;
    g_keycodes[0x06] = MFB_KB_KEY_Z;

    g_keycodes[0x27] = MFB_KB_KEY_APOSTROPHE;
    g_keycodes[0x2A] = MFB_KB_KEY_BACKSLASH;
    g_keycodes[0x2B] = MFB_KB_KEY_COMMA;
    g_keycodes[0x18] = MFB_KB_KEY_EQUAL;
    g_keycodes[0x32] = MFB_KB_KEY_GRAVE_ACCENT;
    g_keycodes[0x21] = MFB_KB_KEY_LEFT_BRACKET;
    g_keycodes[0x1B] = MFB_KB_KEY_MINUS;
    g_keycodes[0x2F] = MFB_KB_KEY_PERIOD;
    g_keycodes[0x1E] = MFB_KB_KEY_RIGHT_BRACKET;
    g_keycodes[0x29] = MFB_KB_KEY_SEMICOLON;
    g_keycodes[0x2C] = MFB_KB_KEY_SLASH;
    g_keycodes[0x0A] = MFB_KB_KEY_WORLD_1;

    g_keycodes[0x33] = MFB_KB_KEY_BACKSPACE;
    g_keycodes[0x39] = MFB_KB_KEY_CAPS_LOCK;
    g_keycodes[0x75] = MFB_KB_KEY_DELETE;
    g_keycodes[0x7D] = MFB_KB_KEY_DOWN;
    g_keycodes[0x77] = MFB_KB_KEY_END;
    g_keycodes[0x24] = MFB_KB_KEY_ENTER;
    g_keycodes[0x35] = MFB_KB_KEY_ESCAPE;
    g_keycodes[0x7A] = MFB_KB_KEY_F1;
    g_keycodes[0x78] = MFB_KB_KEY_F2;
    g_keycodes[0x63] = MFB_KB_KEY_F3;
    g_keycodes[0x76] = MFB_KB_KEY_F4;
    g_keycodes[0x60] = MFB_KB_KEY_F5;
    g_keycodes[0x61] = MFB_KB_KEY_F6;
    g_keycodes[0x62] = MFB_KB_KEY_F7;
    g_keycodes[0x64] = MFB_KB_KEY_F8;
    g_keycodes[0x65] = MFB_KB_KEY_F9;
    g_keycodes[0x6D] = MFB_KB_KEY_F10;
    g_keycodes[0x67] = MFB_KB_KEY_F11;
    g_keycodes[0x6F] = MFB_KB_KEY_F12;
    g_keycodes[0x69] = MFB_KB_KEY_F13;
    g_keycodes[0x6B] = MFB_KB_KEY_F14;
    g_keycodes[0x71] = MFB_KB_KEY_F15;
    g_keycodes[0x6A] = MFB_KB_KEY_F16;
    g_keycodes[0x40] = MFB_KB_KEY_F17;
    g_keycodes[0x4F] = MFB_KB_KEY_F18;
    g_keycodes[0x50] = MFB_KB_KEY_F19;
    g_keycodes[0x5A] = MFB_KB_KEY_F20;
    g_keycodes[0x73] = MFB_KB_KEY_HOME;
    g_keycodes[0x72] = MFB_KB_KEY_INSERT;
    g_keycodes[0x7B] = MFB_KB_KEY_LEFT;
    g_keycodes[0x3A] = MFB_KB_KEY_LEFT_ALT;
    g_keycodes[0x3B] = MFB_KB_KEY_LEFT_CONTROL;
    g_keycodes[0x38] = MFB_KB_KEY_LEFT_SHIFT;
    g_keycodes[0x37] = MFB_KB_KEY_LEFT_SUPER;
    g_keycodes[0x6E] = MFB_KB_KEY_MENU;
    g_keycodes[0x47] = MFB_KB_KEY_NUM_LOCK;
    g_keycodes[0x79] = MFB_KB_KEY_PAGE_DOWN;
    g_keycodes[0x74] = MFB_KB_KEY_PAGE_UP;
    g_keycodes[0x7C] = MFB_KB_KEY_RIGHT;
    g_keycodes[0x3D] = MFB_KB_KEY_RIGHT_ALT;
    g_keycodes[0x3E] = MFB_KB_KEY_RIGHT_CONTROL;
    g_keycodes[0x3C] = MFB_KB_KEY_RIGHT_SHIFT;
    g_keycodes[0x36] = MFB_KB_KEY_RIGHT_SUPER;
    g_keycodes[0x31] = MFB_KB_KEY_SPACE;
    g_keycodes[0x30] = MFB_KB_KEY_TAB;
    g_keycodes[0x7E] = MFB_KB_KEY_UP;

    g_keycodes[0x52] = MFB_KB_KEY_KP_0;
    g_keycodes[0x53] = MFB_KB_KEY_KP_1;
    g_keycodes[0x54] = MFB_KB_KEY_KP_2;
    g_keycodes[0x55] = MFB_KB_KEY_KP_3;
    g_keycodes[0x56] = MFB_KB_KEY_KP_4;
    g_keycodes[0x57] = MFB_KB_KEY_KP_5;
    g_keycodes[0x58] = MFB_KB_KEY_KP_6;
    g_keycodes[0x59] = MFB_KB_KEY_KP_7;
    g_keycodes[0x5B] = MFB_KB_KEY_KP_8;
    g_keycodes[0x5C] = MFB_KB_KEY_KP_9;
    g_keycodes[0x45] = MFB_KB_KEY_KP_ADD;
    g_keycodes[0x41] = MFB_KB_KEY_KP_DECIMAL;
    g_keycodes[0x4B] = MFB_KB_KEY_KP_DIVIDE;
    g_keycodes[0x4C] = MFB_KB_KEY_KP_ENTER;
    g_keycodes[0x51] = MFB_KB_KEY_KP_EQUAL;
    g_keycodes[0x43] = MFB_KB_KEY_KP_MULTIPLY;
    g_keycodes[0x4E] = MFB_KB_KEY_KP_SUBTRACT;
}

//-------------------------------------
bool
mfb_set_viewport(struct mfb_window *window, unsigned offset_x, unsigned offset_y, unsigned width, unsigned height) {
    SWindowData *window_data = (SWindowData *) window;
    if (!mfb_validate_viewport(window_data, offset_x, offset_y, width, height, "MacMiniFB")) {
        return false;
    }

    window_data->dst_offset_x = offset_x;
    window_data->dst_offset_y = offset_y;
    window_data->dst_width    = width;
    window_data->dst_height   = height;
    calc_dst_factor(window_data, window_data->window_width, window_data->window_height);

#if defined(USE_METAL_API)
    SWindowData_OSX *window_data_specific = (SWindowData_OSX *) window_data->specific;
    if (window_data_specific == NULL) {
        MFB_LOG(MFB_LOG_ERROR, "MacMiniFB: mfb_set_viewport missing macOS-specific window data.");
        return false;
    }

    float x1 =  ((float) offset_x           / window_data->window_width)  * 2.0f - 1.0f;
    float x2 = (((float) offset_x + width)  / window_data->window_width)  * 2.0f - 1.0f;
    float y1 =  ((float) offset_y           / window_data->window_height) * 2.0f - 1.0f;
    float y2 = (((float) offset_y + height) / window_data->window_height) * 2.0f - 1.0f;

    window_data_specific->metal.vertices[0].x = x1;
    window_data_specific->metal.vertices[0].y = y1;

    window_data_specific->metal.vertices[1].x = x1;
    window_data_specific->metal.vertices[1].y = y2;

    window_data_specific->metal.vertices[2].x = x2;
    window_data_specific->metal.vertices[2].y = y1;

    window_data_specific->metal.vertices[3].x = x2;
    window_data_specific->metal.vertices[3].y = y2;
#endif

    return true;
}

//-------------------------------------
void
mfb_set_title(struct mfb_window *window, const char *title) {
    if (window == 0x0 || title == 0x0) {
        return;
    }

    SWindowData *window_data = (SWindowData *) window;
    SWindowData_OSX *window_data_specific = (SWindowData_OSX *) window_data->specific;
    if (window_data_specific == 0x0) {
        return;
    }

    @autoreleasepool {
        [window_data_specific->window setTitle:[NSString stringWithUTF8String:title]];
    }
}

//-------------------------------------
void
mfb_get_monitor_scale(struct mfb_window *window, float *scale_x, float *scale_y) {
    float scale = 1.0f;

    if (window != NULL) {
        SWindowData     *window_data = (SWindowData *) window;
        SWindowData_OSX *window_data_specific = (SWindowData_OSX *) window_data->specific;
        if (window_data_specific != NULL && window_data_specific->window != nil) {
            scale = [window_data_specific->window backingScaleFactor];
        }
        else {
            MFB_LOG(MFB_LOG_WARNING, "MacMiniFB: missing macOS window handle; falling back to main screen scale.");
            scale = [[NSScreen mainScreen] backingScaleFactor];
        }
    }
    else {
        scale = [[NSScreen mainScreen] backingScaleFactor];
    }

    if (scale_x) {
        *scale_x = scale;
        if (*scale_x == 0) {
            *scale_x = 1;
        }
    }

    if (scale_y) {
        *scale_y = scale;
        if (*scale_y == 0) {
            *scale_y = 1;
        }
    }
}

//-------------------------------------
extern double   g_timer_frequency;
extern double   g_timer_resolution;

//-------------------------------------
uint64_t
mfb_timer_tick() {
    static mach_timebase_info_data_t    timebase = { 0 };

    if (timebase.denom == 0) {
        (void) mach_timebase_info(&timebase);
    }

    uint64_t time = mach_absolute_time();

    //return (time * s_timebase_info.numer) / s_timebase_info.denom;

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
void mfb_show_cursor(struct mfb_window *window, bool show) {
    SWindowData *window_data = (SWindowData *) window;
    if (window_data == NULL) {
        MFB_LOG(MFB_LOG_ERROR, "MacMiniFB: mfb_show_cursor called with a null window pointer.");
        return;
    }

    @autoreleasepool {
        if (window_data->is_cursor_visible != show) {
            window_data->is_cursor_visible = show;

            // Update cursor rects on the window so we can use a per-window
            // invisible cursor instead of hiding the global cursor.
            SWindowData_OSX *window_data_specific = (SWindowData_OSX *) window_data->specific;
            if (window_data_specific && window_data_specific->window) {
                [window_data_specific->window performSelectorOnMainThread:@selector(updateCursorRects) withObject:nil waitUntilDone:YES];
            }
            else {
                MFB_LOG(MFB_LOG_ERROR, "MacMiniFB: mfb_show_cursor cannot update cursor rects (missing macOS window state).");
            }
        }
    }
}
