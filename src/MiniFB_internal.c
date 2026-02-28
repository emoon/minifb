#include "MiniFB_internal.h"
#include <stdint.h>
#include <stddef.h>

//#define kUseBilinearInterpolation

//-------------------------------------
static uint32_t
interpolate(uint32_t *src_image, uint32_t x, uint32_t y, uint32_t src_offset_x, uint32_t src_offset_y, uint32_t src_width, uint32_t src_height, uint32_t src_pitch) {
    uint32_t inc_x = x + 1 < src_width ? 1 : 0;
    uint32_t inc_y = y + 1 < src_height ? src_pitch : 0;
    uint8_t *p00 = (uint8_t *) &src_image[(src_offset_x >> 16)];
    uint8_t *p01 = (uint8_t *) &src_image[(src_offset_x >> 16) + inc_x];
    uint8_t *p10 = (uint8_t *) &src_image[(src_offset_x >> 16) + inc_y];
    uint8_t *p11 = (uint8_t *) &src_image[(src_offset_x >> 16) + inc_y + inc_x];

    uint32_t wx2 = src_offset_x & 0xffff;
    uint32_t wy2 = src_offset_y & 0xffff;
    uint32_t wx1 = 0x10000 - wx2;
    uint32_t wy1 = 0x10000 - wy2;

    uint32_t w1 = ((uint64_t) wx1 * wy1) >> 16;
    uint32_t w2 = ((uint64_t) wx2 * wy1) >> 16;
    uint32_t w3 = ((uint64_t) wx1 * wy2) >> 16;
    uint32_t w4 = ((uint64_t) wx2 * wy2) >> 16;

    // If you don't have uint64_t
    //uint32_t b = (((p00[0] * wx1 + p01[0] * wx2) >> 16) * wy1 + ((p10[0] * wx1 + p11[0] * wx2) >> 16) * wy2) >> 16;
    //uint32_t g = (((p00[1] * wx1 + p01[1] * wx2) >> 16) * wy1 + ((p10[1] * wx1 + p11[1] * wx2) >> 16) * wy2) >> 16;
    //uint32_t r = (((p00[2] * wx1 + p01[2] * wx2) >> 16) * wy1 + ((p10[2] * wx1 + p11[2] * wx2) >> 16) * wy2) >> 16;
    //uint32_t a = (((p00[3] * wx1 + p01[3] * wx2) >> 16) * wy1 + ((p10[3] * wx1 + p11[3] * wx2) >> 16) * wy2) >> 16;

    uint32_t b = ((p00[0] * w1 + p01[0] * w2) + (p10[0] * w3 + p11[0] * w4)) >> 16;
    uint32_t g = ((p00[1] * w1 + p01[1] * w2) + (p10[1] * w3 + p11[1] * w4)) >> 16;
    uint32_t r = ((p00[2] * w1 + p01[2] * w2) + (p10[2] * w3 + p11[2] * w4)) >> 16;
    uint32_t a = ((p00[3] * w1 + p01[3] * w2) + (p10[3] * w3 + p11[3] * w4)) >> 16;

    return (a << 24) + (r << 16) + (g << 8) + b;
}

// Only for 32 bits images
//-------------------------------------
void
stretch_image(uint32_t *src_image, uint32_t src_x, uint32_t src_y, uint32_t src_width, uint32_t src_height, uint32_t src_pitch,
              uint32_t *dst_image, uint32_t dst_x, uint32_t dst_y, uint32_t dst_width, uint32_t dst_height, uint32_t dst_pitch) {

    uint32_t    x, y;
    uint32_t    src_offset_x, src_offset_y;

    if (src_image == NULL || dst_image == NULL)
        return;

    src_image += src_x + src_y * src_pitch;
    dst_image += dst_x + dst_y * dst_pitch;

    const uint32_t delta_x = (src_width  << 16) / dst_width;
    const uint32_t delta_y = (src_height << 16) / dst_height;

    src_offset_y = 0;
    for(y=0; y<dst_height; ++y) {
        src_offset_x = 0;
        for(x=0; x<dst_width; ++x) {
#if defined(kUseBilinearInterpolation)
            dst_image[x] = interpolate(src_image, x+src_x, y+src_y, src_offset_x, src_offset_y, src_width, src_height, src_pitch);
#else
            dst_image[x] = src_image[src_offset_x >> 16];
#endif
            src_offset_x += delta_x;
        }

        src_offset_y += delta_y;
        if (src_offset_y >= 0x10000) {
            src_image += (src_offset_y >> 16) * src_pitch;
            src_offset_y &= 0xffff;
        }
        dst_image += dst_pitch;
    }
}

//-------------------------------------
void
calc_dst_factor(SWindowData *window_data, uint32_t width, uint32_t height) {
    if (window_data->dst_width == 0) {
        window_data->dst_width = width;
    }
    window_data->factor_x     = (float) window_data->dst_offset_x / (float) width;
    window_data->factor_width = (float) window_data->dst_width    / (float) width;

    if (window_data->dst_height == 0) {
        window_data->dst_height = height;
    }
    window_data->factor_y      = (float) window_data->dst_offset_y / (float) height;
    window_data->factor_height = (float) window_data->dst_height   / (float) height;
}

//-------------------------------------
void
resize_dst(SWindowData *window_data, uint32_t width, uint32_t height) {
    window_data->dst_offset_x = (uint32_t) (width  * window_data->factor_x);
    window_data->dst_offset_y = (uint32_t) (height * window_data->factor_y);
    window_data->dst_width    = (uint32_t) (width  * window_data->factor_width);
    window_data->dst_height   = (uint32_t) (height * window_data->factor_height);
}

#if !defined(USE_OPENGL_API) && !defined(USE_METAL_API)

//-------------------------------------
void
set_target_fps_aux() {
}

#endif
