#include "MiniFB_stretch.h"
#include "MiniFB_stretch_internal.h"
#include "MiniFB_internal.h"
#include "MiniFB_cpu_x86.h"

#include <stdlib.h>
#include <string.h>

#if defined(MINIFB_USE_MMGR)
    #include "mmgr.h"
#endif

// docs/scaling_algorithms.md explains the sampling, the tables, the formulas and the levels

// clang vectorizes the scalar bilinear passes with NEON when they stand alone, but not once they are inlined into the
// loop over the rows, which made the bilinear scaling up to 2.5 times slower on ARM
#if defined(_MSC_VER)
    #define STRETCH_NOINLINE    __declspec(noinline)
#else
    #define STRETCH_NOINLINE    __attribute__((noinline))
#endif

//-------------------------------------
// Memory of the cache
//-------------------------------------

// The cache holds at least size bytes. It grows when a width or the filter changes, so the column table is marked as stale
//-------------------------------------
static bool
reserve_cache(SStretchCache *cache, size_t size) {
    if (size <= cache->memory_size) {
        return true;
    }

    void *memory = realloc(cache->memory, size);
    if (memory == NULL) {
        if (cache->allocation_failed == false) {
            MFB_LOG(MFB_LOG_WARNING, "stretch_image: cannot allocate %zu bytes, so it scales without tables (slower).", size);
            cache->allocation_failed = true;
        }
        return false;
    }

    cache->memory            = memory;
    cache->memory_size       = size;
    cache->columns_valid     = false;
    cache->allocation_failed = false;
    return true;
}

// Whether the column table must be built again for these widths. The cache records them, because the caller builds the
// table right after
//-------------------------------------
static bool
columns_are_stale(SStretchCache *cache, uint32_t src_width, uint32_t dst_width) {
    bool stale = cache->columns_valid == false || cache->src_width != src_width || cache->dst_width != dst_width;
    cache->src_width     = src_width;
    cache->dst_width     = dst_width;
    cache->columns_valid = true;
    return stale;
}

//-------------------------------------
void
stretch_cache_free(SStretchCache *cache) {
    if (cache == NULL) {
        return;
    }

    free(cache->memory);
    memset(cache, 0, sizeof(*cache));
}

//-------------------------------------
// Nearest
//-------------------------------------

// Walks floor((2 * i + 1) * src_size / (2 * dst_size)), the texel under the center of pixel i, for i = 0, 1, 2...
// Every value fits in 32 bits for sizes below 2^30
//-------------------------------------
typedef struct {
    uint32_t    index;
    uint32_t    remainder;
    uint32_t    step_index;
    uint32_t    step_remainder;
    uint32_t    divisor;
} SCenterStep;

//-------------------------------------
static SCenterStep
center_step_start(uint32_t src_size, uint32_t dst_size) {
    SCenterStep step;
    step.divisor        = 2 * dst_size;
    step.index          = src_size / step.divisor;
    step.remainder      = src_size % step.divisor;
    step.step_index     = (2 * src_size) / step.divisor;
    step.step_remainder = (2 * src_size) % step.divisor;
    return step;
}

//-------------------------------------
static void
center_step_next(SCenterStep *step) {
    step->index     += step->step_index;
    step->remainder += step->step_remainder;
    if (step->remainder >= step->divisor) {
        step->remainder -= step->divisor;
        ++step->index;
    }
}

//-------------------------------------
static void
fill_nearest_columns(uint32_t *columns, uint32_t src_width, uint32_t dst_width) {
    SCenterStep column = center_step_start(src_width, dst_width);
    for (uint32_t x = 0; x < dst_width; ++x) {
        columns[x] = column.index;
        center_step_next(&column);
    }
}

//-------------------------------------
static void
repeat_texels(uint32_t *dst_line, const uint32_t *src_line, uint32_t src_width, uint32_t factor) {
    uint32_t x;

    switch (factor) {
        case 1:
            memcpy(dst_line, src_line, (size_t) src_width * sizeof(uint32_t));
            break;

        case 2:
            for (x = 0; x < src_width; ++x) {
                uint32_t texel = src_line[x];
                dst_line[0] = texel;
                dst_line[1] = texel;
                dst_line += 2;
            }
            break;

        case 3:
            for (x = 0; x < src_width; ++x) {
                uint32_t texel = src_line[x];
                dst_line[0] = texel;
                dst_line[1] = texel;
                dst_line[2] = texel;
                dst_line += 3;
            }
            break;

        default:
            for (x = 0; x < src_width; ++x) {
                uint32_t texel = src_line[x];
                dst_line[0] = texel;
                dst_line[1] = texel;
                dst_line[2] = texel;
                dst_line[3] = texel;
                dst_line += 4;
            }
            break;
    }
}

// Compilers do not unroll this loop on their own in the same way, and with a 32-bit index some of them widen x + 1,
// x + 2 and x + 3 to 64 bits on every pixel, so it walks pointers, 4 pixels per turn. All the reads come before the
// writes: the compiler cannot know that the destination and the table do not overlap, so a write between two reads
// makes each pixel wait for the one before it, which on in-order cores (Cortex-A53, A55) cost up to 25 %
//-------------------------------------
static void
take_texels(uint32_t *dst_line, const uint32_t *src_line, const uint32_t *columns, uint32_t width) {
    uint32_t *dst_end = dst_line + width;
    for (; dst_end - dst_line >= 4; dst_line += 4, columns += 4) {
        uint32_t column0 = columns[0];
        uint32_t column1 = columns[1];
        uint32_t column2 = columns[2];
        uint32_t column3 = columns[3];
        uint32_t texel0  = src_line[column0];
        uint32_t texel1  = src_line[column1];
        uint32_t texel2  = src_line[column2];
        uint32_t texel3  = src_line[column3];
        dst_line[0] = texel0;
        dst_line[1] = texel1;
        dst_line[2] = texel2;
        dst_line[3] = texel3;
    }

    for (; dst_line != dst_end; ++dst_line, ++columns) {
        *dst_line = src_line[*columns];
    }
}

// The same pixels as take_texels, for when there is no memory for the table
//-------------------------------------
static void
take_texels_without_table(uint32_t *dst_line, const uint32_t *src_line, uint32_t src_width, uint32_t dst_width) {
    SCenterStep column = center_step_start(src_width, dst_width);
    for (uint32_t x = 0; x < dst_width; ++x) {
        dst_line[x] = src_line[column.index];
        center_step_next(&column);
    }
}

//-------------------------------------
static void
stretch_nearest(SStretchCache *cache,
                const uint32_t *src_image, uint32_t src_width, uint32_t src_height, uint32_t src_pitch,
                uint32_t *dst_image, uint32_t dst_width, uint32_t dst_height, uint32_t dst_pitch) {
    if (src_width >= 0x40000000 || src_height >= 0x40000000 || dst_width >= 0x40000000 || dst_height >= 0x40000000) {
        return;
    }

    uint32_t factor = 0;
    if (dst_width % src_width == 0 && dst_width / src_width <= 4) {
        factor = dst_width / src_width;
    }

    const uint32_t *columns = NULL;
    if (factor == 0 && reserve_cache(cache, (size_t) dst_width * sizeof(uint32_t))) {
        if (columns_are_stale(cache, src_width, dst_width)) {
            fill_nearest_columns((uint32_t *) cache->memory, src_width, dst_width);
        }
        columns = (const uint32_t *) cache->memory;
    }

    SCenterStep row = center_step_start(src_height, dst_height);
    uint32_t previous_src_row = 0;
    for (uint32_t y = 0; y < dst_height; ++y) {
        uint32_t src_row = row.index;
        const uint32_t *src_line = src_image + (size_t) src_row * src_pitch;
        if (y != 0 && src_row == previous_src_row) {
            memcpy(dst_image, dst_image - dst_pitch, (size_t) dst_width * sizeof(uint32_t));
        }
        else if (factor != 0) {
#if defined(MINIFB_STRETCH_SSE2)
            if (factor >= 2 && src_width >= 4) {
                stretch_nearest_repeat_sse2(dst_image, src_line, src_width, factor);
            }
            else
#endif
            {
                repeat_texels(dst_image, src_line, src_width, factor);
            }
        }
        else if (columns == NULL) {
            take_texels_without_table(dst_image, src_line, src_width, dst_width);
        }
        else {
#if defined(MINIFB_STRETCH_SSE2)
            if (dst_width >= 4) {
                stretch_nearest_take_sse2(dst_image, src_line, columns, dst_width);
            }
            else
#endif
            {
                take_texels(dst_image, src_line, columns, dst_width);
            }
        }

        previous_src_row = src_row;
        center_step_next(&row);
        dst_image += dst_pitch;
    }
}

//-------------------------------------
// Bilinear
//-------------------------------------

// Walks round(((2 * i + 1) * src_size - dst_size) * 128 / dst_size), the center of pixel i in 1/256 of a texel,
// clamped to the first and the last texel. It uses 64 bits, but only once per column and once per row
//-------------------------------------
typedef struct {
    int64_t     sample;
    int64_t     remainder;
    int64_t     step_sample;
    int64_t     step_remainder;
    int64_t     divisor;
    int64_t     max_sample;
} SSampleStep;

//-------------------------------------
static SSampleStep
sample_step_start(uint32_t src_size, uint32_t dst_size) {
    SSampleStep step;
    int64_t numerator   = ((int64_t) src_size - dst_size) * 256 + dst_size;
    step.divisor        = 2 * (int64_t) dst_size;
    step.sample         = numerator / step.divisor;
    step.remainder      = numerator % step.divisor;
    if (step.remainder < 0) {
        step.remainder += step.divisor;
        --step.sample;
    }
    step.step_sample    = (512 * (int64_t) src_size) / step.divisor;
    step.step_remainder = (512 * (int64_t) src_size) % step.divisor;
    step.max_sample     = ((int64_t) src_size - 1) * 256;
    return step;
}

//-------------------------------------
static uint32_t
sample_step_clamped(const SSampleStep *step) {
    if (step->sample < 0) {
        return 0;
    }
    if (step->sample > step->max_sample) {
        return (uint32_t) step->max_sample;
    }
    return (uint32_t) step->sample;
}

//-------------------------------------
static void
sample_step_next(SSampleStep *step) {
    step->sample    += step->step_sample;
    step->remainder += step->step_remainder;
    if (step->remainder >= step->divisor) {
        step->remainder -= step->divisor;
        ++step->sample;
    }
}

//-------------------------------------
static SStretchColumn
column_of_sample(uint32_t sample, uint32_t src_width) {
    SStretchColumn column;
    column.col0 = sample >> 8;
    column.col1 = column.col0 + 1 < src_width ? column.col0 + 1 : column.col0;
    column.fx   = sample & 0xff;
    return column;
}

//-------------------------------------
static void
fill_bilinear_columns(SStretchColumn *columns, uint32_t src_width, uint32_t dst_width) {
    SSampleStep step = sample_step_start(src_width, dst_width);
    for (uint32_t x = 0; x < dst_width; ++x) {
        columns[x] = column_of_sample(sample_step_clamped(&step), src_width);
        sample_step_next(&step);
    }
}

// p0 * 256 + (p1 - p0) * fx for blue and red together, and for green and alpha together. Each half of the result ends
// between 0 and 65280, so the sum is exact modulo 2^32 even when a half goes below 0 in between
//-------------------------------------
static void
mix_texels(uint32_t p0, uint32_t p1, uint32_t fx, uint32_t *blue_red, uint32_t *green_alpha) {
    uint32_t br0 = p0 & 0x00ff00ff;
    uint32_t ga0 = (p0 >> 8) & 0x00ff00ff;
    uint32_t br1 = p1 & 0x00ff00ff;
    uint32_t ga1 = (p1 >> 8) & 0x00ff00ff;
    *blue_red    = (br0 << 8) + (br1 - br0) * fx;
    *green_alpha = (ga0 << 8) + (ga1 - ga0) * fx;
}

// fy1 is 256 - fy2. The weights are 16-bit so that the compiler knows they fit, and vectorizes with 16-bit
// multiplications instead of widening every value to 32 bits
//-------------------------------------
static inline uint32_t
mix_rows(uint32_t top, uint32_t bottom, uint16_t fy1, uint16_t fy2) {
    return (top * fy1 + bottom * fy2 + 0x8000) >> 16;
}

// The scalar rows hold blue, red, green, alpha: the two pairs as mix_texels gives them. On a little-endian processor,
// copying each uint32_t leaves the low half first
//-------------------------------------
STRETCH_NOINLINE static void
interpolate_row(uint16_t *row, const uint32_t *src_line, const SStretchColumn *columns, uint32_t width) {
    for (uint32_t x = 0; x < width; ++x) {
        uint32_t blue_red, green_alpha;
        mix_texels(src_line[columns[x].col0], src_line[columns[x].col1], columns[x].fx, &blue_red, &green_alpha);
        memcpy(&row[x * 4 + 0], &blue_red,    sizeof(blue_red));
        memcpy(&row[x * 4 + 2], &green_alpha, sizeof(green_alpha));
    }
}

//-------------------------------------
STRETCH_NOINLINE static void
blend_rows(uint32_t *dst_line, const uint16_t *top, const uint16_t *bottom, uint32_t fy, uint32_t width) {
    const uint16_t fy1 = (uint16_t) (256 - fy);
    const uint16_t fy2 = (uint16_t) fy;
    for (uint32_t x = 0; x < width; ++x) {
        const uint16_t *upper = &top[x * 4];
        const uint16_t *lower = &bottom[x * 4];
        uint32_t b = mix_rows(upper[0], lower[0], fy1, fy2);
        uint32_t r = mix_rows(upper[1], lower[1], fy1, fy2);
        uint32_t g = mix_rows(upper[2], lower[2], fy1, fy2);
        uint32_t a = mix_rows(upper[3], lower[3], fy1, fy2);
        dst_line[x] = (a << 24) + (r << 16) + (g << 8) + b;
    }
}

// The same pixels as the two passes, for when there is no memory for the table and the rows
//-------------------------------------
static void
blend_without_table(uint32_t *dst_line, const uint32_t *src_line0, const uint32_t *src_line1, uint32_t fy,
                    uint32_t src_width, uint32_t dst_width) {
    const uint16_t fy1 = (uint16_t) (256 - fy);
    const uint16_t fy2 = (uint16_t) fy;
    SSampleStep step = sample_step_start(src_width, dst_width);
    for (uint32_t x = 0; x < dst_width; ++x) {
        SStretchColumn column = column_of_sample(sample_step_clamped(&step), src_width);
        uint32_t top_br, top_ga, bottom_br, bottom_ga;
        mix_texels(src_line0[column.col0], src_line0[column.col1], column.fx, &top_br, &top_ga);
        mix_texels(src_line1[column.col0], src_line1[column.col1], column.fx, &bottom_br, &bottom_ga);
        uint32_t b = mix_rows(top_br & 0xffff, bottom_br & 0xffff, fy1, fy2);
        uint32_t r = mix_rows(top_br >> 16,    bottom_br >> 16,    fy1, fy2);
        uint32_t g = mix_rows(top_ga & 0xffff, bottom_ga & 0xffff, fy1, fy2);
        uint32_t a = mix_rows(top_ga >> 16,    bottom_ga >> 16,    fy1, fy2);
        dst_line[x] = (a << 24) + (r << 16) + (g << 8) + b;
        sample_step_next(&step);
    }
}

// The level fills and mixes the rows. Both passes must use the same level, because the channels are in a different
// order in the scalar rows and in the SIMD rows
//-------------------------------------
typedef struct {
    void    (*interpolate_row)(uint16_t *row, const uint32_t *src_line, const SStretchColumn *columns, uint32_t width);
    void    (*blend_rows)(uint32_t *dst_line, const uint16_t *top, const uint16_t *bottom, uint32_t fy, uint32_t width);
} SBilinearLevel;

//-------------------------------------
static SBilinearLevel
choose_bilinear_level(uint32_t dst_width) {
    SBilinearLevel level = { interpolate_row, blend_rows };
#if defined(MINIFB_STRETCH_AVX2)
    if (dst_width >= 8 && mfb_cpu_has_x64v3()) {
        level.interpolate_row = stretch_bilinear_row_avx2;
        level.blend_rows      = stretch_bilinear_blend_avx2;
        return level;
    }
#endif
#if defined(MINIFB_STRETCH_SSE2)
    if (dst_width >= 4) {
        level.interpolate_row = stretch_bilinear_row_sse2;
        level.blend_rows      = stretch_bilinear_blend_sse2;
    }
#endif
#if defined(MINIFB_STRETCH_NEON)
    if (dst_width >= 4) {
        level.interpolate_row = stretch_bilinear_row_neon;
        level.blend_rows      = stretch_bilinear_blend_neon;
    }
#endif
    (void) dst_width;
    return level;
}

//-------------------------------------
static void
stretch_bilinear(SStretchCache *cache,
                 const uint32_t *src_image, uint32_t src_width, uint32_t src_height, uint32_t src_pitch,
                 uint32_t *dst_image, uint32_t dst_width, uint32_t dst_height, uint32_t dst_pitch) {
    // The table, then the two rows of 4 channels of 16 bits
    const size_t column_bytes = sizeof(SStretchColumn) + 2 * 4 * sizeof(uint16_t);

    SStretchColumn *columns = NULL;
    uint16_t *rows[2] = { NULL, NULL };
    if (dst_width <= SIZE_MAX / column_bytes && reserve_cache(cache, (size_t) dst_width * column_bytes)) {
        columns = (SStretchColumn *) cache->memory;
        rows[0] = (uint16_t *) (columns + dst_width);
        rows[1] = rows[0] + (size_t) dst_width * 4;
        if (columns_are_stale(cache, src_width, dst_width)) {
            fill_bilinear_columns(columns, src_width, dst_width);
        }
    }

    const SBilinearLevel level = choose_bilinear_level(dst_width);
    uint32_t row_sources[2] = { UINT32_MAX, UINT32_MAX };

    SSampleStep row = sample_step_start(src_height, dst_height);
    for (uint32_t y = 0; y < dst_height; ++y) {
        uint32_t sample   = sample_step_clamped(&row);
        uint32_t src_row0 = sample >> 8;
        uint32_t src_row1 = src_row0 + 1 < src_height ? src_row0 + 1 : src_row0;
        uint32_t fy       = sample & 0xff;
        const uint32_t *src_line0 = src_image + (size_t) src_row0 * src_pitch;
        const uint32_t *src_line1 = src_image + (size_t) src_row1 * src_pitch;

        if (columns == NULL) {
            blend_without_table(dst_image, src_line0, src_line1, fy, src_width, dst_width);
        }
        else {
            // The source rows only move down, so the bottom row of one destination row is often the top row of the
            // next one
            if (row_sources[0] != src_row0) {
                if (row_sources[1] == src_row0) {
                    uint16_t *swap = rows[0];
                    rows[0]        = rows[1];
                    rows[1]        = swap;
                    row_sources[1] = row_sources[0];
                }
                else {
                    level.interpolate_row(rows[0], src_line0, columns, dst_width);
                }
                row_sources[0] = src_row0;
            }
            if (row_sources[1] != src_row1) {
                level.interpolate_row(rows[1], src_line1, columns, dst_width);
                row_sources[1] = src_row1;
            }
            level.blend_rows(dst_image, rows[0], rows[1], fy, dst_width);
        }

        sample_step_next(&row);
        dst_image += dst_pitch;
    }
}

//-------------------------------------
// Entry points
//-------------------------------------

//-------------------------------------
void
stretch_image_cached(SStretchCache *cache, mfb_stretch_mode mode,
                     uint32_t *src_image, uint32_t src_x, uint32_t src_y,
                     uint32_t src_width, uint32_t src_height, uint32_t src_pitch,
                     uint32_t *dst_image, uint32_t dst_x, uint32_t dst_y,
                     uint32_t dst_width, uint32_t dst_height, uint32_t dst_pitch) {
    if (cache == NULL || src_image == NULL || dst_image == NULL) {
        return;
    }

    if (src_width == 0 || src_height == 0 || dst_width == 0 || dst_height == 0) {
        return;
    }

    src_image += src_x + (size_t) src_y * src_pitch;
    dst_image += dst_x + (size_t) dst_y * dst_pitch;

    if (cache->mode != mode) {
        cache->columns_valid = false;
        cache->mode = mode;
    }

    if (mode == MFB_STRETCH_BILINEAR) {
        stretch_bilinear(cache, src_image, src_width, src_height, src_pitch, dst_image, dst_width, dst_height, dst_pitch);
    }
    else {
        stretch_nearest(cache, src_image, src_width, src_height, src_pitch, dst_image, dst_width, dst_height, dst_pitch);
    }
}

// Without a window there is nowhere to keep the tables, so they last for one call
//-------------------------------------
void
stretch_image(mfb_stretch_mode mode, uint32_t *src_image, uint32_t src_x, uint32_t src_y, uint32_t src_width, uint32_t src_height, uint32_t src_pitch,
              uint32_t *dst_image, uint32_t dst_x, uint32_t dst_y, uint32_t dst_width, uint32_t dst_height, uint32_t dst_pitch) {
    SStretchCache cache;
    memset(&cache, 0, sizeof(cache));
    stretch_image_cached(&cache, mode,
                         src_image, src_x, src_y, src_width, src_height, src_pitch,
                         dst_image, dst_x, dst_y, dst_width, dst_height, dst_pitch);
    stretch_cache_free(&cache);
}
