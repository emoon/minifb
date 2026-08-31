#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <MiniFB.h>
#include "WindowData.h"
#include "MiniFB_internal.h"
#include "MiniFB_keylist.h"

#if defined(__APPLE__)
    #include <TargetConditionals.h>
#endif

//-------------------------------------
short int g_keycodes[MFB_MAX_KEYS] = { 0 };
//-------------------------------------

//-------------------------------------
struct mfb_window *
mfb_open(const char *title, unsigned width, unsigned height) {
    return mfb_open_ex(title, width, height, 0);
}

//-------------------------------------
mfb_update_state
mfb_update(struct mfb_window *window, void *buffer) {
    if (window == NULL) {
        return MFB_STATE_INVALID_WINDOW;
    }

    SWindowData *window_data = (SWindowData *) window;

    return mfb_update_ex(window, buffer, window_data->buffer_width, window_data->buffer_height);
}

//-------------------------------------
void
mfb_set_active_callback(struct mfb_window *window, mfb_active_func callback) {
    if (window != NULL) {
        SWindowData *window_data = (SWindowData *) window;
        window_data->active_func = callback;
    }
}

//-------------------------------------
void
mfb_set_resize_callback(struct mfb_window *window, mfb_resize_func callback) {
    if (window != NULL) {
        SWindowData *window_data = (SWindowData *) window;
        window_data->resize_func = callback;
    }
}

//-------------------------------------
void
mfb_set_close_callback(struct mfb_window *window, mfb_close_func callback) {
    if (window != NULL) {
        SWindowData *window_data = (SWindowData *) window;
        window_data->close_func = callback;
    }
}

//-------------------------------------
void
mfb_set_keyboard_callback(struct mfb_window *window, mfb_keyboard_func callback) {
    if (window != NULL) {
        SWindowData *window_data = (SWindowData *) window;
        window_data->keyboard_func = callback;
    }
}

//-------------------------------------
void
mfb_set_char_input_callback(struct mfb_window *window, mfb_char_input_func callback) {
    if (window != NULL) {
        SWindowData *window_data = (SWindowData *) window;
        window_data->char_input_func = callback;
    }
}

//-------------------------------------
void
mfb_set_mouse_button_callback(struct mfb_window *window, mfb_mouse_button_func callback) {
    if (window != NULL) {
        SWindowData *window_data = (SWindowData *) window;
        window_data->mouse_btn_func = callback;
    }
}

//-------------------------------------
void
mfb_set_mouse_move_callback(struct mfb_window *window, mfb_mouse_move_func callback) {
    if (window != NULL) {
        SWindowData *window_data = (SWindowData *) window;
        window_data->mouse_move_func = callback;
    }
}

//-------------------------------------
void
mfb_set_mouse_scroll_callback(struct mfb_window *window, mfb_mouse_scroll_func callback) {
    if (window != NULL) {
        SWindowData *window_data = (SWindowData *) window;
        window_data->mouse_wheel_func = callback;
    }
}

//-------------------------------------
void
mfb_set_user_data(struct mfb_window *window, void *user_data) {
    if (window != NULL) {
        SWindowData *window_data = (SWindowData *) window;
        window_data->user_data = user_data;
    }
}

//-------------------------------------
void *
mfb_get_user_data(struct mfb_window *window) {
    if (window != NULL) {
        SWindowData *window_data = (SWindowData *) window;
        return window_data->user_data;
    }

    return NULL;
}

// [Deprecated]
//-------------------------------------
void
mfb_get_monitor_dpi(struct mfb_window *window, float *dpi_x, float *dpi_y) {
    mfb_get_monitor_scale(window, dpi_x, dpi_y);
}

//-------------------------------------
void
mfb_close(struct mfb_window *window) {
    if (window != NULL) {
        SWindowData *window_data = (SWindowData *) window;
        window_data->close = true;
    }
}

//-------------------------------------
void
keyboard_default(struct mfb_window *window, mfb_key key, mfb_key_mod mod, bool is_pressed) {
    kUnused(mod);
    if (key == MFB_KB_KEY_ESCAPE && is_pressed == false) {
        SWindowData *window_data = (SWindowData *) window;
        if (!window_data->close_func || window_data->close_func((struct mfb_window *) window_data)) {
            window_data->close = true;
        }
    }
}

//-------------------------------------
bool
mfb_set_viewport_best_fit(struct mfb_window *window, unsigned old_width, unsigned old_height) {
    if (old_width == 0 || old_height == 0) {
        return false;
    }

    if (window != NULL) {
        SWindowData *window_data = (SWindowData *) window;

        unsigned new_width  = window_data->window_width;
        unsigned new_height = window_data->window_height;

        float scale_x = new_width  / (float) old_width;
        float scale_y = new_height / (float) old_height;
        if (scale_x >= scale_y)
            scale_x = scale_y;
        else
            scale_y = scale_x;

        unsigned final_width  = (unsigned) ((old_width  * scale_x) + 0.5f);
        unsigned final_height = (unsigned) ((old_height * scale_y) + 0.5f);

        unsigned offset_x = (new_width  - final_width)  >> 1;
        unsigned offset_y = (new_height - final_height) >> 1;

        return mfb_set_viewport(window,
                                offset_x,
                                offset_y,
                                final_width,
                                final_height);
    }

    return false;
}

//-------------------------------------
void
mfb_set_mouse_enter_callback(struct mfb_window *window, mfb_mouse_enter_func callback) {
    if (window != NULL) {
        SWindowData *window_data = (SWindowData *) window;
        window_data->mouse_enter_func = callback;
    }
}

//-------------------------------------
bool
mfb_is_window_active(struct mfb_window *window) {
    if (window != NULL) {
        SWindowData *window_data = (SWindowData *) window;
        return window_data->is_active;
    }

    return false;
}

//-------------------------------------
bool
mfb_is_mouse_inside(struct mfb_window *window) {
    if (window != NULL) {
        SWindowData *window_data = (SWindowData *) window;
        return window_data->is_mouse_inside;
    }

    return false;
}

//-------------------------------------
unsigned
mfb_get_window_width(struct mfb_window *window) {
    if (window != NULL) {
        SWindowData *window_data = (SWindowData *) window;
        return window_data->window_width;
    }

    return 0;
}

//-------------------------------------
unsigned
mfb_get_window_height(struct mfb_window *window) {
    if (window != NULL) {
        SWindowData *window_data = (SWindowData *) window;
        return window_data->window_height;
    }

    return 0;
}

//-------------------------------------
int
mfb_get_mouse_x(struct mfb_window *window) {
    if (window != NULL) {
        SWindowData *window_data = (SWindowData *) window;
        return window_data->mouse_pos_x;
    }

    return 0;
}

//-------------------------------------
int
mfb_get_mouse_y(struct mfb_window *window) {
    if (window != NULL) {
        SWindowData *window_data = (SWindowData *) window;
        return window_data->mouse_pos_y;
    }

    return 0;
}

//-------------------------------------
void
mfb_decode_touch(int combined, int *pos, int *id) {
#if defined(__ANDROID__) || (defined(TARGET_OS_IOS) && TARGET_OS_IOS)
    uint32_t packed = (uint32_t) combined;
    if (pos != NULL) {
        *pos = (int) mfb_unpack_pos(packed);
    }
    if (id != NULL) {
        *id = (int) mfb_unpack_id(packed);
    }
#else
    if (pos != NULL) {
        *pos = combined;
    }
    if (id != NULL) {
        *id = 0;
    }
#endif
}

//-------------------------------------
int
mfb_decode_touch_pos(int combined) {
#if defined(__ANDROID__) || (defined(TARGET_OS_IOS) && TARGET_OS_IOS)
    return (int) mfb_unpack_pos((uint32_t) combined);
#else
    return combined;
#endif
}

//-------------------------------------
int
mfb_decode_touch_id(int combined) {
#if defined(__ANDROID__) || (defined(TARGET_OS_IOS) && TARGET_OS_IOS)
    return (int) mfb_unpack_id((uint32_t) combined);
#else
    return 0;
#endif
}

//-------------------------------------
float
mfb_get_mouse_scroll_x(struct mfb_window *window) {
    if (window != NULL) {
        SWindowData *window_data = (SWindowData *) window;
        return window_data->mouse_wheel_x;
    }

    return 0;
}

//-------------------------------------
float
mfb_get_mouse_scroll_y(struct mfb_window *window) {
    if (window != NULL) {
        SWindowData *window_data = (SWindowData *) window;
        return window_data->mouse_wheel_y;
    }

    return 0;
}

//-------------------------------------
const uint8_t *
mfb_get_mouse_button_buffer(struct mfb_window *window) {
    if (window != NULL) {
        SWindowData *window_data = (SWindowData *) window;
        return window_data->mouse_button_status;
    }

    return 0;
}

//-------------------------------------
const uint8_t *
mfb_get_key_buffer(struct mfb_window *window)  {
    if (window != NULL) {
        SWindowData *window_data = (SWindowData *) window;
        return window_data->key_status;
    }
    return 0;
}

//-------------------------------------
const char *
mfb_get_key_name(mfb_key key) {
    switch (key) {
        #define KEY_CASE(NAME, _, STR) case MFB_##NAME: return STR;
        KEY_LIST(KEY_CASE)
        #undef KEY_CASE
    }

    return NULL;    // preprocessor trickery up there should catch every possible value, shouldn't ever run
}

//-------------------------------------
unsigned
mfb_get_drawable_width(struct mfb_window *window) {
    if (window != NULL) {
        SWindowData *window_data = (SWindowData *) window;
        return window_data->dst_width;
    }

    return 0;
}

//-------------------------------------
unsigned
mfb_get_drawable_height(struct mfb_window *window) {
    if (window != NULL) {
        SWindowData *window_data = (SWindowData *) window;
        return window_data->dst_height;
    }

    return 0;
}

//-------------------------------------
unsigned
mfb_get_drawable_offset_x(struct mfb_window *window) {
    if (window != NULL) {
        SWindowData *window_data = (SWindowData *) window;
        return window_data->dst_offset_x;
    }

    return 0;
}

//-------------------------------------
unsigned
mfb_get_drawable_offset_y(struct mfb_window *window) {
    if (window != NULL) {
        SWindowData *window_data = (SWindowData *) window;
        return window_data->dst_offset_y;
    }

    return 0;
}

//-------------------------------------
void
mfb_get_window_size(struct mfb_window *window, unsigned *width, unsigned *height) {
    if (window != NULL) {
        SWindowData *window_data = (SWindowData *) window;
        if (width  != NULL) *width  = window_data->window_width;
        if (height != NULL) *height = window_data->window_height;
        return;
    }

    if (width  != NULL) *width  = 0;
    if (height != NULL) *height = 0;
}

//-------------------------------------
void
mfb_get_drawable_bounds(struct mfb_window *window, unsigned *offset_x, unsigned *offset_y, unsigned *width, unsigned *height) {
    if (window != NULL) {
        SWindowData *window_data = (SWindowData *) window;
        if (offset_x != NULL) *offset_x = window_data->dst_offset_x;
        if (offset_y != NULL) *offset_y = window_data->dst_offset_y;
        if (width    != NULL) *width    = window_data->dst_width;
        if (height   != NULL) *height   = window_data->dst_height;
        return;
    }

    if (offset_x != NULL) *offset_x = 0;
    if (offset_y != NULL) *offset_y = 0;
    if (width    != NULL) *width    = 0;
    if (height   != NULL) *height   = 0;
}

//-------------------------------------
// LOGGER
//-------------------------------------

//-------------------------------------
#if defined(_DEBUG)
    static mfb_log_level g_mfb_log_level = MFB_LOG_DEBUG;
#else
    static mfb_log_level g_mfb_log_level = MFB_LOG_INFO;
#endif

//-------------------------------------
// MINIFB_LOG_LEVEL deliberately wins over mfb_set_log_level(): it exists to
// raise the level of a program that cannot be rebuilt, which a hardcoded call
// would otherwise defeat. MiniFB has no initialization entry point, so the
// variable is resolved on the first log call instead.
//-------------------------------------
static mfb_log_level g_mfb_log_level_env          = MFB_LOG_TRACE;
static bool          g_mfb_log_level_env_valid    = false;
static bool          g_mfb_log_level_env_resolved = false;

//-------------------------------------
// Level names are matched instead of numbers so that reordering mfb_log_level
// cannot silently change what an existing script means.
//-------------------------------------
static bool
log_level_name_matches(const char *value, const char *name) {
    size_t i = 0;

    for (; value[i] != '\0' && name[i] != '\0'; ++i) {
        char letter = value[i];
        if (letter >= 'A' && letter <= 'Z') {
            letter = (char) (letter - 'A' + 'a');
        }
        if (letter != name[i]) {
            return false;
        }
    }

    return value[i] == '\0' && name[i] == '\0';
}

//-------------------------------------
static void
resolve_env_log_level(void) {
    static const char *level_names[] = { "trace", "debug", "info", "warning", "error" };

    // Marked resolved before anything is logged: the complaint below re-enters
    // mfb_log(), which would otherwise recurse.
    g_mfb_log_level_env_resolved = true;

    const char *value = getenv("MINIFB_LOG_LEVEL");
    if (value == NULL || value[0] == '\0') {
        return;
    }

    for (size_t i = 0; i < sizeof(level_names) / sizeof(level_names[0]); ++i) {
        if (log_level_name_matches(value, level_names[i]) == true) {
            g_mfb_log_level_env = (mfb_log_level) i;
            g_mfb_log_level_env_valid = true;
            return;
        }
    }

    MFB_LOG(MFB_LOG_ERROR,
            "MINIFB_LOG_LEVEL=\"%s\" is not a level name; expected trace, debug, info, warning or error. Ignored.",
            value);
}

//-------------------------------------
static mfb_log_level
current_log_level(void) {
    if (g_mfb_log_level_env_resolved == false) {
        resolve_env_log_level();
    }

    if (g_mfb_log_level_env_valid == true) {
        return g_mfb_log_level_env;
    }

    return g_mfb_log_level;
}

//-------------------------------------
void
mfb_log_default(const mfb_log_info *info, const char *tag, const char *message) {
    static const char *level_str[] = { "TRACE", "DEBUG", "INFO", "WARNING", "ERROR" };

    mfb_log_level level = info->level;
    const char *level_aux = (level >= 0 && level < (int)(sizeof(level_str) / sizeof(level_str[0])))
                          ? level_str[level]
                          : "UNKNOWN";

    FILE *out = (level < MFB_LOG_WARNING) ? stdout : stderr;

    if (info->file != NULL && info->file[0] != '\0') {
        const char *file = info->file;
        const char *slash = strrchr(file, '/');
        if (slash == NULL) {
            slash = strrchr(file, '\\');
        }
        if (slash != NULL) {
            file = slash + 1;
        }
        fprintf(out, "[%s] %s: %s:%d (%s): %s\n", tag, level_aux, file, info->line, info->func, message);
    }
    else {
        fprintf(out, "[%s] %s: %s\n", tag, level_aux, message);
    }
}

//-------------------------------------
static mfb_log_func mfb_log_sink = &mfb_log_default;


//-------------------------------------
void
mfb_log(const mfb_log_info *info, const char *tag, const char *message, ...) {
    char buffer[1024];

    if (info->level < current_log_level()) {
        return;
    }

    va_list args;
    va_start(args, message);
    vsnprintf(buffer, sizeof(buffer), message, args);
    va_end(args);

    mfb_log_sink(info, tag, buffer);
}

//-------------------------------------
void
mfb_set_logger(mfb_log_func user_logger) {
    if (user_logger == NULL) {
        mfb_log_sink = mfb_log_default;
    }
    else {
        mfb_log_sink = user_logger;
    }
}

//-------------------------------------
void
mfb_set_log_level(mfb_log_level level) {
    g_mfb_log_level = level;
}

//-------------------------------------
// Android and iOS provide their own platform-specific implementations.
// All other backends return 0 insets. A NULL window is treated as an invalid query.
#if !defined(__ANDROID__) && !(defined(TARGET_OS_IOS) && TARGET_OS_IOS)

bool
mfb_get_display_cutout_insets(struct mfb_window *window, int *left, int *top, int *right, int *bottom) {
    if (left)   *left   = 0;
    if (top)    *top    = 0;
    if (right)  *right  = 0;
    if (bottom) *bottom = 0;

    if (window == NULL) {
        return false;
    }

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

    return true;
}

#endif // !__ANDROID__ && !TARGET_OS_IOS
