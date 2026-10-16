#include "MiniFB_stretch_internal.h"

// The bilinear passes on 4 pixels at a time, for ARM. Each function needs at least 4 pixels; the last block of a row
// ends where the row ends and may overlap the one before it

#if defined(MINIFB_STRETCH_NEON)
#include <arm_neon.h>

// Interpolates 4 destination columns across one source row, into 16 channels of 16 bits in pixel byte order:
// p0 * 256 + (p1 - p0) * fx, exact modulo 2^16 in each lane
//-------------------------------------
static inline void
interpolate_block(uint16_t *row, const uint32_t *src_line, const SStretchColumn *columns) {
    uint32x4x3_t table = vld3q_u32(&columns[0].col0);
    uint32x4_t   p0    = vdupq_n_u32(0);
    uint32x4_t   p1    = vdupq_n_u32(0);
    p0 = vld1q_lane_u32(&src_line[columns[0].col0], p0, 0);
    p0 = vld1q_lane_u32(&src_line[columns[1].col0], p0, 1);
    p0 = vld1q_lane_u32(&src_line[columns[2].col0], p0, 2);
    p0 = vld1q_lane_u32(&src_line[columns[3].col0], p0, 3);
    p1 = vld1q_lane_u32(&src_line[columns[0].col1], p1, 0);
    p1 = vld1q_lane_u32(&src_line[columns[1].col1], p1, 1);
    p1 = vld1q_lane_u32(&src_line[columns[2].col1], p1, 2);
    p1 = vld1q_lane_u32(&src_line[columns[3].col1], p1, 3);

    // Each weight goes to the 4 bytes of its pixel
    uint8x16_t fx  = vreinterpretq_u8_u32(vmulq_n_u32(table.val[2], 0x01010101u));
    uint8x16_t t0  = vreinterpretq_u8_u32(p0);
    uint8x16_t t1  = vreinterpretq_u8_u32(p1);
    uint16x8_t low = vmlaq_u16(vshll_n_u8(vget_low_u8(t0), 8),
                               vsubl_u8(vget_low_u8(t1), vget_low_u8(t0)), vmovl_u8(vget_low_u8(fx)));
    uint16x8_t high = vmlaq_u16(vshll_n_u8(vget_high_u8(t0), 8),
                                vsubl_u8(vget_high_u8(t1), vget_high_u8(t0)), vmovl_u8(vget_high_u8(fx)));
    vst1q_u16(row,     low);
    vst1q_u16(row + 8, high);
}

//-------------------------------------
void
stretch_bilinear_row_neon(uint16_t *row, const uint32_t *src_line, const SStretchColumn *columns, uint32_t width) {
    uint32_t x = 0;
    for (; x + 4 <= width; x += 4) {
        interpolate_block(row + x * 4, src_line, columns + x);
    }
    if (x < width) {
        x = width - 4;
        interpolate_block(row + x * 4, src_line, columns + x);
    }
}

// (top * fy1 + bottom * fy2 + 0x8000) >> 16 for 8 channels. The rounding shift adds the 0x8000
//-------------------------------------
static inline uint8x8_t
blend_channels(uint16x8_t top, uint16x8_t bottom, uint16x4_t fy1, uint16x4_t fy2) {
    uint32x4_t low  = vmlal_u16(vmull_u16(vget_low_u16(top), fy1), vget_low_u16(bottom), fy2);
    uint32x4_t high = vmlal_u16(vmull_u16(vget_high_u16(top), fy1), vget_high_u16(bottom), fy2);
    return vmovn_u16(vcombine_u16(vrshrn_n_u32(low, 16), vrshrn_n_u32(high, 16)));
}

// Mixes 4 pixels of two rows of 16-bit channels in pixel byte order, and writes them as 4 pixels of 32 bits
//-------------------------------------
static inline void
blend_block(uint32_t *dst, const uint16_t *top, const uint16_t *bottom, uint16x4_t fy1, uint16x4_t fy2) {
    uint8x8_t first  = blend_channels(vld1q_u16(top),     vld1q_u16(bottom),     fy1, fy2);
    uint8x8_t second = blend_channels(vld1q_u16(top + 8), vld1q_u16(bottom + 8), fy1, fy2);
    vst1q_u32(dst, vreinterpretq_u32_u8(vcombine_u8(first, second)));
}

//-------------------------------------
void
stretch_bilinear_blend_neon(uint32_t *dst_line, const uint16_t *top, const uint16_t *bottom, uint32_t fy, uint32_t width) {
    const uint16x4_t fy1 = vdup_n_u16((uint16_t) (256 - fy));
    const uint16x4_t fy2 = vdup_n_u16((uint16_t) fy);

    uint32_t x = 0;
    for (; x + 4 <= width; x += 4) {
        blend_block(dst_line + x, top + x * 4, bottom + x * 4, fy1, fy2);
    }
    if (x < width) {
        x = width - 4;
        blend_block(dst_line + x, top + x * 4, bottom + x * 4, fy1, fy2);
    }
}
#endif
