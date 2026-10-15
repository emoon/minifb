#pragma once

#include <stdint.h>

// What the scalar scaling in MiniFB_stretch.c shares with the SIMD files. Each SIMD function needs at least one full
// block: 4 pixels for SSE2 and NEON, 8 for AVX2

#if defined(_M_X64) || defined(__x86_64__)
    #define MINIFB_STRETCH_SSE2
#elif defined(__ARM_NEON) || defined(__ARM_NEON__) || defined(_M_ARM64)
    #define MINIFB_STRETCH_NEON
    #undef MINIFB_STRETCH_AVX2
#else
    // A universal build of macOS defines it for every architecture, and only the x86_64 part has AVX2
    #undef MINIFB_STRETCH_AVX2
#endif

// The two source columns and the horizontal weight, from 0 to 255, of one destination column
//-------------------------------------
typedef struct {
    uint32_t    col0;
    uint32_t    col1;
    uint32_t    fx;
} SStretchColumn;

#if defined(MINIFB_STRETCH_SSE2)
    void    stretch_nearest_repeat_sse2(uint32_t *dst_line, const uint32_t *src_line, uint32_t src_width, uint32_t factor);
    void    stretch_nearest_take_sse2(uint32_t *dst_line, const uint32_t *src_line, const uint32_t *columns, uint32_t width);

    // The rows hold 16 bits per channel, in the byte order of a pixel: blue, green, red, alpha
    void    stretch_bilinear_row_sse2(uint16_t *row, const uint32_t *src_line, const SStretchColumn *columns, uint32_t width);
    void    stretch_bilinear_blend_sse2(uint32_t *dst_line, const uint16_t *top, const uint16_t *bottom, uint32_t fy, uint32_t width);
#endif

#if defined(MINIFB_STRETCH_NEON)
    // The rows hold 16 bits per channel, in the byte order of a pixel, as in SSE2
    void    stretch_bilinear_row_neon(uint16_t *row, const uint32_t *src_line, const SStretchColumn *columns, uint32_t width);
    void    stretch_bilinear_blend_neon(uint32_t *dst_line, const uint16_t *top, const uint16_t *bottom, uint32_t fy, uint32_t width);
#endif

#if defined(MINIFB_STRETCH_AVX2)
    void    stretch_bilinear_row_avx2(uint16_t *row, const uint32_t *src_line, const SStretchColumn *columns, uint32_t width);
    void    stretch_bilinear_blend_avx2(uint32_t *dst_line, const uint16_t *top, const uint16_t *bottom, uint32_t fy, uint32_t width);
#endif
