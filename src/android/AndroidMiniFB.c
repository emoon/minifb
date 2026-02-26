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
stretch_image(uint32_t *srcImage, uint32_t srcX, uint32_t srcY, uint32_t srcWidth, uint32_t srcHeight, uint32_t srcPitch,
              uint32_t *dstImage, uint32_t dstX, uint32_t dstY, uint32_t dstWidth, uint32_t dstHeight, uint32_t dstPitch);

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

    if ((window_data->buffer_width == window_buffer->width) && (window_data->buffer_height == window_buffer->height)) {
        if (window_data->buffer_stride == window_buffer->stride*4) {
            memcpy(window_buffer->bits, window_data->draw_buffer, window_data->buffer_width * window_data->buffer_height * 4);
        }
        else {
            uint8_t  *src = window_data->draw_buffer;
            uint32_t *dst = window_buffer->bits;
            for(uint32_t y=0; y<window_data->buffer_height; ++y) {
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
                src, 0, 0, window_data->buffer_width, window_data->buffer_height, window_data->buffer_width,
                dst, 0, 0, window_buffer->width,      window_buffer->height,      window_buffer->stride
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

    int t = AInputEvent_getType(event);
    int s = AInputEvent_getSource(event);
    mfb_log(MFB_LOG_TRACE, "AndroidMiniFB: input event type=%d source=%d.", t, s);
    if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION) {
    //if (AInputEvent_getSource(event) == AINPUT_SOURCE_TOUCHSCREEN) {
        int action = AMotionEvent_getAction(event);
        int type   = action & AMOTION_EVENT_ACTION_MASK;
        switch(type) {
            case AMOTION_EVENT_ACTION_POINTER_DOWN:
            case AMOTION_EVENT_ACTION_POINTER_UP:
                {
                    bool is_pressed = (type == AMOTION_EVENT_ACTION_POINTER_DOWN);
                    int idx = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
                    int id  = AMotionEvent_getPointerId(event, idx);
                    int x   = AMotionEvent_getX(event, idx);
                    int y   = AMotionEvent_getY(event, idx);
                    window_data->mouse_pos_x = x | (int) ((uint32_t) id << 28);
                    window_data->mouse_pos_y = y | (int) ((uint32_t) id << 28);
                    window_data->mouse_button_status[id & 0x07] = is_pressed;
                    kCall(mouse_btn_func, id, 0, is_pressed);
                }
                break;

            case AMOTION_EVENT_ACTION_DOWN:
            case AMOTION_EVENT_ACTION_UP:
                {
                    bool is_pressed = (type == AMOTION_EVENT_ACTION_DOWN);
                    int count = AMotionEvent_getPointerCount(event);
                    for(int i=0; i < count; ++i) {
                        int id = AMotionEvent_getPointerId(event, i);
                        int x  = AMotionEvent_getX(event, i);
                        int y  = AMotionEvent_getY(event, i);
                        window_data->mouse_pos_x = x | (int) ((uint32_t) id << 28);
                        window_data->mouse_pos_y = y | (int) ((uint32_t) id << 28);
                        window_data->mouse_button_status[id & 0x07] = is_pressed;
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
                        window_data->mouse_button_status[id & 0x07] = true;
                        kCall(mouse_move_func, window_data->mouse_pos_x, window_data->mouse_pos_y);
                    }
                }
                break;

            default:
                mfb_log(MFB_LOG_TRACE, "AndroidMiniFB: unhandled touch event action=%x source=%x type=%d.", action, s, t);
                break;
        }

        window_data->is_active = true;
        return 1;
    }
    else if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_KEY) {
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
            if (app->window != NULL) {
                int lock_result = ANativeWindow_setBuffersGeometry(app->window,
                                                                    ANativeWindow_getWidth(app->window),
                                                                    ANativeWindow_getHeight(app->window),
                                                                    format
                );
                if (lock_result < 0) {
                    mfb_log(MFB_LOG_ERROR, "AndroidMiniFB: ANativeWindow_setBuffersGeometry failed on term (%d).", lock_result);
                }
            }
            else {
                mfb_log(MFB_LOG_WARNING, "AndroidMiniFB: APP_CMD_TERM_WINDOW received with a null ANativeWindow.");
            }
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
