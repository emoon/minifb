#pragma once

#include <MiniFB.h>
#include "WindowData.h"
#include <stddef.h>

//-------------------------------------
#define kCall(func, ...)    if (window_data && window_data->func) window_data->func((struct mfb_window *) window_data, __VA_ARGS__);
#define kUnused(var)        (void) var;

//-------------------------------------
// Mobile backends may encode a pointer id in the upper bits of mouse positions.
// Keep these constants centralized so packing/unpacking stays compatible.
//-------------------------------------
#define MFB_COMBINED_POS_ID_BITS   4u
#define MFB_COMBINED_POS_BITS      (32u - MFB_COMBINED_POS_ID_BITS)
#define MFB_COMBINED_POS_ID_SHIFT  MFB_COMBINED_POS_BITS
#define MFB_COMBINED_POS_MASK      ((1u << MFB_COMBINED_POS_BITS) - 1u)
#define MFB_COMBINED_POS_SIGN_BIT  (1u << (MFB_COMBINED_POS_BITS - 1u))
#define MFB_COMBINED_ID_MASK       ((1u << MFB_COMBINED_POS_ID_BITS) - 1u)

//-------------------------------------
static inline int
mfb_pack_pos_id(int32_t pos, uint32_t id) {
    uint32_t packed = (((uint32_t) pos) & MFB_COMBINED_POS_MASK)
                    | ((id & MFB_COMBINED_ID_MASK) << MFB_COMBINED_POS_ID_SHIFT);
    return (int) packed;
}

//-------------------------------------
static inline int32_t
mfb_unpack_pos(uint32_t combined) {
    uint32_t pos_bits = combined & MFB_COMBINED_POS_MASK;
    if ((pos_bits & MFB_COMBINED_POS_SIGN_BIT) != 0u) {
        pos_bits |= ~MFB_COMBINED_POS_MASK;
    }
    return (int32_t) pos_bits;
}

//-------------------------------------
static inline uint32_t
mfb_unpack_id(uint32_t combined) {
    return (combined >> MFB_COMBINED_POS_ID_SHIFT) & MFB_COMBINED_ID_MASK;
}

//-------------------------------------
typedef struct mfb_timer {
    int64_t     start_ticks;
    int64_t     last_delta_ticks;
    uint64_t    accumulated_ticks;
    int64_t     accumulated_error_ticks;
} mfb_timer;

//-------------------------------------
#if defined(__cplusplus)
extern "C" {
#endif
    #undef MFB_LOG
    #if defined(__cplusplus)
        #define MFB_LOG(level, ...)                                                      \
            do {                                                                          \
                const mfb_log_info mfb_log_info_aux = { level, __FILE__, MFB_FUNC_NAME, __LINE__ }; \
                mfb_log(&mfb_log_info_aux, "MiniFB", __VA_ARGS__);                      \
            } while (0)
    #else
        #define MFB_LOG(level, ...) mfb_log(&(mfb_log_info){ level, __FILE__, MFB_FUNC_NAME, __LINE__ }, "MiniFB", __VA_ARGS__)
    #endif

    extern short int g_keycodes[MFB_MAX_KEYS];
    void keyboard_default(struct mfb_window *window, mfb_key key, mfb_key_mod mod, bool is_pressed);
    void release_cpp_stub(struct mfb_window *window);

    bool calculate_buffer_layout(uint32_t width, uint32_t height, uint32_t *stride_out, size_t *total_bytes_out);
    bool mfb_validate_viewport(const SWindowData *window_data,
                               unsigned offset_x, unsigned offset_y,
                               unsigned width, unsigned height,
                               const char *backend_name);
    void calc_dst_factor(SWindowData *window_data, uint32_t width, uint32_t height);
    void resize_dst(SWindowData *window_data, uint32_t width, uint32_t height);
    void set_target_fps_aux();

    void stretch_image(uint32_t *src_image, uint32_t src_x, uint32_t src_y,
                       uint32_t src_width, uint32_t src_height, uint32_t src_pitch,
                       uint32_t *dst_image, uint32_t dst_x, uint32_t dst_y,
                       uint32_t dst_width, uint32_t dst_height, uint32_t dst_pitch);

#if defined(__cplusplus)
}
#endif
