#include "MiniFB_internal.h"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

#if defined(MINIFB_USE_MMGR)
    #include "mmgr.h"
#endif

//-------------------------------------
bool
calculate_buffer_layout(uint32_t width, uint32_t height, uint32_t *stride_out, size_t *total_bytes_out) {
    if (width == 0 || height == 0) {
        return false;
    }

    if (width > UINT32_MAX / 4u) {
        return false;
    }

    uint32_t stride = width * 4u;
    if ((size_t) height > SIZE_MAX / (size_t) stride) {
        return false;
    }

    if (stride_out != NULL) {
        *stride_out = stride;
    }

    if (total_bytes_out != NULL) {
        *total_bytes_out = (size_t) stride * (size_t) height;
    }

    return true;
}

//-------------------------------------
bool
mfb_validate_viewport(const SWindowData *window_data,
                      unsigned offset_x, unsigned offset_y,
                      unsigned width, unsigned height,
                      const char *backend_name) {
    const char *name = (backend_name != NULL) ? backend_name : "MiniFB";

    if (window_data == NULL) {
        MFB_LOG(MFB_LOG_ERROR, "%s: mfb_set_viewport called with a null window pointer.", name);
        return false;
    }

    if (width == 0 || height == 0) {
        MFB_LOG(MFB_LOG_ERROR, "%s: mfb_set_viewport called with zero viewport size (width=%u, height=%u).",
                name, width, height);
        return false;
    }

    if (window_data->window_width == 0 || window_data->window_height == 0) {
        MFB_LOG(MFB_LOG_ERROR, "%s: mfb_set_viewport called with invalid window dimensions %ux%u.",
                name, window_data->window_width, window_data->window_height);
        return false;
    }

    if (offset_x > window_data->window_width || width > window_data->window_width - offset_x) {
        MFB_LOG(MFB_LOG_ERROR, "%s: viewport exceeds window width (offset_x=%u, width=%u, window_width=%u).",
                name, offset_x, width, window_data->window_width);
        return false;
    }

    if (offset_y > window_data->window_height || height > window_data->window_height - offset_y) {
        MFB_LOG(MFB_LOG_ERROR, "%s: viewport exceeds window height (offset_y=%u, height=%u, window_height=%u).",
                name, offset_y, height, window_data->window_height);
        return false;
    }

    return true;
}

//-------------------------------------
#if defined(MINIFB_USE_BILINEAR)
static uint32_t
interpolate(uint32_t *src_image, uint32_t src_row, uint64_t src_offset_x, uint64_t src_offset_y, uint32_t src_width, uint32_t src_height, uint32_t src_pitch) {
    uint32_t src_col = (uint32_t) (src_offset_x >> 16);
    uint32_t inc_x = src_col + 1 < src_width  ? 1         : 0;
    uint32_t inc_y = src_row + 1 < src_height ? src_pitch : 0;
    uint8_t *p00 = (uint8_t *) &src_image[src_col];
    uint8_t *p01 = (uint8_t *) &src_image[src_col + inc_x];
    uint8_t *p10 = (uint8_t *) &src_image[src_col + inc_y];
    uint8_t *p11 = (uint8_t *) &src_image[src_col + inc_y + inc_x];

    uint32_t wx2 = src_offset_x & 0xffff;
    uint32_t wy2 = src_offset_y & 0xffff;
    uint32_t wx1 = 0x10000 - wx2;
    uint32_t wy1 = 0x10000 - wy2;

    // w1 takes the rounding remainder so the four weights always add up to 0x10000, and a flat
    // color keeps its exact value. It also avoids wx1 * wy1, which is 2^32 when both fractions are 0.
    uint32_t w2 = (wx2 * wy1) >> 16;
    uint32_t w3 = (wx1 * wy2) >> 16;
    uint32_t w4 = (wx2 * wy2) >> 16;
    uint32_t w1 = 0x10000 - w2 - w3 - w4;

    uint32_t b = ((p00[0] * w1 + p01[0] * w2) + (p10[0] * w3 + p11[0] * w4)) >> 16;
    uint32_t g = ((p00[1] * w1 + p01[1] * w2) + (p10[1] * w3 + p11[1] * w4)) >> 16;
    uint32_t r = ((p00[2] * w1 + p01[2] * w2) + (p10[2] * w3 + p11[2] * w4)) >> 16;
    uint32_t a = ((p00[3] * w1 + p01[3] * w2) + (p10[3] * w3 + p11[3] * w4)) >> 16;

    return (a << 24) + (r << 16) + (g << 8) + b;
}
#endif

// Only for 32 bits images
//-------------------------------------
void
stretch_image(uint32_t *src_image, uint32_t src_x, uint32_t src_y, uint32_t src_width, uint32_t src_height, uint32_t src_pitch,
              uint32_t *dst_image, uint32_t dst_x, uint32_t dst_y, uint32_t dst_width, uint32_t dst_height, uint32_t dst_pitch) {
    uint32_t    x, y;
    uint64_t    src_offset_x, src_offset_y;

    if (src_image == NULL || dst_image == NULL)
        return;

    if (src_width == 0 || src_height == 0 || dst_width == 0 || dst_height == 0)
        return;

    src_image += src_x + (size_t) src_y * src_pitch;
    dst_image += dst_x + (size_t) dst_y * dst_pitch;

    const uint64_t delta_x = ((uint64_t) src_width  << 16) / dst_width;
    const uint64_t delta_y = ((uint64_t) src_height << 16) / dst_height;

    src_offset_y = 0;
    for (y=0; y<dst_height; ++y) {
        uint64_t sample_y = src_offset_y;
#if defined(MINIFB_USE_BILINEAR)
        // Match GPU sampling at pixel centers and clamp to the edge texels.
        sample_y += delta_y / 2;
        sample_y = sample_y > 0x8000 ? sample_y - 0x8000 : 0;
        if (sample_y > ((uint64_t) (src_height - 1) << 16)) {
            sample_y = (uint64_t) (src_height - 1) << 16;
        }
#endif
        uint32_t src_row = (uint32_t) (sample_y >> 16);
        uint32_t *src_line = src_image + (size_t) src_row * src_pitch;
        src_offset_x = 0;
        for (x=0; x<dst_width; ++x) {
#if defined(MINIFB_USE_BILINEAR)
            uint64_t sample_x = src_offset_x + delta_x / 2;
            sample_x = sample_x > 0x8000 ? sample_x - 0x8000 : 0;
            if (sample_x > ((uint64_t) (src_width - 1) << 16)) {
                sample_x = (uint64_t) (src_width - 1) << 16;
            }
            dst_image[x] = interpolate(src_line, src_row, sample_x, sample_y, src_width, src_height, src_pitch);
#else
            dst_image[x] = src_line[src_offset_x >> 16];
#endif
            src_offset_x += delta_x;
        }

        src_offset_y += delta_y;
        dst_image += dst_pitch;
    }
}

#if defined(MINIFB_USE_BILINEAR)
//-------------------------------------
uint32_t *
mfb_get_scaled_buffer(SWindowData *window_data, uint32_t width, uint32_t height) {
    size_t total_bytes;
    if (window_data == NULL || window_data->draw_buffer == NULL ||
        window_data->buffer_width == 0 || window_data->buffer_height == 0 ||
        calculate_buffer_layout(width, height, NULL, &total_bytes) == false) {
        return NULL;
    }

    if (width == window_data->buffer_width && height == window_data->buffer_height) {
        return window_data->draw_buffer;
    }

    if (total_bytes > window_data->scaled_buffer_size) {
        uint32_t *buffer = (uint32_t *) realloc(window_data->scaled_buffer, total_bytes);
        if (buffer == NULL) {
            MFB_LOG(MFB_LOG_ERROR, "Cannot allocate %zu bytes for bilinear scaling.", total_bytes);
            return NULL;
        }
        window_data->scaled_buffer = buffer;
        window_data->scaled_buffer_size = total_bytes;
    }

    stretch_image(window_data->draw_buffer, 0, 0,
                  window_data->buffer_width, window_data->buffer_height, window_data->buffer_stride / 4,
                  window_data->scaled_buffer, 0, 0, width, height, width);
    return window_data->scaled_buffer;
}
#endif

//-------------------------------------
void
calc_dst_factor(SWindowData *window_data, uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) {
        return;
    }

    if (window_data->dst_width == 0) {
        window_data->dst_width = width;
    }
    if (window_data->dst_height == 0) {
        window_data->dst_height = height;
    }

    float inv_width  = 1.0f / (float) width;
    float inv_height = 1.0f / (float) height;

    window_data->factor_x     = (float) window_data->dst_offset_x * inv_width;
    window_data->factor_width = (float) window_data->dst_width    * inv_width;

    window_data->factor_y      = (float) window_data->dst_offset_y * inv_height;
    window_data->factor_height = (float) window_data->dst_height   * inv_height;
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
