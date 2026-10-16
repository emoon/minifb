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
            MFB_LOG(MFB_LOG_ERROR, "Cannot allocate %zu bytes for scaling.", total_bytes);
            return NULL;
        }
        window_data->scaled_buffer = buffer;
        window_data->scaled_buffer_size = total_bytes;
    }

    stretch_image_cached(&window_data->stretch_cache, window_data->stretch_mode,
                         window_data->draw_buffer, 0, 0,
                         window_data->buffer_width, window_data->buffer_height, window_data->buffer_stride / 4,
                         window_data->scaled_buffer, 0, 0, width, height, width);
    return window_data->scaled_buffer;
}

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
