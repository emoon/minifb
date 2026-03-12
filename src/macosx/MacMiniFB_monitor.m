#include <Cocoa/Cocoa.h>
#include <MiniFB.h>
#include <MiniFB_internal.h>
#include <WindowData.h>
#include "WindowData_OSX.h"

#include <stdio.h>
#include <string.h>

// Returns the NSScreen array sorted so that the main (primary) screen is first.
static NSArray<NSScreen *> *
sorted_screens(void) {
    NSArray<NSScreen *> *screens = [NSScreen screens];
    NSScreen *primary = [NSScreen mainScreen];
    if ([screens count] == 0 || screens[0] == primary)
        return screens;

    NSMutableArray<NSScreen *> *sorted = [screens mutableCopy];
    [sorted removeObject:primary];
    [sorted insertObject:primary atIndex:0];
    return sorted;
}

static void
fill_info_from_screen(NSScreen *screen, bool is_primary, mfb_monitor_info *out_info) {
    NSRect  frame = [screen frame];
    CGFloat scale = [screen backingScaleFactor];

    out_info->logical_x      = (int) frame.origin.x;
    out_info->logical_y      = (int) frame.origin.y;
    out_info->logical_width  = (unsigned) frame.size.width;
    out_info->logical_height = (unsigned) frame.size.height;
    out_info->physical_width  = (unsigned) (frame.size.width  * scale);
    out_info->physical_height = (unsigned) (frame.size.height * scale);
    out_info->scale_x        = (float) scale;
    out_info->scale_y        = (float) scale;
    out_info->is_primary     = is_primary;

    // localizedName is macOS 10.15+; fall back to an empty string on older systems.
    NSString *n = @"";
    if ([screen respondsToSelector:@selector(localizedName)])
        n = [screen localizedName];
    strncpy(out_info->name, [n UTF8String], sizeof(out_info->name) - 1);
    out_info->name[sizeof(out_info->name) - 1] = '\0';
}

//-------------------------------------
int
mfb_get_num_monitors(void) {
    @autoreleasepool {
        return (int) [[NSScreen screens] count];
    }
}

//-------------------------------------
bool
mfb_get_monitor_info(unsigned index, mfb_monitor_info *out_info) {
    if (out_info == NULL)
        return false;

    @autoreleasepool {
        NSArray<NSScreen *> *screens = sorted_screens();
        if (index >= (unsigned) [screens count])
            return false;

        memset(out_info, 0, sizeof(*out_info));
        fill_info_from_screen(screens[index], index == 0, out_info);
        return true;
    }
}

//-------------------------------------
static mfb_monitor_info g_window_monitor_cache;

mfb_monitor_info *
mfb_get_window_monitor(struct mfb_window *window) {
    if (window == NULL)
        return NULL;

    @autoreleasepool {
        SWindowData     *window_data = (SWindowData *) window;
        SWindowData_OSX *osx         = (SWindowData_OSX *) window_data->specific;
        if (osx == NULL || osx->window == nil)
            return NULL;

        NSScreen *screen  = [osx->window screen];
        NSScreen *primary = [NSScreen mainScreen];
        if (screen == nil)
            screen = primary;

        memset(&g_window_monitor_cache, 0, sizeof(g_window_monitor_cache));
        fill_info_from_screen(screen, screen == primary, &g_window_monitor_cache);
        return &g_window_monitor_cache;
    }
}

//-------------------------------------
struct mfb_window *
mfb_open_on_monitor(const char *title, unsigned width, unsigned height,
                    unsigned monitor_index) {
    return mfb_open_on_monitor_ex(title, width, height, 0, monitor_index);
}

//-------------------------------------
struct mfb_window *
mfb_open_on_monitor_ex(const char *title, unsigned width, unsigned height,
                        unsigned flags, unsigned monitor_index) {
    if ((flags & MFB_WF_SIZE_LOGICAL) && (flags & MFB_WF_SIZE_PHYSICAL)) {
        fprintf(stderr, "mfb_open_on_monitor_ex: MFB_WF_SIZE_LOGICAL and MFB_WF_SIZE_PHYSICAL are mutually exclusive\n");
        return NULL;
    }

    @autoreleasepool {
        // Resolve target screen
        NSArray<NSScreen *> *screens = sorted_screens();
        NSScreen *target = (monitor_index < (unsigned) [screens count])
                           ? screens[monitor_index]
                           : [NSScreen mainScreen];

        // Adjust dimensions
        CGFloat scale    = [target backingScaleFactor];
        unsigned open_w  = width, open_h = height;
        if (flags & MFB_WF_SIZE_PHYSICAL) {
            // mfb_open_ex works in logical points; divide physical by scale
            if (scale > 0.0) {
                open_w = (unsigned) (width  / scale);
                open_h = (unsigned) (height / scale);
            }
        }
        // MFB_WF_SIZE_LOGICAL: mfb_open_ex already works in logical pts

        unsigned open_flags = flags & ~(unsigned)(MFB_WF_SIZE_LOGICAL | MFB_WF_SIZE_PHYSICAL);
        struct mfb_window *window = mfb_open_ex(title, open_w, open_h, open_flags);
        if (window == NULL)
            return NULL;

        // Reposition to center of target screen
        SWindowData     *window_data = (SWindowData *) window;
        SWindowData_OSX *osx         = (SWindowData_OSX *) window_data->specific;
        if (osx != NULL && osx->window != nil) {
            NSRect sf = [target visibleFrame];
            NSRect wf = [osx->window frame];
            NSPoint origin;
            origin.x = NSMinX(sf) + (NSWidth(sf)  - NSWidth(wf))  / 2.0;
            origin.y = NSMinY(sf) + (NSHeight(sf) - NSHeight(wf)) / 2.0;
            [osx->window setFrameOrigin:origin];
        }

        return window;
    }
}
