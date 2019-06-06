#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <MiniFB_enums.h>

typedef struct {
    void                    *specific;
    void                    *user_data;

    mfb_active_func         active_func;
    mfb_resize_func         resize_func;
    mfb_keyboard_func       keyboard_func;
    mfb_char_input_func     char_input_func;
    mfb_mouse_btn_func      mouse_btn_func;
    mfb_mouse_move_func     mouse_move_func;
    mfb_mouse_scroll_func   mouse_wheel_func;

    uint32_t                window_width;
    uint32_t                window_height;

    uint32_t                dst_offset_x;
    uint32_t                dst_offset_y;
    uint32_t                dst_width;
    uint32_t                dst_height;

    void                    *draw_buffer;
    uint32_t                buffer_width;
    uint32_t                buffer_height;
    uint32_t                buffer_stride;
    uint32_t                mod_keys;
    bool                    close;
} SWindowData;
