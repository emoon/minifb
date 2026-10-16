#include "MiniFB_stretch_internal.h"

// The bilinear passes on 8 pixels at a time. This file is built with the options of x86-64-v3, and is only called
// when mfb_cpu_has_x64v3 says the processor and the operating system support it. Each function needs at least 8
// pixels; the last block of a row ends where the row ends and may overlap the one before it

#if defined(MINIFB_STRETCH_AVX2)
#include <immintrin.h>

// Interpolates 8 destination columns across one source row, into 32 channels of 16 bits in pixel byte order:
// p0 * 256 + (p1 - p0) * fx, exact modulo 2^16 in each lane
//-------------------------------------
static inline void
interpolate_block(uint16_t *row, const uint32_t *src_line, const SStretchColumn *columns) {
    const __m256i low_bytes = _mm256_set1_epi16(0x00ff);
    __m256i p0 = _mm256_set_epi32((int) src_line[columns[7].col0], (int) src_line[columns[6].col0],
                                  (int) src_line[columns[5].col0], (int) src_line[columns[4].col0],
                                  (int) src_line[columns[3].col0], (int) src_line[columns[2].col0],
                                  (int) src_line[columns[1].col0], (int) src_line[columns[0].col0]);
    __m256i p1 = _mm256_set_epi32((int) src_line[columns[7].col1], (int) src_line[columns[6].col1],
                                  (int) src_line[columns[5].col1], (int) src_line[columns[4].col1],
                                  (int) src_line[columns[3].col1], (int) src_line[columns[2].col1],
                                  (int) src_line[columns[1].col1], (int) src_line[columns[0].col1]);
    __m256i fx = _mm256_set_epi32((int) columns[7].fx, (int) columns[6].fx, (int) columns[5].fx, (int) columns[4].fx,
                                  (int) columns[3].fx, (int) columns[2].fx, (int) columns[1].fx, (int) columns[0].fx);
    fx = _mm256_or_si256(fx, _mm256_slli_epi32(fx, 16));

    // Blue and red stay in place, green and alpha move down to the low byte of each 16-bit lane
    __m256i br0 = _mm256_and_si256(p0, low_bytes);
    __m256i ga0 = _mm256_srli_epi16(p0, 8);
    __m256i br1 = _mm256_and_si256(p1, low_bytes);
    __m256i ga1 = _mm256_srli_epi16(p1, 8);
    __m256i br  = _mm256_add_epi16(_mm256_slli_epi16(br0, 8), _mm256_mullo_epi16(_mm256_sub_epi16(br1, br0), fx));
    __m256i ga  = _mm256_add_epi16(_mm256_slli_epi16(ga0, 8), _mm256_mullo_epi16(_mm256_sub_epi16(ga1, ga0), fx));
    // unpack works inside each 128-bit half: low has the pixels 0 1 4 5, and high has 2 3 6 7
    __m256i low  = _mm256_unpacklo_epi16(br, ga);
    __m256i high = _mm256_unpackhi_epi16(br, ga);
    _mm256_storeu_si256((__m256i *) row,        _mm256_permute2x128_si256(low, high, 0x20));
    _mm256_storeu_si256((__m256i *) (row + 16), _mm256_permute2x128_si256(low, high, 0x31));
}

//-------------------------------------
void
stretch_bilinear_row_avx2(uint16_t *row, const uint32_t *src_line, const SStretchColumn *columns, uint32_t width) {
    uint32_t x = 0;
    for (; x + 8 <= width; x += 8) {
        interpolate_block(row + x * 4, src_line, columns + x);
    }
    if (x < width) {
        x = width - 8;
        interpolate_block(row + x * 4, src_line, columns + x);
    }
}

// Mixes two rows of 16-bit channels, 16 channels (4 pixels) per register: (top * fy1 + bottom * fy2 + 0x8000) >> 16.
// The 24-bit sums do not fit in a 16-bit lane, so each value is split in its high and low bytes, whose sums do
//-------------------------------------
static inline __m256i
blend_channels(__m256i top, __m256i bottom, __m256i fy1, __m256i fy2) {
    const __m256i low_byte = _mm256_set1_epi16(0x00ff);
    __m256i hi = _mm256_add_epi16(_mm256_mullo_epi16(_mm256_srli_epi16(top, 8), fy1),
                                  _mm256_mullo_epi16(_mm256_srli_epi16(bottom, 8), fy2));
    __m256i lo = _mm256_add_epi16(_mm256_mullo_epi16(_mm256_and_si256(top, low_byte), fy1),
                                  _mm256_mullo_epi16(_mm256_and_si256(bottom, low_byte), fy2));
    __m256i sum = _mm256_add_epi16(_mm256_add_epi16(hi, _mm256_set1_epi16(128)), _mm256_srli_epi16(lo, 8));
    return _mm256_srli_epi16(sum, 8);
}

// Mixes 8 pixels of two rows of 16-bit channels in pixel byte order, and writes them as 8 pixels of 32 bits
//-------------------------------------
static inline void
blend_block(uint32_t *dst, const uint16_t *top, const uint16_t *bottom, __m256i fy1, __m256i fy2) {
    __m256i first  = blend_channels(_mm256_loadu_si256((const __m256i *) top),
                                    _mm256_loadu_si256((const __m256i *) bottom), fy1, fy2);
    __m256i second = blend_channels(_mm256_loadu_si256((const __m256i *) (top + 16)),
                                    _mm256_loadu_si256((const __m256i *) (bottom + 16)), fy1, fy2);
    // packus works inside each 128-bit half, so the pixels come out in the order 0 1 4 5 2 3 6 7
    __m256i packed = _mm256_packus_epi16(first, second);
    _mm256_storeu_si256((__m256i *) dst, _mm256_permute4x64_epi64(packed, _MM_SHUFFLE(3, 1, 2, 0)));
}

//-------------------------------------
void
stretch_bilinear_blend_avx2(uint32_t *dst_line, const uint16_t *top, const uint16_t *bottom, uint32_t fy, uint32_t width) {
    const __m256i fy1 = _mm256_set1_epi16((short) (256 - fy));
    const __m256i fy2 = _mm256_set1_epi16((short) fy);

    uint32_t x = 0;
    for (; x + 8 <= width; x += 8) {
        blend_block(dst_line + x, top + x * 4, bottom + x * 4, fy1, fy2);
    }
    if (x < width) {
        x = width - 8;
        blend_block(dst_line + x, top + x * 4, bottom + x * 4, fy1, fy2);
    }
}
#endif
