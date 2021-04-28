#include <Cocoa/Cocoa.h>
#if defined(USE_METAL_API)
#include <Carbon/Carbon.h>
#include <MetalKit/MetalKit.h>
#endif
#include <unistd.h>
#include <sched.h>
#include <mach/mach_time.h>

#include "OSXWindow.h"
#include "OSXView.h"
#include "OSXViewDelegate.h"
#include "WindowData_OSX.h"
#include <MiniFB.h>
#include <MiniFB_internal.h>
#include <MiniFB_enums.h>

//-------------------------------------
void init_keycodes();

//-------------------------------------
SWindowData *
create_window_data(unsigned width, unsigned height) {
    SWindowData *window_data;

    window_data = malloc(sizeof(SWindowData));
    if(window_data == 0x0) {
        NSLog(@"Cannot allocate window data");
        return 0x0;
    }
    memset(window_data, 0, sizeof(SWindowData));

    SWindowData_OSX *window_data_osx = malloc(sizeof(SWindowData_OSX));
    if(window_data_osx == 0x0) {
        free(window_data);
        NSLog(@"Cannot allocate osx window data");
        return 0x0;
    }
    memset(window_data_osx, 0, sizeof(SWindowData_OSX));

    window_data->specific = window_data_osx;

    calc_dst_factor(window_data, width, height);

    window_data->buffer_width  = width;
    window_data->buffer_height = height;
    window_data->buffer_stride = width * 4;

#if defined(USE_METAL_API)
    window_data->draw_buffer = malloc(width * height * 4);
    if (!window_data->draw_buffer) {
        free(window_data_osx);
        free(window_data);
        NSLog(@"Unable to create draw buffer");
        return 0x0;
    }
#endif

    return window_data;
}

//-------------------------------------
struct mfb_window *
mfb_open_ex(const char *title, unsigned width, unsigned height, unsigned flags) {
    @autoreleasepool {
        SWindowData *window_data = create_window_data(width, height);
        if (window_data == 0x0) {
            return 0x0;
        }
        SWindowData_OSX *window_data_osx = (SWindowData_OSX *) window_data->specific;

        init_keycodes();

        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

        NSRect              rectangle, frameRect;
        NSWindowStyleMask   styles = 0;

        if (flags & WF_BORDERLESS) {
            styles |= NSWindowStyleMaskBorderless;
        }
        else {
            styles |= NSWindowStyleMaskClosable | NSWindowStyleMaskTitled;
        }

        if (flags & WF_RESIZABLE)
            styles |= NSWindowStyleMaskResizable;

        if (flags & WF_FULLSCREEN) {
            styles = NSWindowStyleMaskFullScreen;
            NSScreen *mainScreen = [NSScreen mainScreen];
            NSRect screenRect = [mainScreen frame];
            window_data->window_width  = screenRect.size.width;
            window_data->window_height = screenRect.size.height;
            rectangle = NSMakeRect(0, 0, window_data->window_width, window_data->window_height);
            frameRect = rectangle;
        }
        else if (flags & WF_FULLSCREEN_DESKTOP) {
            NSScreen *mainScreen = [NSScreen mainScreen];
            NSRect screenRect = [mainScreen visibleFrame];
            window_data->window_width  = screenRect.size.width;
            window_data->window_height = screenRect.size.height;
            rectangle = NSMakeRect(0, 0, window_data->window_width, window_data->window_height);
            frameRect = rectangle;
        }
        else {
            window_data->window_width  = width;
            window_data->window_height = height;
            rectangle = NSMakeRect(0, 0, window_data->window_width, window_data->window_height);
            frameRect = [NSWindow frameRectForContentRect:rectangle styleMask:styles];
        }

        window_data_osx->window = [[OSXWindow alloc] initWithContentRect:frameRect styleMask:styles backing:NSBackingStoreBuffered defer:NO windowData:window_data];
        if (!window_data_osx->window) {
            NSLog(@"Cannot create window");
            if(window_data->draw_buffer != 0x0) {
                free(window_data->draw_buffer);
                window_data->draw_buffer = 0x0;
            }
            free(window_data_osx);
            free(window_data);
            return 0x0;
        }

    #if defined(USE_METAL_API)
        window_data_osx->viewController = [[OSXViewDelegate alloc] initWithWindowData:window_data];

        MTKView* view = [[MTKView alloc] initWithFrame:rectangle];
        view.device   = window_data_osx->viewController->metal_device;
        view.delegate = window_data_osx->viewController;
        view.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        [window_data_osx->window.contentView addSubview:view];

        //[window_data->window updateSize];
    #endif

        [window_data_osx->window setTitle:[NSString stringWithUTF8String:title]];
        [window_data_osx->window setReleasedWhenClosed:NO];
        [window_data_osx->window performSelectorOnMainThread:@selector(makeKeyAndOrderFront:) withObject:nil waitUntilDone:YES];
        [window_data_osx->window setAcceptsMouseMovedEvents:YES];

        [window_data_osx->window center];
        window_data_osx->timer = mfb_timer_create();

        [NSApp activateIgnoringOtherApps:YES];

    #if defined(USE_METAL_API)
        [NSApp finishLaunching];
    #endif

        mfb_set_keyboard_callback((struct mfb_window *) window_data, keyboard_default);

#if defined(_DEBUG) || defined(DEBUG)
    #if defined(USE_METAL_API)
        NSLog(@"Window created using Metal API");
    #else
        NSLog(@"Window created using Cocoa API");
    #endif
#endif

        window_data->is_initialized = true;
        return (struct mfb_window *) window_data;
    }
}

//-------------------------------------
static void
destroy_window_data(SWindowData *window_data) {
    if(window_data == 0x0)
        return;

    @autoreleasepool {
        SWindowData_OSX   *window_data_osx = (SWindowData_OSX *) window_data->specific;
        if(window_data_osx != 0x0) {
            OSXWindow   *window = window_data_osx->window;
            [window performClose:nil];

            // Flush events!
            NSEvent* event;
            do {
                event = [NSApp nextEventMatchingMask:NSEventMaskAny untilDate:[NSDate distantPast] inMode:NSDefaultRunLoopMode dequeue:YES];
                if (event) {
                    [NSApp sendEvent:event];
                }
            } while (event);
            [window removeWindowData];

            mfb_timer_destroy(window_data_osx->timer);

            memset(window_data_osx, 0, sizeof(SWindowData_OSX));
            free(window_data_osx);
        }

#if defined(USE_METAL_API)
        if(window_data->draw_buffer != 0x0) {
            free(window_data->draw_buffer);
            window_data->draw_buffer = 0x0;
        }
#endif

        memset(window_data, 0, sizeof(SWindowData));
        free(window_data);
    }
}

//-------------------------------------
static void
update_events(SWindowData *window_data) {
    NSEvent* event;

    @autoreleasepool {
        do {
            event = [NSApp nextEventMatchingMask:NSEventMaskAny untilDate:[NSDate distantPast] inMode:NSDefaultRunLoopMode dequeue:YES];
            if (event) {
                [NSApp sendEvent:event];
            }
        } while ((window_data->close == false) && event);
    }
}

//-------------------------------------
mfb_update_state
mfb_update_ex(struct mfb_window *window, void *buffer, unsigned width, unsigned height) {
    if(window == 0x0) {
        return STATE_INVALID_WINDOW;
    }

    SWindowData *window_data = (SWindowData *) window;
    if(window_data->close) {
        destroy_window_data(window_data);
        return STATE_EXIT;
    }

    if(buffer == 0x0) {
        return STATE_INVALID_BUFFER;
    }

    SWindowData_OSX *window_data_osx = (SWindowData_OSX *) window_data->specific;

#if defined(USE_METAL_API)
    if(window_data->buffer_width != width || window_data->buffer_height != height) {
        window_data->buffer_width  = width;
        window_data->buffer_stride = width * 4;
        window_data->buffer_height = height;
        window_data->draw_buffer   = realloc(window_data->draw_buffer, window_data->buffer_stride * window_data->buffer_height);

        [window_data_osx->viewController resizeTextures];
    }

    memcpy(window_data->draw_buffer, buffer, window_data->buffer_stride * window_data->buffer_height);
#else
    if(window_data->buffer_width != width || window_data->buffer_height != height) {
        window_data->buffer_width  = width;
        window_data->buffer_stride = width * 4;
        window_data->buffer_height = height;
    }

    window_data->draw_buffer = buffer;
#endif

    update_events(window_data);
    if(window_data->close) {
        destroy_window_data(window_data);
        return STATE_EXIT;
    }

    [[window_data_osx->window contentView] setNeedsDisplay:YES];

    return STATE_OK;
}

//-------------------------------------
mfb_update_state
mfb_update_events(struct mfb_window *window) {
    if(window == 0x0) {
        return STATE_INVALID_WINDOW;
    }

    SWindowData *window_data = (SWindowData *) window;
    if(window_data->close) {
        destroy_window_data(window_data);
        return STATE_EXIT;
    }

    update_events(window_data);
    if(window_data->close) {
        destroy_window_data(window_data);
        return STATE_EXIT;
    }

    SWindowData_OSX *window_data_osx = (SWindowData_OSX *) window_data->specific;
    [[window_data_osx->window contentView] setNeedsDisplay:YES];

    return STATE_OK;
}

//-------------------------------------
extern double   g_time_for_frame;
extern bool     g_use_hardware_sync;

bool
mfb_wait_sync(struct mfb_window *window) {
    NSEvent* event;

    if(window == 0x0) {
        return false;
    }

    SWindowData *window_data = (SWindowData *) window;
    if(window_data->close) {
        destroy_window_data(window_data);
        return false;
    }

    if(g_use_hardware_sync) {
        return true;
    }

    @autoreleasepool {
        SWindowData_OSX *window_data_osx = (SWindowData_OSX *) window_data->specific;
        if(window_data_osx == 0x0) {
            return false;
        }

        double      current;
        uint32_t    millis = 1;
        while(1) {
            current = mfb_timer_now(window_data_osx->timer);
            if (current >= g_time_for_frame * 0.96) {
                mfb_timer_reset(window_data_osx->timer);
                return true;
            }
            else if(current >= g_time_for_frame * 0.8) {
                millis = 0;
            }

            usleep(millis * 1000);
            //sched_yield();

            if(millis == 1) {
                event = [NSApp nextEventMatchingMask:NSEventMaskAny untilDate:[NSDate distantPast] inMode:NSDefaultRunLoopMode dequeue:YES];
                if (event) {
                    [NSApp sendEvent:event];

                    if(window_data->close) {
                        destroy_window_data(window_data);
                        return false;
                    }
                }
            }
        }
    }

    return true;
}

//-------------------------------------
bool
mfb_set_viewport(struct mfb_window *window, unsigned offset_x, unsigned offset_y, unsigned width, unsigned height) {
    if(window == 0x0) {
        return false;
    }

    SWindowData *window_data = (SWindowData *) window;

    if(offset_x + width > window_data->window_width) {
        return false;
    }
    if(offset_y + height > window_data->window_height) {
        return false;
    }

    window_data->dst_offset_x = offset_x;
    window_data->dst_offset_y = offset_y;
    window_data->dst_width    = width;
    window_data->dst_height   = height;
    calc_dst_factor(window_data, window_data->window_width, window_data->window_height);

#if defined(USE_METAL_API)
    float x1 =  ((float) offset_x           / window_data->window_width)  * 2.0f - 1.0f;
    float x2 = (((float) offset_x + width)  / window_data->window_width)  * 2.0f - 1.0f;
    float y1 =  ((float) offset_y           / window_data->window_height) * 2.0f - 1.0f;
    float y2 = (((float) offset_y + height) / window_data->window_height) * 2.0f - 1.0f;

    SWindowData_OSX *window_data_osx = (SWindowData_OSX *) window_data->specific;

    window_data_osx->metal.vertices[0].x = x1;
    window_data_osx->metal.vertices[0].y = y1;

    window_data_osx->metal.vertices[1].x = x1;
    window_data_osx->metal.vertices[1].y = y2;

    window_data_osx->metal.vertices[2].x = x2;
    window_data_osx->metal.vertices[2].y = y1;

    window_data_osx->metal.vertices[3].x = x2;
    window_data_osx->metal.vertices[3].y = y2;
#endif

    return true;
}

//-------------------------------------
extern short int g_keycodes[512];

void
init_keycodes() {
    // Clear keys
    for (unsigned int i = 0; i < sizeof(g_keycodes) / sizeof(g_keycodes[0]); ++i)
        g_keycodes[i] = 0;

    g_keycodes[0x1D] = KB_KEY_0;
    g_keycodes[0x12] = KB_KEY_1;
    g_keycodes[0x13] = KB_KEY_2;
    g_keycodes[0x14] = KB_KEY_3;
    g_keycodes[0x15] = KB_KEY_4;
    g_keycodes[0x17] = KB_KEY_5;
    g_keycodes[0x16] = KB_KEY_6;
    g_keycodes[0x1A] = KB_KEY_7;
    g_keycodes[0x1C] = KB_KEY_8;
    g_keycodes[0x19] = KB_KEY_9;
    g_keycodes[0x00] = KB_KEY_A;
    g_keycodes[0x0B] = KB_KEY_B;
    g_keycodes[0x08] = KB_KEY_C;
    g_keycodes[0x02] = KB_KEY_D;
    g_keycodes[0x0E] = KB_KEY_E;
    g_keycodes[0x03] = KB_KEY_F;
    g_keycodes[0x05] = KB_KEY_G;
    g_keycodes[0x04] = KB_KEY_H;
    g_keycodes[0x22] = KB_KEY_I;
    g_keycodes[0x26] = KB_KEY_J;
    g_keycodes[0x28] = KB_KEY_K;
    g_keycodes[0x25] = KB_KEY_L;
    g_keycodes[0x2E] = KB_KEY_M;
    g_keycodes[0x2D] = KB_KEY_N;
    g_keycodes[0x1F] = KB_KEY_O;
    g_keycodes[0x23] = KB_KEY_P;
    g_keycodes[0x0C] = KB_KEY_Q;
    g_keycodes[0x0F] = KB_KEY_R;
    g_keycodes[0x01] = KB_KEY_S;
    g_keycodes[0x11] = KB_KEY_T;
    g_keycodes[0x20] = KB_KEY_U;
    g_keycodes[0x09] = KB_KEY_V;
    g_keycodes[0x0D] = KB_KEY_W;
    g_keycodes[0x07] = KB_KEY_X;
    g_keycodes[0x10] = KB_KEY_Y;
    g_keycodes[0x06] = KB_KEY_Z;

    g_keycodes[0x27] = KB_KEY_APOSTROPHE;
    g_keycodes[0x2A] = KB_KEY_BACKSLASH;
    g_keycodes[0x2B] = KB_KEY_COMMA;
    g_keycodes[0x18] = KB_KEY_EQUAL;
    g_keycodes[0x32] = KB_KEY_GRAVE_ACCENT;
    g_keycodes[0x21] = KB_KEY_LEFT_BRACKET;
    g_keycodes[0x1B] = KB_KEY_MINUS;
    g_keycodes[0x2F] = KB_KEY_PERIOD;
    g_keycodes[0x1E] = KB_KEY_RIGHT_BRACKET;
    g_keycodes[0x29] = KB_KEY_SEMICOLON;
    g_keycodes[0x2C] = KB_KEY_SLASH;
    g_keycodes[0x0A] = KB_KEY_WORLD_1;

    g_keycodes[0x33] = KB_KEY_BACKSPACE;
    g_keycodes[0x39] = KB_KEY_CAPS_LOCK;
    g_keycodes[0x75] = KB_KEY_DELETE;
    g_keycodes[0x7D] = KB_KEY_DOWN;
    g_keycodes[0x77] = KB_KEY_END;
    g_keycodes[0x24] = KB_KEY_ENTER;
    g_keycodes[0x35] = KB_KEY_ESCAPE;
    g_keycodes[0x7A] = KB_KEY_F1;
    g_keycodes[0x78] = KB_KEY_F2;
    g_keycodes[0x63] = KB_KEY_F3;
    g_keycodes[0x76] = KB_KEY_F4;
    g_keycodes[0x60] = KB_KEY_F5;
    g_keycodes[0x61] = KB_KEY_F6;
    g_keycodes[0x62] = KB_KEY_F7;
    g_keycodes[0x64] = KB_KEY_F8;
    g_keycodes[0x65] = KB_KEY_F9;
    g_keycodes[0x6D] = KB_KEY_F10;
    g_keycodes[0x67] = KB_KEY_F11;
    g_keycodes[0x6F] = KB_KEY_F12;
    g_keycodes[0x69] = KB_KEY_F13;
    g_keycodes[0x6B] = KB_KEY_F14;
    g_keycodes[0x71] = KB_KEY_F15;
    g_keycodes[0x6A] = KB_KEY_F16;
    g_keycodes[0x40] = KB_KEY_F17;
    g_keycodes[0x4F] = KB_KEY_F18;
    g_keycodes[0x50] = KB_KEY_F19;
    g_keycodes[0x5A] = KB_KEY_F20;
    g_keycodes[0x73] = KB_KEY_HOME;
    g_keycodes[0x72] = KB_KEY_INSERT;
    g_keycodes[0x7B] = KB_KEY_LEFT;
    g_keycodes[0x3A] = KB_KEY_LEFT_ALT;
    g_keycodes[0x3B] = KB_KEY_LEFT_CONTROL;
    g_keycodes[0x38] = KB_KEY_LEFT_SHIFT;
    g_keycodes[0x37] = KB_KEY_LEFT_SUPER;
    g_keycodes[0x6E] = KB_KEY_MENU;
    g_keycodes[0x47] = KB_KEY_NUM_LOCK;
    g_keycodes[0x79] = KB_KEY_PAGE_DOWN;
    g_keycodes[0x74] = KB_KEY_PAGE_UP;
    g_keycodes[0x7C] = KB_KEY_RIGHT;
    g_keycodes[0x3D] = KB_KEY_RIGHT_ALT;
    g_keycodes[0x3E] = KB_KEY_RIGHT_CONTROL;
    g_keycodes[0x3C] = KB_KEY_RIGHT_SHIFT;
    g_keycodes[0x36] = KB_KEY_RIGHT_SUPER;
    g_keycodes[0x31] = KB_KEY_SPACE;
    g_keycodes[0x30] = KB_KEY_TAB;
    g_keycodes[0x7E] = KB_KEY_UP;

    g_keycodes[0x52] = KB_KEY_KP_0;
    g_keycodes[0x53] = KB_KEY_KP_1;
    g_keycodes[0x54] = KB_KEY_KP_2;
    g_keycodes[0x55] = KB_KEY_KP_3;
    g_keycodes[0x56] = KB_KEY_KP_4;
    g_keycodes[0x57] = KB_KEY_KP_5;
    g_keycodes[0x58] = KB_KEY_KP_6;
    g_keycodes[0x59] = KB_KEY_KP_7;
    g_keycodes[0x5B] = KB_KEY_KP_8;
    g_keycodes[0x5C] = KB_KEY_KP_9;
    g_keycodes[0x45] = KB_KEY_KP_ADD;
    g_keycodes[0x41] = KB_KEY_KP_DECIMAL;
    g_keycodes[0x4B] = KB_KEY_KP_DIVIDE;
    g_keycodes[0x4C] = KB_KEY_KP_ENTER;
    g_keycodes[0x51] = KB_KEY_KP_EQUAL;
    g_keycodes[0x43] = KB_KEY_KP_MULTIPLY;
    g_keycodes[0x4E] = KB_KEY_KP_SUBTRACT;
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

    //return (time * s_timebase_info.numer) / s_timebase_info.denom;

    // Perform the arithmetic at 128-bit precision to avoid the overflow!
    uint64_t high    = (time >> 32) * timebase.numer;
    uint64_t highRem = ((high % timebase.denom) << 32) / timebase.denom;
    uint64_t low     = (time & 0xFFFFFFFFull) * timebase.numer / timebase.denom;
    high /= timebase.denom;

    return (high << 32) + highRem + low;
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
    float scale = 1.0f;

    if(window != 0x0) {
        SWindowData     *window_data     = (SWindowData *) window;
        SWindowData_OSX *window_data_osx = (SWindowData_OSX *) window_data->specific;

        scale = [window_data_osx->window backingScaleFactor];
    }
    else {
        scale = [[NSScreen mainScreen] backingScaleFactor];
    }

    if (scale_x) {
        *scale_x = scale;
        if(*scale_x == 0) {
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
