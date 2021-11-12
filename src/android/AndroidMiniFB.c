#include <android_native_app_glue.h>
#include <android/log.h>
#include <jni.h>
//--
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
//--
#include <MiniFB.h>
#include <WindowData.h>
#include "WindowData_Android.h"

#define  LOG_TAG    "MiniFB"
#define  LOGV(...)  __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__)
#define  LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG,   LOG_TAG, __VA_ARGS__)
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,    LOG_TAG, __VA_ARGS__)
#define  LOGW(...)  __android_log_print(ANDROID_LOG_WARN,    LOG_TAG, __VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,   LOG_TAG, __VA_ARGS__)
#define  LOGF(...)  __android_log_print(ANDROID_LOG_FATAL,   LOG_TAG, __VA_ARGS__)

#define kCall(func, ...)    if(window_data && window_data->func) window_data->func((struct mfb_window *) window_data, __VA_ARGS__);

#define kUnused(var)        (void) var;

struct android_app  *gApplication;

//-------------------------------------
extern void
stretch_image(uint32_t *srcImage, uint32_t srcX, uint32_t srcY, uint32_t srcWidth, uint32_t srcHeight, uint32_t srcPitch,
              uint32_t *dstImage, uint32_t dstX, uint32_t dstY, uint32_t dstWidth, uint32_t dstHeight, uint32_t dstPitch);

//-------------------------------------
extern int
main(int argc, char *argv[]);

//-------------------------------------
static void
draw(SWindowData *window_data, ANativeWindow_Buffer *window_buffer) {
    if(window_data == 0x0 || window_data->draw_buffer == 0x0 || window_buffer == 0x0)
        return;

    if((window_data->buffer_width == window_buffer->width) && (window_data->buffer_height == window_buffer->height)) {
        if(window_data->buffer_stride == window_buffer->stride*4) {
            memcpy(window_buffer->bits, window_data->draw_buffer, window_data->buffer_width * window_data->buffer_height * 4);
        }
        else {
            uint8_t  *src = window_data->draw_buffer;
            uint32_t *dst = window_buffer->bits;
            for(uint32_t y=0; y<window_data->window_height; ++y) {
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
    SWindowData *window_data = (SWindowData *) app->userData;
    if (window_data->close) {
        //destroy_window_data(window_data);
        return 0;
    }

    SWindowData_Android *window_data_android = (SWindowData_Android *) window_data->specific;

    int t = AInputEvent_getType(event);
    int s = AInputEvent_getSource(event);
    LOGV("Event: type= %d, source=%d", t, s);
    if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION) {
    //if (AInputEvent_getSource(event) == AINPUT_SOURCE_TOUCHSCREEN) {
        int action = AMotionEvent_getAction(event);
        int type   = action & AMOTION_EVENT_ACTION_MASK;
        switch(type) {
            case AMOTION_EVENT_ACTION_POINTER_DOWN:
            case AMOTION_EVENT_ACTION_POINTER_UP:
                {
                    int idx = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
                    int id  = AMotionEvent_getPointerId(event, idx);
                    int x   = AMotionEvent_getX(event, idx);
                    int y   = AMotionEvent_getY(event, idx);
                    window_data->mouse_pos_x = x | (id << 28);
                    window_data->mouse_pos_y = y | (id << 28);
                    window_data->mouse_button_status[id & 0x07] = (action == AMOTION_EVENT_ACTION_POINTER_DOWN);
                    kCall(mouse_btn_func, id, 0, action == AMOTION_EVENT_ACTION_POINTER_DOWN);
                }
                break;

            case AMOTION_EVENT_ACTION_DOWN:
            case AMOTION_EVENT_ACTION_UP:
                {
                    int count = AMotionEvent_getPointerCount(event);
                    for(int i=0; i < count; ++i) {
                        int id = AMotionEvent_getPointerId(event, i);
                        int x  = AMotionEvent_getX(event, i);
                        int y  = AMotionEvent_getY(event, i);
                        window_data->mouse_pos_x = x | (id << 28);
                        window_data->mouse_pos_y = y | (id << 28);
                        window_data->mouse_button_status[id & 0x07] = (action == AMOTION_EVENT_ACTION_POINTER_DOWN);
                        kCall(mouse_btn_func, id, 0, action == AMOTION_EVENT_ACTION_DOWN);
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
                        window_data->mouse_pos_x = x | (id << 28);
                        window_data->mouse_pos_y = y | (id << 28);
                        window_data->mouse_button_status[id & 0x07] = true;
                        kCall(mouse_move_func, window_data->mouse_pos_x, window_data->mouse_pos_y);
                    }
                }
                break;

            default:
                LOGV("Touch: event: action=%x, source=%x, type=%d", action, s, t);
                break;
        }

        if(window_data != 0x0) {
            window_data->is_active = true;
        }
        return 1;
    }
    else
    if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_KEY) {
        LOGV("Key event: action=%d keyCode=%d metaState=0x%x",
                AKeyEvent_getAction(event),
                AKeyEvent_getKeyCode(event),
                AKeyEvent_getMetaState(event));
    }

    return 0;
}

//-------------------------------------
static void
handle_cmd(struct android_app* app, int32_t cmd) {
    static int32_t  format = WINDOW_FORMAT_RGBX_8888;
    static int      sCurrentState = -1;

    sCurrentState = cmd;

    SWindowData         *window_data;
    SWindowData_Android *window_data_android;

    window_data = (SWindowData *) app->userData;
    if(window_data != 0x0) {
        window_data_android = (SWindowData_Android *) window_data->specific;
    }

    LOGV("cmd: %d", cmd);
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
                format = ANativeWindow_getFormat(app->window);
                ANativeWindow_setBuffersGeometry(app->window,
                                                 ANativeWindow_getWidth(app->window),
                                                 ANativeWindow_getHeight(app->window),
                                                 format
                );
                //engine_draw_frame(window_data_android);
            }
            break;

            // The current ANativeWindow has been resized. Please redraw with its new size.
        case APP_CMD_WINDOW_RESIZED:
            break;

            // The content area of the window has changed, such as from the soft input window being shown or hidden.
            // You can find the new content rect in android_app::contentRect.
        case APP_CMD_CONTENT_RECT_CHANGED:
            if(window_data_android != 0x0) {
                // This does not work
                //int32_t width  = window_data_android->app->contentRect.right  - window_data_android->app->contentRect.left;
                //int32_t height = window_data_android->app->contentRect.bottom - window_data_android->app->contentRect.top;
                // TODO: Check the DPI?
                if(window_data != 0x0) {
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
            if(window_data != 0x0) {
                window_data->is_active = true;
            }
            kCall(active_func, true);
            break;

            // The app's activity has been paused.
        case APP_CMD_PAUSE:
            break;

            // The app's activity window has lost input focus.
        case APP_CMD_LOST_FOCUS:
            if(window_data != 0x0) {
                window_data->is_active = true;
                //engine_draw_frame(window_data_android);
            }
            kCall(active_func, false);
            break;

            // The existing ANativeWindow needs to be terminated.
            // Upon receiving this command, android_app->window still contains the existing window;
            // after calling android_app_exec_cmd it will be set to NULL.
        case APP_CMD_TERM_WINDOW:
            if(window_data != 0x0) {
                window_data->is_active = false;
            }
            ANativeWindow_setBuffersGeometry(app->window,
                                             ANativeWindow_getWidth(app->window),
                                             ANativeWindow_getHeight(app->window),
                                             format
            );
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
            if(window_data != 0x0) {
                window_data->close = true;
            }
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
void
android_main(struct android_app* app) {
    app->onAppCmd     = handle_cmd;
    app->onInputEvent = handle_input;
    gApplication = app;

    // Read all pending events.
    int ident;
    int events;
    struct android_poll_source* source;
    while(app->window == 0x0) {
        while ((ident = ALooper_pollAll(0, NULL, &events, (void **) &source)) >= 0) {
            // Process this event.
            if (source != NULL) {
                source->process(app, source);
            }

            // Check if we are exiting.
            if (app->destroyRequested != 0) {
                LOGD("Engine thread destroy requested!");
                return;
            }
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

    SWindowData *window_data = malloc(sizeof(SWindowData));
    if (window_data == 0x0) {
        return 0x0;
    }
    memset(window_data, 0, sizeof(SWindowData));

    SWindowData_Android *window_data_android = malloc(sizeof(SWindowData_Android));
    if(window_data_android == 0x0) {
        free(window_data);
        return 0x0;
    }
    memset(window_data_android, 0, sizeof(SWindowData_Android));
    window_data->specific = window_data_android;

    window_data->is_active         = true;
    window_data_android->app       = gApplication;
    window_data_android->timer     = mfb_timer_create();

    window_data->buffer_width  = width;
    window_data->buffer_height = height;
    window_data->buffer_stride = width * 4;

    gApplication->userData = window_data;
    if(gApplication->window != 0x0) {
        window_data->window_width  = ANativeWindow_getWidth(gApplication->window);
        window_data->window_height = ANativeWindow_getHeight(gApplication->window);
    }

    window_data->is_initialized = true;
    return (struct mfb_window *) window_data;
}

//-------------------------------------
mfb_update_state
mfb_update_ex(struct mfb_window *window, void *buffer, unsigned width, unsigned height) {
    if (window == 0x0) {
        return STATE_INVALID_WINDOW;
    }

    SWindowData *window_data = (SWindowData *) window;
    if (window_data->close) {
        //destroy_window_data(window_data);
        return STATE_EXIT;
    }

    if (buffer == 0x0) {
        return STATE_INVALID_BUFFER;
    }

    window_data->draw_buffer   = buffer;
    window_data->buffer_width  = width;
    window_data->buffer_stride = width * 4;
    window_data->buffer_height = height;

    SWindowData_Android *window_data_android = (SWindowData_Android *) window_data->specific;

    ANativeWindow_Buffer native_buffer;
    if (ANativeWindow_lock(window_data_android->app->window, &native_buffer, NULL) < 0) {
        LOGE("Unable to lock window buffer");
        return STATE_INTERNAL_ERROR;
    }

    draw(window_data, &native_buffer);

    ANativeWindow_unlockAndPost(window_data_android->app->window);

    return STATE_OK;
}

//-------------------------------------
extern double   g_time_for_frame;

bool
mfb_wait_sync(struct mfb_window *window) {
    if (window == 0x0) {
        return false;
    }

    SWindowData *window_data = (SWindowData *) window;
    if (window_data->close) {
        //destroy_window_data(window_data);
        return false;
    }

    SWindowData_Android *window_data_android = (SWindowData_Android *) window_data->specific;

    // Read all pending events.
    int                         ident;
    int                         events;
    struct android_poll_source  *source;
    double                      current;

    while(1) {
        // If not animating, we will block forever waiting for events.
        // If animating, we loop until all events are read, then continue
        // to draw the next frame of animation.
        while ((ident = ALooper_pollAll(window_data->is_active ? 0 : -1, NULL, &events, (void **) &source)) >= 0) {
            // Process this event.
            if (source != NULL) {
                source->process(window_data_android->app, source);
            }

            // Check if we are exiting.
            if (window_data_android->app->destroyRequested != 0) {
                LOGD("Engine thread destroy requested!");
                window_data->is_active = false;
                window_data->close = true;
                return false;
            }
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

    if(scale_x != 0x0) {
        *scale_x = 1.0f;
    }
    if(scale_y != 0x0) {
        *scale_y = 1.0f;
    }
}
