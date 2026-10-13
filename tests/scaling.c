#include <MiniFB_internal.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(MINIFB_USE_MMGR)
    #include "mmgr.h"
#endif

static uint32_t
expected_pixel(const uint32_t *src, unsigned width, unsigned height, unsigned pitch,
               unsigned x, unsigned y, unsigned dst_width, unsigned dst_height) {
#if defined(MINIFB_USE_BILINEAR)
    double sx = ((double) x + 0.5) * width / dst_width - 0.5;
    double sy = ((double) y + 0.5) * height / dst_height - 0.5;
    if (sx < 0) sx = 0;
    if (sy < 0) sy = 0;
    if (sx > width - 1) sx = width - 1;
    if (sy > height - 1) sy = height - 1;
    unsigned x0 = (unsigned) sx;
    unsigned y0 = (unsigned) sy;
    unsigned x1 = x0 + 1 < width ? x0 + 1 : x0;
    unsigned y1 = y0 + 1 < height ? y0 + 1 : y0;
    double fx = sx - x0;
    double fy = sy - y0;
    uint32_t result = 0;
    for (unsigned shift = 0; shift < 32; shift += 8) {
        double top = ((src[(size_t) y0 * pitch + x0] >> shift) & 255) * (1 - fx)
                   + ((src[(size_t) y0 * pitch + x1] >> shift) & 255) * fx;
        double bottom = ((src[(size_t) y1 * pitch + x0] >> shift) & 255) * (1 - fx)
                      + ((src[(size_t) y1 * pitch + x1] >> shift) & 255) * fx;
        result |= (uint32_t) (top * (1 - fy) + bottom * fy) << shift;
    }
    return result;
#else
    return src[(size_t) ((uint64_t) y * height / dst_height) * pitch
               + (uint64_t) x * width / dst_width];
#endif
}

static int
check_scale(unsigned width, unsigned height, unsigned dst_width, unsigned dst_height, bool flat) {
    unsigned src_pitch = width + 3;
    unsigned dst_pitch = dst_width + 5;
    size_t src_count = (size_t) src_pitch * (height + 2);
    size_t dst_count = (size_t) dst_pitch * (dst_height + 2);
    uint32_t *src = malloc(src_count * sizeof(uint32_t));
    uint32_t *dst = malloc(dst_count * sizeof(uint32_t));
    if (src == NULL || dst == NULL) {
        free(src);
        free(dst);
        return 1;
    }
    for (size_t i = 0; i < src_count; ++i) src[i] = flat ? 0xffffffff : (uint32_t) (i * 0x5d3b1729u);
    for (size_t i = 0; i < dst_count; ++i) dst[i] = 0x12345678;

    stretch_image(src, 1, 1, width, height, src_pitch, dst, 2, 1, dst_width, dst_height, dst_pitch);
    int failures = 0;
    for (unsigned y = 0; y < dst_height + 2; ++y) {
        for (unsigned x = 0; x < dst_pitch; ++x) {
            uint32_t actual = dst[(size_t) y * dst_pitch + x];
            if (y >= 1 && y <= dst_height && x >= 2 && x < dst_width + 2) {
                uint32_t expected = flat == true ? 0xffffffff :
                    expected_pixel(src + src_pitch + 1, width, height, src_pitch,
                                   x - 2, y - 1, dst_width, dst_height);
                for (unsigned shift = 0; shift < 32; shift += 8) {
                    int difference = (int) ((actual >> shift) & 255) - (int) ((expected >> shift) & 255);
#if defined(MINIFB_USE_BILINEAR)
                    int tolerance = flat == true ? 0 : 2;
#else
                    int tolerance = 0;
#endif
                    if (difference < -tolerance || difference > tolerance) ++failures;
                }
            }
            else if (actual != 0x12345678) {
                ++failures;
            }
        }
    }
    if (failures != 0) {
        fprintf(stderr, "Scaling %ux%u to %ux%u: %d mismatches\n", width, height, dst_width, dst_height, failures);
    }
    free(src);
    free(dst);
    return failures;
}

int
main(void) {
#if defined(MINIFB_USE_MMGR)
    m_setLogFiles(MINIFB_MMGR_MEMORY_LOG, MINIFB_MMGR_LEAKS_LOG);
#endif

    int failures = 0;
    failures += check_scale(2, 2, 4, 4, false);
    failures += check_scale(3, 2, 5, 7, false);
    failures += check_scale(8, 6, 3, 2, false);
    failures += check_scale(1, 1, 9, 7, false);
    failures += check_scale(3, 5, 3, 5, false);
    failures += check_scale(2, 2, 5, 5, true);
    failures += check_scale(65536, 1, 2, 1, false);
    failures += check_scale(1, 65536, 1, 2, false);

    uint32_t pixel = 0x12345678;
    uint32_t result = 0xffffffff;
    stretch_image(&pixel, 0, 0, 0, 1, 1, &result, 0, 0, 1, 1, 1);
    if (result != 0xffffffff) ++failures;
    stretch_image(&pixel, 0, 0, 1, 1, 1, &result, 0, 0, 0, 1, 1);
    if (result != 0xffffffff) ++failures;

#if defined(MINIFB_USE_BILINEAR)
    SWindowData window_data = {0};
    window_data.draw_buffer = &pixel;
    window_data.buffer_width = window_data.buffer_height = 1;
    window_data.buffer_stride = 4;
    if (mfb_get_scaled_buffer(&window_data, 1, 1) != &pixel) ++failures;
    uint32_t *buffer = mfb_get_scaled_buffer(&window_data, 4, 4);
    if (buffer == NULL) {
        ++failures;
    }
    else {
        for (unsigned i = 0; i < 16; ++i) {
            if (buffer[i] != pixel) ++failures;
        }
        size_t capacity = window_data.scaled_buffer_size;
        if (mfb_get_scaled_buffer(&window_data, 2, 3) != buffer) ++failures;
        if (window_data.scaled_buffer_size != capacity) ++failures;
        if (mfb_store_title(&window_data, "Scaling test") == NULL) ++failures;
        if (window_data.scaled_buffer != buffer || buffer[0] != pixel) ++failures;
        if (mfb_get_scaled_buffer(&window_data, UINT32_MAX, 1) != NULL) ++failures;
        if (window_data.scaled_buffer != buffer || window_data.scaled_buffer_size != capacity) ++failures;
    }
    free(window_data.scaled_buffer);
    free(window_data.safe_title);
#endif

    if (failures != 0) return EXIT_FAILURE;
    puts("Scaling checks passed.");
    return EXIT_SUCCESS;
}
