#include "MiniFB_stretch_internal.h"

// SSE2 is part of every 64-bit x86 processor, so this file needs no option of the compiler and no check of the
// processor. Each function needs at least 4 pixels; the last block of a row ends where the row ends and may overlap
// the one before it

#if defined(MINIFB_STRETCH_SSE2)
#include <emmintrin.h>

//-------------------------------------
// Nearest
//-------------------------------------

// Writes each of the 4 texels of a register 2, 3 or 4 times
//-------------------------------------
static inline void
repeat_block(uint32_t *dst_line, const uint32_t *src_line, uint32_t factor) {
    __m128i texels = _mm_loadu_si128((const __m128i *) src_line);
    __m128i *dst   = (__m128i *) dst_line;
    switch (factor) {
        case 2:
            _mm_storeu_si128(dst + 0, _mm_unpacklo_epi32(texels, texels));
            _mm_storeu_si128(dst + 1, _mm_unpackhi_epi32(texels, texels));
            break;

        case 3:
            _mm_storeu_si128(dst + 0, _mm_shuffle_epi32(texels, _MM_SHUFFLE(1, 0, 0, 0)));
            _mm_storeu_si128(dst + 1, _mm_shuffle_epi32(texels, _MM_SHUFFLE(2, 2, 1, 1)));
            _mm_storeu_si128(dst + 2, _mm_shuffle_epi32(texels, _MM_SHUFFLE(3, 3, 3, 2)));
            break;

        default:
            _mm_storeu_si128(dst + 0, _mm_shuffle_epi32(texels, _MM_SHUFFLE(0, 0, 0, 0)));
            _mm_storeu_si128(dst + 1, _mm_shuffle_epi32(texels, _MM_SHUFFLE(1, 1, 1, 1)));
            _mm_storeu_si128(dst + 2, _mm_shuffle_epi32(texels, _MM_SHUFFLE(2, 2, 2, 2)));
            _mm_storeu_si128(dst + 3, _mm_shuffle_epi32(texels, _MM_SHUFFLE(3, 3, 3, 3)));
            break;
    }
}

//-------------------------------------
void
stretch_nearest_repeat_sse2(uint32_t *dst_line, const uint32_t *src_line, uint32_t src_width, uint32_t factor) {
    uint32_t x = 0;
    for (; x + 4 <= src_width; x += 4) {
        repeat_block(dst_line + x * factor, src_line + x, factor);
    }
    if (x < src_width) {
        x = src_width - 4;
        repeat_block(dst_line + x * factor, src_line + x, factor);
    }
}

// Writes the 4 texels that 4 entries of the table point to. x86 has no fast gather, so they are read one by one
//-------------------------------------
static inline void
take_block(uint32_t *dst, const uint32_t *src_line, const uint32_t *columns) {
    __m128i texels = _mm_set_epi32((int) src_line[columns[3]], (int) src_line[columns[2]],
                                   (int) src_line[columns[1]], (int) src_line[columns[0]]);
    _mm_storeu_si128((__m128i *) dst, texels);
}

//-------------------------------------
void
stretch_nearest_take_sse2(uint32_t *dst_line, const uint32_t *src_line, const uint32_t *columns, uint32_t width) {
    uint32_t x = 0;
    for (; x + 4 <= width; x += 4) {
        take_block(dst_line + x, src_line, columns + x);
    }
    if (x < width) {
        x = width - 4;
        take_block(dst_line + x, src_line, columns + x);
    }
}

//-------------------------------------
// Bilinear
//-------------------------------------

// Interpolates 4 destination columns across one source row, into 16 channels of 16 bits in pixel byte order:
// p0 * 256 + (p1 - p0) * fx, exact modulo 2^16 in each lane
//-------------------------------------
static inline void
interpolate_block(uint16_t *row, const uint32_t *src_line, const SStretchColumn *columns) {
    const __m128i low_bytes = _mm_set1_epi16(0x00ff);
    __m128i p0 = _mm_set_epi32((int) src_line[columns[3].col0], (int) src_line[columns[2].col0],
                               (int) src_line[columns[1].col0], (int) src_line[columns[0].col0]);
    __m128i p1 = _mm_set_epi32((int) src_line[columns[3].col1], (int) src_line[columns[2].col1],
                               (int) src_line[columns[1].col1], (int) src_line[columns[0].col1]);
    __m128i fx = _mm_set_epi32((int) columns[3].fx, (int) columns[2].fx, (int) columns[1].fx, (int) columns[0].fx);
    fx = _mm_or_si128(fx, _mm_slli_epi32(fx, 16));

    // Blue and red stay in place, green and alpha move down to the low byte of each 16-bit lane
    __m128i br0 = _mm_and_si128(p0, low_bytes);
    __m128i ga0 = _mm_srli_epi16(p0, 8);
    __m128i br1 = _mm_and_si128(p1, low_bytes);
    __m128i ga1 = _mm_srli_epi16(p1, 8);
    __m128i br  = _mm_add_epi16(_mm_slli_epi16(br0, 8), _mm_mullo_epi16(_mm_sub_epi16(br1, br0), fx));
    __m128i ga  = _mm_add_epi16(_mm_slli_epi16(ga0, 8), _mm_mullo_epi16(_mm_sub_epi16(ga1, ga0), fx));
    _mm_storeu_si128((__m128i *) row,       _mm_unpacklo_epi16(br, ga));
    _mm_storeu_si128((__m128i *) (row + 8), _mm_unpackhi_epi16(br, ga));
}

//-------------------------------------
void
stretch_bilinear_row_sse2(uint16_t *row, const uint32_t *src_line, const SStretchColumn *columns, uint32_t width) {
    uint32_t x = 0;
    for (; x + 4 <= width; x += 4) {
        interpolate_block(row + x * 4, src_line, columns + x);
    }
    if (x < width) {
        x = width - 4;
        interpolate_block(row + x * 4, src_line, columns + x);
    }
}

// Mixes two rows of 16-bit channels, 8 channels (2 pixels) per register: (top * fy1 + bottom * fy2 + 0x8000) >> 16.
// The 24-bit sums do not fit in a 16-bit lane, so each value is split in its high and low bytes, whose sums do
//-------------------------------------
static inline __m128i
blend_channels(__m128i top, __m128i bottom, __m128i fy1, __m128i fy2) {
    const __m128i low_byte = _mm_set1_epi16(0x00ff);
    __m128i hi = _mm_add_epi16(_mm_mullo_epi16(_mm_srli_epi16(top, 8), fy1),
                               _mm_mullo_epi16(_mm_srli_epi16(bottom, 8), fy2));
    __m128i lo = _mm_add_epi16(_mm_mullo_epi16(_mm_and_si128(top, low_byte), fy1),
                               _mm_mullo_epi16(_mm_and_si128(bottom, low_byte), fy2));
    __m128i sum = _mm_add_epi16(_mm_add_epi16(hi, _mm_set1_epi16(128)), _mm_srli_epi16(lo, 8));
    return _mm_srli_epi16(sum, 8);
}

// Mixes 4 pixels of two rows of 16-bit channels in pixel byte order, and writes them as 4 pixels of 32 bits
//-------------------------------------
static inline void
blend_block(uint32_t *dst, const uint16_t *top, const uint16_t *bottom, __m128i fy1, __m128i fy2) {
    __m128i first  = blend_channels(_mm_loadu_si128((const __m128i *) top),
                                    _mm_loadu_si128((const __m128i *) bottom), fy1, fy2);
    __m128i second = blend_channels(_mm_loadu_si128((const __m128i *) (top + 8)),
                                    _mm_loadu_si128((const __m128i *) (bottom + 8)), fy1, fy2);
    _mm_storeu_si128((__m128i *) dst, _mm_packus_epi16(first, second));
}

//-------------------------------------
void
stretch_bilinear_blend_sse2(uint32_t *dst_line, const uint16_t *top, const uint16_t *bottom, uint32_t fy, uint32_t width) {
    const __m128i fy1 = _mm_set1_epi16((short) (256 - fy));
    const __m128i fy2 = _mm_set1_epi16((short) fy);

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
