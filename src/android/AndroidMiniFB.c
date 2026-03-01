#include <android_native_app_glue.h>
#include <android/log.h>
#include <jni.h>
//--
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
//--
#include <MiniFB.h>
#include <WindowData.h>
#include "WindowData_Android.h"

#define  LOG_TAG    "MiniFB"

struct android_app  *gApplication = NULL;

//-------------------------------------
static int
mfb_log_level_to_android_priority(mfb_log_level level) {
    switch (level) {
        case MFB_LOG_TRACE:   return ANDROID_LOG_VERBOSE;
        case MFB_LOG_DEBUG:   return ANDROID_LOG_DEBUG;
        case MFB_LOG_INFO:    return ANDROID_LOG_INFO;
        case MFB_LOG_WARNING: return ANDROID_LOG_WARN;
        case MFB_LOG_ERROR:   return ANDROID_LOG_ERROR;
        default:              return ANDROID_LOG_DEFAULT;
    }
}

//-------------------------------------
static void
android_mfb_log_sink(mfb_log_level level, const char *message) {
    if (message == NULL) {
        return;
    }

    __android_log_write(mfb_log_level_to_android_priority(level), LOG_TAG, message);
}

//-------------------------------------
extern void
stretch_image(uint32_t *src_image, uint32_t src_x, uint32_t src_y, uint32_t src_width, uint32_t src_height, uint32_t src_pitch,
              uint32_t *dst_image, uint32_t dst_x, uint32_t dst_y, uint32_t dst_width, uint32_t dst_height, uint32_t dst_pitch);

//-------------------------------------
extern int
main(int argc, char *argv[]);

//-------------------------------------
static const char *
app_cmd_name(int32_t cmd) {
    switch (cmd) {
        case APP_CMD_START:                return "APP_CMD_START";
        case APP_CMD_RESUME:               return "APP_CMD_RESUME";
        case APP_CMD_INPUT_CHANGED:        return "APP_CMD_INPUT_CHANGED";
        case APP_CMD_INIT_WINDOW:          return "APP_CMD_INIT_WINDOW";
        case APP_CMD_WINDOW_RESIZED:       return "APP_CMD_WINDOW_RESIZED";
        case APP_CMD_CONTENT_RECT_CHANGED: return "APP_CMD_CONTENT_RECT_CHANGED";
        case APP_CMD_WINDOW_REDRAW_NEEDED: return "APP_CMD_WINDOW_REDRAW_NEEDED";
        case APP_CMD_GAINED_FOCUS:         return "APP_CMD_GAINED_FOCUS";
        case APP_CMD_PAUSE:                return "APP_CMD_PAUSE";
        case APP_CMD_LOST_FOCUS:           return "APP_CMD_LOST_FOCUS";
        case APP_CMD_TERM_WINDOW:          return "APP_CMD_TERM_WINDOW";
        case APP_CMD_STOP:                 return "APP_CMD_STOP";
        case APP_CMD_SAVE_STATE:           return "APP_CMD_SAVE_STATE";
        case APP_CMD_DESTROY:              return "APP_CMD_DESTROY";
        case APP_CMD_LOW_MEMORY:           return "APP_CMD_LOW_MEMORY";
        case APP_CMD_CONFIG_CHANGED:       return "APP_CMD_CONFIG_CHANGED";
        default:                           return "APP_CMD_UNKNOWN";
    }
}

//-------------------------------------
static void
destroy_window_data(SWindowData *window_data) {
    if (window_data == NULL) {
        return;
    }

    SWindowData_Android *window_data_android = (SWindowData_Android *) window_data->specific;
    if (window_data_android != NULL) {
        if (window_data_android->timer != NULL) {
            mfb_timer_destroy(window_data_android->timer);
            window_data_android->timer = NULL;
        }
        free(window_data_android);
        window_data->specific = NULL;
    }

    if (gApplication != NULL && gApplication->userData == window_data) {
        gApplication->userData = NULL;
    }

    free(window_data);
}

//-------------------------------------
static void
draw(SWindowData *window_data, ANativeWindow_Buffer *window_buffer) {
    if (window_data == NULL || window_data->draw_buffer == NULL || window_buffer == NULL)
        return;

    uint32_t dst_x = window_data->dst_offset_x;
    uint32_t dst_y = window_data->dst_offset_y;
    uint32_t dst_w = window_data->dst_width;
    uint32_t dst_h = window_data->dst_height;

    // Clamp dst rect to actual native buffer dimensions. dst_* can be stale for one frame if
    // a rotation lifecycle event (CONTENT_RECT_CHANGED) hasn't been processed yet.
    uint32_t native_w = (uint32_t) window_buffer->width;
    uint32_t native_h = (uint32_t) window_buffer->height;
    if (dst_x >= native_w || dst_y >= native_h) return;
    if (dst_x + dst_w > native_w) dst_w = native_w - dst_x;
    if (dst_y + dst_h > native_h) dst_h = native_h - dst_y;

    // Fast path: 1:1 copy when the buffer exactly fills the native window with no viewport offset.
    if (dst_x == 0 && dst_y == 0 &&
        window_data->buffer_width  == native_w && dst_w == native_w &&
        window_data->buffer_height == native_h && dst_h == native_h) {
        if (window_data->buffer_stride == (uint32_t) window_buffer->stride * 4) {
            memcpy(window_buffer->bits, window_data->draw_buffer, window_data->buffer_width * window_data->buffer_height * 4);
        }
        else {
            uint8_t  *src = window_data->draw_buffer;
            uint32_t *dst = window_buffer->bits;
            for (uint32_t y = 0; y < window_data->buffer_height; ++y) {
                memcpy(dst, src, window_data->buffer_width * 4);
                src += window_data->buffer_stride;
                dst += window_buffer->stride;
            }
        }
    }
    else {
        uint32_t *src = window_data->draw_buffer;
        uint32_t *dst = window_buffer->bits;
        stretch_image(
            src, 0,     0,     window_data->buffer_width, window_data->buffer_height, window_data->buffer_width,
            dst, dst_x, dst_y, dst_w,                     dst_h,                     window_buffer->stride
        );
    }
}

//-------------------------------------
static int32_t
handle_input(struct android_app* app, AInputEvent* event) {
    if (app == NULL || event == NULL) {
        mfb_log(MFB_LOG_ERROR, "AndroidMiniFB: handle_input received null app or event.");
        return 0;
    }

    SWindowData *window_data = (SWindowData *) app->userData;
    if (window_data == NULL) {
        mfb_log(MFB_LOG_TRACE, "AndroidMiniFB: input event ignored because window data is not initialized.");
        return 0;
    }

    if (window_data->close) {
        mfb_log(MFB_LOG_DEBUG, "AndroidMiniFB: input event ignored because window is closing.");
        //destroy_window_data(window_data);
        return 0;
    }

    int type   = AInputEvent_getType(event);
    int source = AInputEvent_getSource(event);
    mfb_log(MFB_LOG_TRACE, "AndroidMiniFB: input event type=%d source=%d.", type, source);
    if (type == AINPUT_EVENT_TYPE_MOTION) {
    //if (source == AINPUT_SOURCE_TOUCHSCREEN) {
        int action      = AMotionEvent_getAction(event);
        int action_type = action & AMOTION_EVENT_ACTION_MASK;
        switch(action_type) {
            case AMOTION_EVENT_ACTION_POINTER_DOWN:
            case AMOTION_EVENT_ACTION_POINTER_UP:
                {
                    bool is_pressed = (action_type == AMOTION_EVENT_ACTION_POINTER_DOWN);
                    int idx = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
                    int id  = AMotionEvent_getPointerId(event, idx);
                    int x   = AMotionEvent_getX(event, idx);
                    int y   = AMotionEvent_getY(event, idx);
                    window_data->mouse_pos_x = x | (int) ((uint32_t) id << 28);
                    window_data->mouse_pos_y = y | (int) ((uint32_t) id << 28);
                    window_data->mouse_button_status[id & MFB_MAX_MOUSE_BUTTONS_MASK] = is_pressed;
                    kCall(mouse_btn_func, id, 0, is_pressed);
                }
                break;

            case AMOTION_EVENT_ACTION_DOWN:
            case AMOTION_EVENT_ACTION_UP:
                {
                    bool is_pressed = (action_type == AMOTION_EVENT_ACTION_DOWN);
                    int count = AMotionEvent_getPointerCount(event);
                    for(int i=0; i < count; ++i) {
                        int id = AMotionEvent_getPointerId(event, i);
                        int x  = AMotionEvent_getX(event, i);
                        int y  = AMotionEvent_getY(event, i);
                        window_data->mouse_pos_x = x | (int) ((uint32_t) id << 28);
                        window_data->mouse_pos_y = y | (int) ((uint32_t) id << 28);
                        window_data->mouse_button_status[id & MFB_MAX_MOUSE_BUTTONS_MASK] = is_pressed;
                        kCall(mouse_btn_func, id, 0, is_pressed);
                    }
                }
                break;

            case AMOTION_EVENT_ACTION_MOVE:
                {
                    int count = AMotionEvent_getPointerCount(event);
                    for(int i=0; i < count; ++i){
                        int id = AMotionEvent_getPointerId(event, i);
                        int x  = AMotionEvent_getX(event, i);
                        int y  = AMotionEvent_getY(event, i);
                        window_data->mouse_pos_x = x | (int) ((uint32_t) id << 28);
                        window_data->mouse_pos_y = y | (int) ((uint32_t) id << 28);
                        // MOVE events are only delivered while the pointer is down,
                        // so the pressed state is always true by definition.
                        window_data->mouse_button_status[id & MFB_MAX_MOUSE_BUTTONS_MASK] = true;
                        kCall(mouse_move_func, window_data->mouse_pos_x, window_data->mouse_pos_y);
                    }
                }
                break;

            case AMOTION_EVENT_ACTION_CANCEL:
                // Android cancelled the gesture (e.g., the system intercepted it).
                // Release all active pointers to prevent ghost touches.
                {
                    int count = AMotionEvent_getPointerCount(event);
                    for (int i = 0; i < count; ++i) {
                        int id = AMotionEvent_getPointerId(event, i);
                        window_data->mouse_button_status[id & MFB_MAX_MOUSE_BUTTONS_MASK] = false;
                        kCall(mouse_btn_func, id, 0, false);
                    }
                }
                break;

            case AMOTION_EVENT_ACTION_HOVER_MOVE:
                // External mouse or stylus hover (pointer not pressed).
                // Only available when a Bluetooth/USB-OTG mouse or stylus is connected.
                // Not a common use case on mobile; difficult to test without physical hardware.
                {
                    int x = (int) AMotionEvent_getX(event, 0);
                    int y = (int) AMotionEvent_getY(event, 0);
                    window_data->mouse_pos_x = x;
                    window_data->mouse_pos_y = y;
                    kCall(mouse_move_func, x, y);
                }
                break;

            case AMOTION_EVENT_ACTION_SCROLL:
                // Mouse scroll wheel from an external mouse/trackpad (Bluetooth or USB-OTG).
                // AXIS_VSCROLL = vertical scroll, AXIS_HSCROLL = horizontal scroll.
                // Not a common use case on mobile; difficult to test without physical hardware.
                {
                    float h = AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_HSCROLL, 0);
                    float v = AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_VSCROLL, 0);
                    window_data->mouse_wheel_x = h;
                    window_data->mouse_wheel_y = v;
                    kCall(mouse_wheel_func, 0, h, v);
                }
                break;

            default:
                mfb_log(MFB_LOG_TRACE, "AndroidMiniFB: unhandled motion event action=%x source=%x type=%d.", action, source, type);
                break;
        }

        window_data->is_active = true;
        return 1;
    }
    else if (type == AINPUT_EVENT_TYPE_KEY) {
        int action   = AKeyEvent_getAction(event);
        int key_code = AKeyEvent_getKeyCode(event);

        mfb_log(MFB_LOG_TRACE, "AndroidMiniFB: key event action=%d keyCode=%d metaState=0x%x.",
                action, key_code, AKeyEvent_getMetaState(event));

#if defined(MINIFB_ANDROID_CAPTURE_RIGHT_CLICK_AS_ESC)
        // On emulator, right mouse button is often mapped to BACK.
        // Consume it to avoid Android finishing the activity, and expose it as ESC.
        if (key_code == AKEYCODE_BACK) {
            if (action == AKEY_EVENT_ACTION_DOWN || action == AKEY_EVENT_ACTION_UP) {
                bool is_pressed = (action == AKEY_EVENT_ACTION_DOWN);
                window_data->key_status[KB_KEY_ESCAPE] = is_pressed;
                kCall(keyboard_func, KB_KEY_ESCAPE, 0, is_pressed);
            }
            return 1;
        }
#endif
    }

    return 0;
}

//-------------------------------------
static void
handle_cmd(struct android_app* app, int32_t cmd) {
    static int32_t  format = WINDOW_FORMAT_RGBX_8888;

    if (app == NULL) {
        mfb_log(MFB_LOG_ERROR, "AndroidMiniFB: handle_cmd received null app.");
        return;
    }

    SWindowData         *window_data;
    SWindowData_Android *window_data_android = NULL;

    window_data = (SWindowData *) app->userData;
    if (window_data != NULL) {
        window_data_android = (SWindowData_Android *) window_data->specific;
    }

    mfb_log(MFB_LOG_TRACE, "AndroidMiniFB: lifecycle command %s (%d).", app_cmd_name(cmd), cmd);
    // Init: 10, 11, 0, 1, 3, 5, 4, 6
    //   START, RESUME, INPUT_CHANGED, INIT_WINDOW, WINDOW_RESIZED, CONTENT_RECT_CHANGED, WINDOW_REDRAW_NEEDED, GAINED_FOCUS
    // Pause: 13, 7, 2, 14, 12
    //   PAUSE, LOST_FOCUS, TERM_WINDOW, STOP, SAVE_STATE
    // Resume: 10, 11, 1, 3, 4, 6
    //   START, RESUME, INIT_WINDOW, WINDOW_RESIZED, WINDOW_REDRAW_NEEDED, GAINED_FOCUS
    // Close: 0, 15
    //   INPUT_CHANGED, DESTROY
    // Lower the shutter: 7, 0
    //   LOST_FOCUS, INPUT_CHANGED
    // Raising the shutter: 6, 1
    //   GAINED_FOCUS, INIT_WINDOW
    // Rotate: 13, 2, 14, 12, 0, 15, 10, 11, 0, 1, 3, 5, 4, 6, 4
    //   PAUSE, TERM_WINDOW, STOP, SAVE_STATE, (similar to Pause but LOST_FOCUS)
    //   INPUT_CHANGED, DESTROY, (like Close)
    //   START, RESUME, INPUT_CHANGED, INIT_WINDOW, WINDOW_RESIZED, CONTENT_RECT_CHANGED, WINDOW_REDRAW_NEEDED, GAINED_FOCUS (like Init)
    switch (cmd) {
        // The app's activity has been started.
        case APP_CMD_START:
            break;

        // The app's activity has been resumed.
        case APP_CMD_RESUME:
            break;

        // The AInputQueue has changed.
        // Upon processing this command, android_app->inputQueue will be updated to the new queue (or NULL).
        case APP_CMD_INPUT_CHANGED:
            break;

        // A new ANativeWindow is ready for use.
        // Upon receiving this command, android_app->window will contain the new window surface.
        case APP_CMD_INIT_WINDOW:
            if (app->window != NULL) {
                //format = ANativeWindow_getFormat(app->window);
                int lock_result = ANativeWindow_setBuffersGeometry(app->window,
                                                                    ANativeWindow_getWidth(app->window),
                                                                    ANativeWindow_getHeight(app->window),
                                                                    format
                );
                if (lock_result < 0) {
                    mfb_log(MFB_LOG_ERROR, "AndroidMiniFB: ANativeWindow_setBuffersGeometry failed on init (%d).", lock_result);
                }
                //engine_draw_frame(window_data_android);
            }
            else {
                mfb_log(MFB_LOG_WARNING, "AndroidMiniFB: APP_CMD_INIT_WINDOW received with a null ANativeWindow.");
            }
            break;

        // The current ANativeWindow has been resized. Please redraw with its new size.
        case APP_CMD_WINDOW_RESIZED:
            break;

        // The content area of the window has changed, such as from the soft input window being shown or hidden.
        // You can find the new content rect in android_app::contentRect.
        case APP_CMD_CONTENT_RECT_CHANGED:
            if (window_data_android != NULL) {
                if (app->window == NULL) {
                    mfb_log(MFB_LOG_WARNING, "AndroidMiniFB: content rect changed but ANativeWindow is null.");
                    break;
                }
                // This does not work
                //int32_t width  = window_data_android->app->contentRect.right  - window_data_android->app->contentRect.left;
                //int32_t height = window_data_android->app->contentRect.bottom - window_data_android->app->contentRect.top;
                // TODO: Check the DPI?
                if (window_data != NULL) {
                    window_data->window_width  = ANativeWindow_getWidth(app->window);
                    window_data->window_height = ANativeWindow_getHeight(app->window);
                    // Do NOT call resize_dst here. The viewport was already aligned to the native
                    // window in mfb_open_ex. Calling resize_dst again at this point reads
                    // ANativeWindow_getWidth/Height AFTER the surface has fully settled, which can
                    // return slightly different dimensions than the values mfb_open_ex read (nav bar
                    // animation, window inset changes) and corrupts the viewport causing a visible
                    // horizontal or vertical black bar whose side depends on the rotation direction.
                    // If the user wants to adjust the viewport for the new window size they can
                    // call mfb_set_viewport or mfb_set_viewport_best_fit inside their resize callback.
                    kCall(resize_func, window_data->window_width, window_data->window_height);
                }
            }
            break;

        // The system needs that the current ANativeWindow be redrawn.
        // You should redraw the window before handing this to android_app_exec_cmd() in order to avoid transient drawing glitches.
        case APP_CMD_WINDOW_REDRAW_NEEDED:
            break;

        // The app's activity window has gained input focus.
        case APP_CMD_GAINED_FOCUS:
            if (window_data != NULL) {
                window_data->is_active = true;
            }
            kCall(active_func, true);
            break;

        // The app's activity has been paused.
        case APP_CMD_PAUSE:
            break;

        // The app's activity window has lost input focus.
        case APP_CMD_LOST_FOCUS:
            if (window_data != NULL) {
                window_data->is_active = false;
                //engine_draw_frame(window_data_android);
            }
            kCall(active_func, false);
            break;

        // The existing ANativeWindow needs to be terminated.
        // Upon receiving this command, android_app->window still contains the existing window;
        // after calling android_app_exec_cmd it will be set to NULL.
        case APP_CMD_TERM_WINDOW:
            if (window_data != NULL) {
                window_data->is_active = false;
            }
            // Testing: setBuffersGeometry on TERM_WINDOW appears unnecessary (window is being destroyed).
            //if (app->window != NULL) {
            //    int lock_result = ANativeWindow_setBuffersGeometry(app->window,
            //                                                        ANativeWindow_getWidth(app->window),
            //                                                        ANativeWindow_getHeight(app->window),
            //                                                        format
            //    );
            //    if (lock_result < 0) {
            //        mfb_log(MFB_LOG_ERROR, "AndroidMiniFB: ANativeWindow_setBuffersGeometry failed on term (%d).", lock_result);
            //    }
            //}
            //else {
            //    mfb_log(MFB_LOG_WARNING, "AndroidMiniFB: APP_CMD_TERM_WINDOW received with a null ANativeWindow.");
            //}
            break;

        // The app's activity has been stopped.
        case APP_CMD_STOP:
            break;

        // The app should generate a new saved state for itself, to restore from later if needed.
        // If you have saved state, allocate it with malloc and place it in android_app.savedState with
        // the size in android_app.savedStateSize.
        // The will be freed for you later.
        case APP_CMD_SAVE_STATE:
            break;

        // The app's activity is being destroyed, and waiting for the app thread to clean up and exit before proceeding.
        case APP_CMD_DESTROY:
            if (window_data != NULL) {
                window_data->close = true;
            }
            mfb_log(MFB_LOG_DEBUG, "AndroidMiniFB: received APP_CMD_DESTROY.");
            break;

        // The system is running low on memory. Try to reduce your memory use.
        case APP_CMD_LOW_MEMORY:
            break;

        // The current device configuration has changed.
        case APP_CMD_CONFIG_CHANGED:
            break;
    }
}

//-------------------------------------
static mfb_update_state
process_events(SWindowData *window_data, int timeout_ms) {
    if (window_data == NULL) {
        mfb_log(MFB_LOG_DEBUG, "process_events: invalid window");
        return STATE_INVALID_WINDOW;
    }

    if (window_data->close) {
        mfb_log(MFB_LOG_DEBUG, "process_events: window requested close");
        return STATE_EXIT;
    }

    SWindowData_Android *window_data_android = (SWindowData_Android *) window_data->specific;
    if (window_data_android == NULL || window_data_android->app == NULL) {
        mfb_log(MFB_LOG_ERROR, "AndroidMiniFB: process_events missing Android window state.");
        return STATE_INVALID_WINDOW;
    }

    int ident;
    int events;
    struct android_poll_source *source = NULL;

    // First poll may block (timeout_ms), subsequent polls are non-blocking to drain the queue.
    int poll_timeout_ms = timeout_ms;
    while ((ident = ALooper_pollOnce(poll_timeout_ms, NULL, &events, (void **) &source)) >= 0) {
        if (source != NULL) {
            source->process(window_data_android->app, source);
        }

        if (window_data_android->app->destroyRequested != 0) {
            mfb_log(MFB_LOG_DEBUG, "AndroidMiniFB: engine thread destroy requested.");
            window_data->is_active = false;
            window_data->close = true;
            return STATE_EXIT;
        }

        poll_timeout_ms = 0;
    }

    if (ident == ALOOPER_POLL_ERROR) {
        mfb_log(MFB_LOG_ERROR, "AndroidMiniFB: ALooper_pollOnce returned ALOOPER_POLL_ERROR.");
        return STATE_INTERNAL_ERROR;
    }

    return window_data->close ? STATE_EXIT : STATE_OK;
}

//-------------------------------------
static int
timeout_ms_from_remaining_seconds(double remaining_seconds) {
    if (remaining_seconds <= 0.0) {
        return 0;
    }

    double timeout_ms = remaining_seconds * 1000.0;
    if (timeout_ms >= (double) INT_MAX) {
        return INT_MAX;
    }

    int timeout = (int) timeout_ms;
    return timeout > 0 ? timeout : 1;
}

//-------------------------------------
void
android_main(struct android_app* app) {
    if (app == NULL) {
        mfb_log(MFB_LOG_ERROR, "AndroidMiniFB: android_main received null app.");
        return;
    }

    app->onAppCmd     = handle_cmd;
    app->onInputEvent = handle_input;
    gApplication = app;

    mfb_set_logger(android_mfb_log_sink);
#if !defined(NDEBUG) || defined(DEBUG) || defined(_DEBUG)
    mfb_set_log_level(MFB_LOG_TRACE);
#else
    mfb_set_log_level(MFB_LOG_INFO);
#endif

    // Wait for the first window without busy-polling.
    int ident;
    int events;
    struct android_poll_source *source = NULL;
    while (app->window == NULL) {
        ident = ALooper_pollOnce(-1, NULL, &events, (void **) &source);

        if (ident >= 0 && source != NULL) {
            source->process(app, source);
        }
        else if (ident == ALOOPER_POLL_ERROR) {
            mfb_log(MFB_LOG_ERROR, "AndroidMiniFB: ALooper_pollOnce failed while waiting for initial window.");
        }

        if (app->destroyRequested != 0) {
            mfb_log(MFB_LOG_DEBUG, "AndroidMiniFB: destroy requested while waiting for initial window.");
            return;
        }
    }

    char cwd[1024];
    getcwd(cwd, sizeof(cwd));
    char *argv[] = {
            cwd,
            (char *) app
    };
    main(2, argv);
}

//-------------------------------------
struct mfb_window *
mfb_open_ex(const char *title, unsigned width, unsigned height, unsigned flags) {
    kUnused(title);
    kUnused(flags);

    if (gApplication == NULL) {
        mfb_log(MFB_LOG_ERROR, "AndroidMiniFB: mfb_open_ex called before android_main initialized the app context.");
        return NULL;
    }

    SWindowData *window_data = malloc(sizeof(SWindowData));
    if (window_data == NULL) {
        mfb_log(MFB_LOG_ERROR, "AndroidMiniFB: failed to allocate SWindowData.");
        return NULL;
    }
    memset(window_data, 0, sizeof(SWindowData));

    SWindowData_Android *window_data_android = malloc(sizeof(SWindowData_Android));
    if (window_data_android == NULL) {
        mfb_log(MFB_LOG_ERROR, "AndroidMiniFB: failed to allocate SWindowData_Android.");
        free(window_data);
        return NULL;
    }
    memset(window_data_android, 0, sizeof(SWindowData_Android));
    window_data->specific = window_data_android;

    window_data->is_active         = true;
    window_data->is_cursor_visible = false;

    window_data_android->app       = gApplication;
    window_data_android->timer     = mfb_timer_create();
    if (window_data_android->timer == NULL) {
        mfb_log(MFB_LOG_ERROR, "AndroidMiniFB: mfb_timer_create failed.");
        free(window_data_android);
        free(window_data);
        return NULL;
    }

    window_data->buffer_width  = width;
    window_data->buffer_height = height;
    window_data->buffer_stride = width * 4;

    gApplication->userData = window_data;
    if (gApplication->window != NULL) {
        window_data->window_width  = ANativeWindow_getWidth(gApplication->window);
        window_data->window_height = ANativeWindow_getHeight(gApplication->window);
    }
    else {
        mfb_log(MFB_LOG_WARNING, "AndroidMiniFB: mfb_open_ex created window data without an active ANativeWindow.");
    }

    // Initialize dst_* fields to match drawable area (same as buffer dimensions)
    calc_dst_factor(window_data, width, height);
    // If the native window is already available, immediately scale dst to the real window
    // dimensions. Without this, the first draw() call after a rotation would use dst_h equal
    // to the buffer height (portrait), but the post-rotation native buffer is landscape-sized,
    // causing stretch_image to write past the end of the buffer and crash.
    if (window_data->window_width != 0 && window_data->window_height != 0) {
        resize_dst(window_data, window_data->window_width, window_data->window_height);
    }

#if defined(_DEBUG)
    mfb_log(MFB_LOG_DEBUG, "Window created using Android API");
#endif

    window_data->is_initialized = true;
    return (struct mfb_window *) window_data;
}

//-------------------------------------
mfb_update_state
mfb_update_ex(struct mfb_window *window, void *buffer, unsigned width, unsigned height) {
    if (window == NULL) {
        mfb_log(MFB_LOG_DEBUG, "mfb_update_ex: invalid window");
        return STATE_INVALID_WINDOW;
    }

    SWindowData *window_data = (SWindowData *) window;
    if (window_data->close) {
        mfb_log(MFB_LOG_DEBUG, "mfb_update_ex: window requested close");
        destroy_window_data(window_data);
        return STATE_EXIT;
    }

    if (buffer == NULL) {
        mfb_log(MFB_LOG_DEBUG, "mfb_update_ex: invalid buffer");
        return STATE_INVALID_BUFFER;
    }

    if (width == 0 || height == 0) {
        mfb_log(MFB_LOG_DEBUG, "mfb_update_ex: invalid buffer size %ux%u", width, height);
        return STATE_INVALID_BUFFER;
    }

    window_data->draw_buffer   = buffer;
    window_data->buffer_width  = width;
    window_data->buffer_stride = width * 4;
    window_data->buffer_height = height;

    SWindowData_Android *window_data_android = (SWindowData_Android *) window_data->specific;
    if (window_data_android == NULL || window_data_android->app == NULL) {
        mfb_log(MFB_LOG_ERROR, "AndroidMiniFB: missing Android window state in mfb_update_ex.");
        return STATE_INVALID_WINDOW;
    }

    // During Android lifecycle transitions (pause/term window), the native window
    // can be temporarily NULL. This is not a terminal window error: skip rendering
    // and let mfb_wait_sync block until the app becomes drawable again.
    if (window_data_android->app->window == NULL) {
        mfb_log(MFB_LOG_TRACE, "AndroidMiniFB: skipping frame because ANativeWindow is temporarily unavailable.");
        return STATE_OK;
    }

    ANativeWindow_Buffer native_buffer;
    int lock_result = ANativeWindow_lock(window_data_android->app->window, &native_buffer, NULL);
    if (lock_result < 0) {
        mfb_log(MFB_LOG_ERROR, "AndroidMiniFB: ANativeWindow_lock failed (%d).", lock_result);
        return STATE_INTERNAL_ERROR;
    }

    draw(window_data, &native_buffer);

    int post_result = ANativeWindow_unlockAndPost(window_data_android->app->window);
    if (post_result < 0) {
        mfb_log(MFB_LOG_ERROR, "AndroidMiniFB: ANativeWindow_unlockAndPost failed (%d).", post_result);
        return STATE_INTERNAL_ERROR;
    }

    mfb_update_state event_state = process_events(window_data, 0);
    if (event_state == STATE_EXIT) {
        mfb_log(MFB_LOG_DEBUG, "mfb_update_ex: window closed after event processing");
        destroy_window_data(window_data);
        return STATE_EXIT;
    }

    if (event_state == STATE_INVALID_WINDOW) {
        mfb_log(MFB_LOG_ERROR, "AndroidMiniFB: mfb_update_ex detected invalid Android window state.");
        return STATE_INVALID_WINDOW;
    }

    if (event_state == STATE_INTERNAL_ERROR) {
        mfb_log(MFB_LOG_ERROR, "AndroidMiniFB: mfb_update_ex detected looper error.");
        return STATE_INTERNAL_ERROR;
    }

    return STATE_OK;
}

//-------------------------------------
mfb_update_state
mfb_update_events(struct mfb_window *window) {
    if (window == NULL) {
        mfb_log(MFB_LOG_DEBUG, "mfb_update_events: invalid window");
        return STATE_INVALID_WINDOW;
    }

    SWindowData *window_data = (SWindowData *) window;
    if (window_data->close) {
        mfb_log(MFB_LOG_DEBUG, "mfb_update_events: window requested close");
        destroy_window_data(window_data);
        return STATE_EXIT;
    }

    mfb_update_state event_state = process_events(window_data, 0);
    if (event_state == STATE_EXIT) {
        mfb_log(MFB_LOG_DEBUG, "mfb_update_events: window closed after event processing");
        destroy_window_data(window_data);
    }
    else if (event_state == STATE_INVALID_WINDOW) {
        mfb_log(MFB_LOG_ERROR, "AndroidMiniFB: mfb_update_events detected invalid Android window state.");
    }

    return event_state;
}

//-------------------------------------
extern double   g_time_for_frame;

//-------------------------------------
bool
mfb_wait_sync(struct mfb_window *window) {
    if (window == NULL) {
        mfb_log(MFB_LOG_DEBUG, "mfb_wait_sync: invalid window");
        return false;
    }

    SWindowData *window_data = (SWindowData *) window;
    if (window_data->close) {
        mfb_log(MFB_LOG_DEBUG, "mfb_wait_sync: window requested close");
        destroy_window_data(window_data);
        return false;
    }

    SWindowData_Android *window_data_android = (SWindowData_Android *) window_data->specific;
    if (window_data_android == NULL) {
        mfb_log(MFB_LOG_DEBUG, "mfb_wait_sync: invalid window specific data");
        return false;
    }

    if (window_data_android->app == NULL) {
        mfb_log(MFB_LOG_ERROR, "AndroidMiniFB: mfb_wait_sync has a null android_app handle.");
        return false;
    }

    if (window_data_android->timer == NULL) {
        mfb_log(MFB_LOG_ERROR, "AndroidMiniFB: mfb_wait_sync missing frame timer state.");
        return false;
    }

    double current;

    while(1) {
        int timeout_ms;
        if (window_data->is_active && window_data_android->app->window != NULL) {
            current = mfb_timer_now(window_data_android->timer);
            timeout_ms = timeout_ms_from_remaining_seconds(g_time_for_frame - current);
        }
        else {
            timeout_ms = -1;
        }

        mfb_update_state event_state = process_events(window_data, timeout_ms);
        if (event_state == STATE_EXIT) {
            mfb_log(MFB_LOG_DEBUG, "mfb_wait_sync: window closed while waiting for frame sync");
            destroy_window_data(window_data);
            return false;
        }
        if (event_state == STATE_INVALID_WINDOW) {
            mfb_log(MFB_LOG_ERROR, "AndroidMiniFB: mfb_wait_sync aborted due to invalid Android window state.");
            return false;
        }

        if (event_state == STATE_INTERNAL_ERROR) {
            mfb_log(MFB_LOG_ERROR, "AndroidMiniFB: mfb_wait_sync aborted due to looper error.");
            return false;
        }

        // While paused or without a surface, keep waiting for lifecycle events and
        // do not advance frame timing.
        if (!window_data->is_active || window_data_android->app->window == NULL) {
            continue;
        }

        current = mfb_timer_now(window_data_android->timer);
        if (current >= g_time_for_frame) {
            break;
        }
    }
    mfb_timer_reset(window_data_android->timer);

    return true;
}

//-------------------------------------
void
mfb_get_monitor_scale(struct mfb_window *window, float *scale_x, float *scale_y) {
    kUnused(window);

    if (scale_x != NULL) {
        *scale_x = 1.0f;
    }
    if (scale_y != NULL) {
        *scale_y = 1.0f;
    }
}

//-------------------------------------
bool
mfb_set_viewport(struct mfb_window *window, unsigned offset_x, unsigned offset_y, unsigned width, unsigned height) {
    if (window == NULL) {
        mfb_log(MFB_LOG_ERROR, "AndroidMiniFB: mfb_set_viewport called with a null window pointer.");
        return false;
    }

    SWindowData *window_data = (SWindowData *) window;

    if (offset_x + width > window_data->window_width) {
        mfb_log(MFB_LOG_ERROR, "AndroidMiniFB: viewport exceeds window width (offset_x=%u, width=%u, window_width=%u).",
                offset_x, width, window_data->window_width);
        return false;
    }
    if (offset_y + height > window_data->window_height) {
        mfb_log(MFB_LOG_ERROR, "AndroidMiniFB: viewport exceeds window height (offset_y=%u, height=%u, window_height=%u).",
                offset_y, height, window_data->window_height);
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
mfb_show_cursor(struct mfb_window *window, bool show) {
    kUnused(window);
    kUnused(show);
    // Cursors are not applicable on Android.
}

//-------------------------------------
// JNI helper: attach to the JVM on the calling thread if needed.
// Returns JNI_OK on success.  Sets *out_attached = true when AttachCurrentThread was
// called so the caller knows it must call DetachCurrentThread before returning.
static int
jni_get_env(JavaVM *vm, JNIEnv **out_env, bool *out_attached) {
    *out_attached = false;
    int status = (*vm)->GetEnv(vm, (void **) out_env, JNI_VERSION_1_6);
    if (status == JNI_EDETACHED) {
        if ((*vm)->AttachCurrentThread(vm, out_env, NULL) != JNI_OK) {
            return JNI_ERR;
        }
        *out_attached = true;
        return JNI_OK;
    }
    return status; // JNI_OK or JNI_EVERSION
}

//-------------------------------------
// JNI helper: clear any pending Java exception so subsequent JNI calls are safe.
static void
jni_clear_exception(JNIEnv *env) {
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
    }
}

//-------------------------------------
// JNI helper: retrieve the WindowInsets object from the Activity's decor view.
// Returns NULL on failure (missing method, exception, or API too old).
static jobject
jni_get_root_window_insets(JNIEnv *env, jobject activity_obj) {
    // activity.getWindow()
    jclass  activity_class = (*env)->GetObjectClass(env, activity_obj);
    if (!activity_class) return NULL;
    jmethodID get_window = (*env)->GetMethodID(env, activity_class, "getWindow", "()Landroid/view/Window;");
    if (!get_window) { jni_clear_exception(env); return NULL; }
    jobject window_obj = (*env)->CallObjectMethod(env, activity_obj, get_window);
    if (!window_obj || (*env)->ExceptionCheck(env)) { jni_clear_exception(env); return NULL; }

    // window.getDecorView()
    jclass  window_class = (*env)->GetObjectClass(env, window_obj);
    if (!window_class) return NULL;
    jmethodID get_decor = (*env)->GetMethodID(env, window_class, "getDecorView", "()Landroid/view/View;");
    if (!get_decor) { jni_clear_exception(env); return NULL; }
    jobject decor_view = (*env)->CallObjectMethod(env, window_obj, get_decor);
    if (!decor_view || (*env)->ExceptionCheck(env)) { jni_clear_exception(env); return NULL; }

    // decorView.getRootWindowInsets()
    jclass  view_class = (*env)->GetObjectClass(env, decor_view);
    if (!view_class) return NULL;
    jmethodID get_insets = (*env)->GetMethodID(env, view_class, "getRootWindowInsets", "()Landroid/view/WindowInsets;");
    if (!get_insets) { jni_clear_exception(env); return NULL; }
    jobject insets = (*env)->CallObjectMethod(env, decor_view, get_insets);
    if ((*env)->ExceptionCheck(env)) { jni_clear_exception(env); return NULL; }
    return insets; // may be NULL (view not yet attached)
}

//-------------------------------------
// Common setup: validate window, attach JVM, get WindowInsets.
// Returns true on success and fills *out_env / *out_insets / *out_attached.
static bool
insets_setup(struct mfb_window *window, JNIEnv **out_env, jobject *out_insets, bool *out_attached) {
    *out_env      = NULL;
    *out_insets   = NULL;
    *out_attached = false;

    if (!window) return false;

    SWindowData         *wd  = (SWindowData *) window;
    SWindowData_Android *wda = (SWindowData_Android *) wd->specific;
    if (!wda || !wda->app || !wda->app->activity) return false;

    ANativeActivity *activity = wda->app->activity;
    if (jni_get_env(activity->vm, out_env, out_attached) != JNI_OK) return false;

    *out_insets = jni_get_root_window_insets(*out_env, activity->clazz);
    if (!*out_insets) return false;
    return true;
}

//-------------------------------------
bool
mfb_get_display_cutout_insets(struct mfb_window *window, int *left, int *top, int *right, int *bottom) {
    if (left)   *left   = 0;
    if (top)    *top    = 0;
    if (right)  *right  = 0;
    if (bottom) *bottom = 0;

    JNIEnv  *env      = NULL;
    jobject  insets   = NULL;
    bool     attached = false;

    if (!insets_setup(window, &env, &insets, &attached)) goto done;

    {
        // windowInsets.getDisplayCutout() — API 28+
        jclass    insets_class = (*env)->GetObjectClass(env, insets);
        jmethodID get_cutout   = (*env)->GetMethodID(env, insets_class, "getDisplayCutout", "()Landroid/view/DisplayCutout;");
        if (!get_cutout) {
            jni_clear_exception(env);
            goto done;
        }

        jobject display_cutout = (*env)->CallObjectMethod(env, insets, get_cutout);
        if ((*env)->ExceptionCheck(env)) {
            jni_clear_exception(env);
            goto done;
        }

        if (!display_cutout) {
            // Device has no cutout — return true with zeros (valid answer).
            if (attached) {
                SWindowData         *wd  = (SWindowData *) window;
                SWindowData_Android *wda = (SWindowData_Android *) wd->specific;
                (*wda->app->activity->vm)->DetachCurrentThread(wda->app->activity->vm);
            }
            return true;
        }

        jclass cutout_class = (*env)->GetObjectClass(env, display_cutout);

#define READ_INSET(field, method_name)                                                      \
        if (field) {                                                                        \
            jmethodID m = (*env)->GetMethodID(env, cutout_class, method_name, "()I");       \
            if (m) { *field = (int) (*env)->CallIntMethod(env, display_cutout, m); }        \
            jni_clear_exception(env);                                                       \
        }

        READ_INSET(left,   "getSafeInsetLeft")
        READ_INSET(top,    "getSafeInsetTop")
        READ_INSET(right,  "getSafeInsetRight")
        READ_INSET(bottom, "getSafeInsetBottom")

#undef READ_INSET
    }

    if (attached) {
        SWindowData         *wd  = (SWindowData *) window;
        SWindowData_Android *wda = (SWindowData_Android *) wd->specific;
        (*wda->app->activity->vm)->DetachCurrentThread(wda->app->activity->vm);
    }

    return true;

done:
    if (attached && window) {
        SWindowData         *wd  = (SWindowData *) window;
        SWindowData_Android *wda = (SWindowData_Android *) wd->specific;
        if (wda && wda->app && wda->app->activity) {
            (*wda->app->activity->vm)->DetachCurrentThread(wda->app->activity->vm);
        }
    }

    return false;
}

//-------------------------------------
bool
mfb_get_display_safe_insets(struct mfb_window *window, int *left, int *top, int *right, int *bottom) {
    if (left)   *left   = 0;
    if (top)    *top    = 0;
    if (right)  *right  = 0;
    if (bottom) *bottom = 0;

    JNIEnv  *env      = NULL;
    jobject  insets   = NULL;
    bool     attached = false;

    if (!insets_setup(window, &env, &insets, &attached))
        goto done;

    {
        jclass    insets_class = (*env)->GetObjectClass(env, insets);
        bool      got_values   = false;

        // ----------------------------------------------------------------
        // API 30+: WindowInsets.getInsets(type) returns android.graphics.Insets.
        // Type constants (android.view.WindowInsets.Type, API 30+):
        //   statusBars()    = 1  (0x01)
        //   navigationBars()= 2  (0x02)
        //   captionBar()    = 4  (0x04)   → systemBars() = 1|2|4 = 7
        //   displayCutout() = 128 (0x80)
        //   combined        = 7 | 128     = 135 (0x87)
        // ----------------------------------------------------------------
        jmethodID get_insets_m = (*env)->GetMethodID(env, insets_class, "getInsets", "(I)Landroid/graphics/Insets;");
        if (get_insets_m) {
            jint combined_type = 0x87; // systemBars() | displayCutout()
            jobject insets_obj = (*env)->CallObjectMethod(env, insets, get_insets_m, combined_type);
            if (!(*env)->ExceptionCheck(env) && insets_obj) {
                jclass insets_obj_class = (*env)->GetObjectClass(env, insets_obj);
                if (left)   { jfieldID f = (*env)->GetFieldID(env, insets_obj_class, "left",   "I"); if (f) *left   = (int) (*env)->GetIntField(env, insets_obj, f); }
                if (top)    { jfieldID f = (*env)->GetFieldID(env, insets_obj_class, "top",    "I"); if (f) *top    = (int) (*env)->GetIntField(env, insets_obj, f); }
                if (right)  { jfieldID f = (*env)->GetFieldID(env, insets_obj_class, "right",  "I"); if (f) *right  = (int) (*env)->GetIntField(env, insets_obj, f); }
                if (bottom) { jfieldID f = (*env)->GetFieldID(env, insets_obj_class, "bottom", "I"); if (f) *bottom = (int) (*env)->GetIntField(env, insets_obj, f); }
                got_values = true;
            }
            jni_clear_exception(env);
        }
        else {
            jni_clear_exception(env);
        }

        // ----------------------------------------------------------------
        // API 24-29 fallback: individual getSystemWindowInset{Top,Right,Bottom,Left}()
        // methods each return int directly and are available from API 20.
        // (getSystemWindowInsets() returning android.graphics.Insets is API 30+ only.)
        // ----------------------------------------------------------------
        if (!got_values) {
            jmethodID get_left   = (*env)->GetMethodID(env, insets_class, "getSystemWindowInsetLeft",   "()I");
            jmethodID get_top    = (*env)->GetMethodID(env, insets_class, "getSystemWindowInsetTop",    "()I");
            jmethodID get_right  = (*env)->GetMethodID(env, insets_class, "getSystemWindowInsetRight",  "()I");
            jmethodID get_bottom = (*env)->GetMethodID(env, insets_class, "getSystemWindowInsetBottom", "()I");
            jni_clear_exception(env);
            if (get_left && get_right && get_top && get_bottom) {
                if (left)   *left   = (int) (*env)->CallIntMethod(env, insets, get_left);
                if (top)    *top    = (int) (*env)->CallIntMethod(env, insets, get_top);
                if (right)  *right  = (int) (*env)->CallIntMethod(env, insets, get_right);
                if (bottom) *bottom = (int) (*env)->CallIntMethod(env, insets, get_bottom);
                jni_clear_exception(env);
                got_values = true;
            }
        }

        if (!got_values)
            goto done;
    }

    if (attached) {
        SWindowData         *wd  = (SWindowData *) window;
        SWindowData_Android *wda = (SWindowData_Android *) wd->specific;
        (*wda->app->activity->vm)->DetachCurrentThread(wda->app->activity->vm);
    }

    return true;

done:
    if (attached && window) {
        SWindowData         *wd  = (SWindowData *) window;
        SWindowData_Android *wda = (SWindowData_Android *) wd->specific;
        if (wda && wda->app && wda->app->activity) {
            (*wda->app->activity->vm)->DetachCurrentThread(wda->app->activity->vm);
        }
    }

    return false;
}
