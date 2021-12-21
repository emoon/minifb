#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <MiniFB_enums.h>

//-------------------------------------
typedef struct {
    void                    *specific;
    void                    *user_data;

    mfb_active_func         active_func;
    mfb_resize_func         resize_func;
    mfb_close_func          close_func;
	mfb_keyboard_func       keyboard_func;
    mfb_char_input_func     char_input_func;
    mfb_mouse_button_func   mouse_btn_func;
    mfb_mouse_move_func     mouse_move_func;
    mfb_mouse_scroll_func   mouse_wheel_func;

    uint32_t                window_width;
    uint32_t                window_height;

    uint32_t                dst_offset_x;
    uint32_t                dst_offset_y;
    uint32_t                dst_width;
    uint32_t                dst_height;
    float                   factor_x;
    float                   factor_y;
    float                   factor_width;
    float                   factor_height;

    void                    *draw_buffer;
    uint32_t                buffer_width;
    uint32_t                buffer_height;
    uint32_t                buffer_stride;
    
    int32_t                 mouse_pos_x;
    int32_t                 mouse_pos_y;
    float                   mouse_wheel_x;
    float                   mouse_wheel_y;
    uint8_t                 mouse_button_status[8];
    uint8_t                 key_status[512];
    uint32_t                mod_keys;

    bool                    is_active;
    bool                    is_initialized;

    bool                    close;
} SWindowData;
