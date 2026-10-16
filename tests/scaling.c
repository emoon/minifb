#include <MiniFB_internal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(MINIFB_USE_MMGR)
    #include "mmgr.h"
#endif

// The center of destination pixel i in 1/256 of a source texel, rounded and clamped to the first and the last texel
static uint32_t
sample_position(unsigned i, unsigned size, unsigned dst_size) {
    int64_t numerator = ((2 * (int64_t) i + 1) * size - dst_size) * 256 + dst_size;
    int64_t divisor   = 2 * (int64_t) dst_size;
    int64_t sample    = numerator >= 0 ? numerator / divisor : -((-numerator + divisor - 1) / divisor);
    int64_t last      = ((int64_t) size - 1) * 256;
    if (sample < 0) {
        sample = 0;
    }

    if (sample > last) {
        sample = last;
    }

    return (uint32_t) sample;
}

static uint32_t
expected_pixel(const uint32_t *src, unsigned width, unsigned height, unsigned pitch,
               unsigned x, unsigned y, unsigned dst_width, unsigned dst_height, mfb_stretch_mode mode) {
    if (mode == MFB_STRETCH_BILINEAR) {
        uint32_t sx = sample_position(x, width, dst_width);
        uint32_t sy = sample_position(y, height, dst_height);
        unsigned x0 = sx >> 8;
        unsigned y0 = sy >> 8;
        unsigned x1 = x0 + 1 < width ? x0 + 1 : x0;
        unsigned y1 = y0 + 1 < height ? y0 + 1 : y0;
        uint32_t fx = sx & 255;
        uint32_t fy = sy & 255;
        uint32_t result = 0;
        for (unsigned shift = 0; shift < 32; shift += 8) {
            uint32_t top    = ((src[(size_t) y0 * pitch + x0] >> shift) & 255) * (256 - fx)
                            + ((src[(size_t) y0 * pitch + x1] >> shift) & 255) * fx;
            uint32_t bottom = ((src[(size_t) y1 * pitch + x0] >> shift) & 255) * (256 - fx)
                            + ((src[(size_t) y1 * pitch + x1] >> shift) & 255) * fx;
            result |= ((top * (256 - fy) + bottom * fy + 0x8000) >> 16) << shift;
        }

        return result;
    }

    // The texel under the center of the pixel
    return src[(size_t) ((2 * (uint64_t) y + 1) * height / (2 * (uint64_t) dst_height)) * pitch
               + (2 * (uint64_t) x + 1) * width / (2 * (uint64_t) dst_width)];
}

static const char *
mode_name(mfb_stretch_mode mode) {
    return mode == MFB_STRETCH_BILINEAR ? "bilinear" : "nearest";
}

static int
check_scale(unsigned width, unsigned height, unsigned dst_width, unsigned dst_height, bool flat, mfb_stretch_mode mode) {
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

    for (size_t i = 0; i < src_count; ++i) {
        src[i] = flat ? 0xffffffff : (uint32_t) (i * 0x5d3b1729u);
    }

    for (size_t i = 0; i < dst_count; ++i) {
        dst[i] = 0x12345678;
    }

    stretch_image(mode, src, 1, 1, width, height, src_pitch, dst, 2, 1, dst_width, dst_height, dst_pitch);
    int failures = 0;
    for (unsigned y = 0; y < dst_height + 2; ++y) {
        for (unsigned x = 0; x < dst_pitch; ++x) {
            uint32_t actual = dst[(size_t) y * dst_pitch + x];
            if (y >= 1 && y <= dst_height && x >= 2 && x < dst_width + 2) {
                uint32_t expected = flat == true ? 0xffffffff :
                    expected_pixel(src + src_pitch + 1, width, height, src_pitch,
                                   x - 2, y - 1, dst_width, dst_height, mode);
                if (actual != expected) {
                    ++failures;
                }
            }
            else if (actual != 0x12345678) {
                ++failures;
            }
        }
    }

    if (failures != 0) {
        fprintf(stderr,
                "Scaling %ux%u to %ux%u (%s): %d mismatches\n",
                width,
                height,
                dst_width,
                dst_height,
                mode_name(mode),
                failures);
    }

    free(src);
    free(dst);
    return failures;
}

// One cache through calls with different sizes and filters must give what a call with no cache before it gives
static int
check_cache(void) {
    static const unsigned sizes[][4] = {
        { 10, 8, 30, 24 }, { 10, 8, 17, 13 }, { 7, 8, 17, 13 }, { 10, 8, 30, 24 }, { 40, 30, 9, 7 },
    };
    uint32_t src[40 * 30];
    uint32_t cached[30 * 24];
    uint32_t fresh[30 * 24];
    for (unsigned i = 0; i < 40 * 30; ++i) {
        src[i] = i * 0x5d3b1729u;
    }

    SStretchCache cache;
    memset(&cache, 0, sizeof(cache));
    int failures = 0;
    for (unsigned i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i) {
        const unsigned *size = sizes[i];
        for (unsigned pass = 0; pass < 6; ++pass) {
            mfb_stretch_mode mode = (pass & 1) == 0 ? MFB_STRETCH_BILINEAR : MFB_STRETCH_NEAREST;
            stretch_image_cached(&cache, mode, src, 0, 0, size[0], size[1], size[0], cached, 0, 0, size[2], size[3], size[2]);
            stretch_image(mode, src, 0, 0, size[0], size[1], size[0], fresh, 0, 0, size[2], size[3], size[2]);
            if (memcmp(cached, fresh, (size_t) size[2] * size[3] * sizeof(uint32_t)) != 0) {
                fprintf(stderr,
                        "Cached scaling %ux%u to %ux%u (%s) differs\n",
                        size[0],
                        size[1],
                        size[2],
                        size[3],
                        mode_name(mode));
                ++failures;
            }
        }
    }

    stretch_cache_free(&cache);

    return failures;
}

int
main(void) {
#if defined(MINIFB_USE_MMGR)
    m_setLogFiles(MINIFB_MMGR_MEMORY_LOG, MINIFB_MMGR_LEAKS_LOG);
#endif

    int failures = 0;
    for (mfb_stretch_mode mode = MFB_STRETCH_NEAREST; mode <= MFB_STRETCH_BILINEAR; ++mode) {
        failures += check_scale(2, 2, 4, 4, false, mode);
        failures += check_scale(3, 2, 5, 7, false, mode);
        failures += check_scale(8, 6, 3, 2, false, mode);
        failures += check_scale(1, 1, 9, 7, false, mode);
        failures += check_scale(3, 5, 3, 5, false, mode);
        failures += check_scale(2, 2, 5, 5, true, mode);
        failures += check_scale(65536, 1, 2, 1, false, mode);
        failures += check_scale(1, 65536, 1, 2, false, mode);
        failures += check_scale(16, 9, 48, 27, false, mode);
        failures += check_scale(13, 7, 26, 14, false, mode);
        failures += check_scale(11, 5, 44, 20, false, mode);
        failures += check_scale(37, 23, 100, 61, false, mode);
        failures += check_scale(100, 61, 37, 23, false, mode);

        uint32_t seed = 12345;
        for (unsigned i = 0; i < 300; ++i) {
            unsigned size[4];
            for (unsigned j = 0; j < 4; ++j) {
                seed = seed * 1103515245u + 12345u;
                size[j] = 1 + (seed >> 16) % 70;
            }
            failures += check_scale(size[0], size[1], size[2], size[3], false, mode);
        }
    }

    failures += check_cache();

    uint32_t pixel = 0x12345678;
    uint32_t result = 0xffffffff;
    stretch_image(MFB_STRETCH_NEAREST, &pixel, 0, 0, 0, 1, 1, &result, 0, 0, 1, 1, 1);
    if (result != 0xffffffff) {
        ++failures;
    }

    stretch_image(MFB_STRETCH_NEAREST, &pixel, 0, 0, 1, 1, 1, &result, 0, 0, 0, 1, 1);
    if (result != 0xffffffff) {
        ++failures;
    }

    SWindowData window_data = {0};
    window_data.stretch_mode = MFB_STRETCH_BILINEAR;
    window_data.draw_buffer = &pixel;
    window_data.buffer_width = window_data.buffer_height = 1;
    window_data.buffer_stride = 4;
    if (mfb_get_scaled_buffer(&window_data, 1, 1) != &pixel) {
        ++failures;
    }

    uint32_t *buffer = mfb_get_scaled_buffer(&window_data, 4, 4);
    if (buffer == NULL) {
        ++failures;
    }
    else {
        for (unsigned i = 0; i < 16; ++i) {
            if (buffer[i] != pixel) {
                ++failures;
            }
        }

        size_t capacity = window_data.scaled_buffer_size;
        if (mfb_get_scaled_buffer(&window_data, 2, 3) != buffer) {
            ++failures;
        }

        if (window_data.scaled_buffer_size != capacity) {
            ++failures;
        }

        if (mfb_store_title(&window_data, "Scaling test") == NULL) {
            ++failures;
        }

        if (window_data.scaled_buffer != buffer || buffer[0] != pixel) {
            ++failures;
        }

        if (mfb_get_scaled_buffer(&window_data, UINT32_MAX, 1) != NULL) {
            ++failures;
        }

        if (window_data.scaled_buffer != buffer || window_data.scaled_buffer_size != capacity) {
            ++failures;
        }
    }

    free(window_data.scaled_buffer);
    free(window_data.safe_title);
    stretch_cache_free(&window_data.stretch_cache);

    if (failures != 0) {
        return EXIT_FAILURE;
    }

    puts("Scaling checks passed.");

    return EXIT_SUCCESS;
}
