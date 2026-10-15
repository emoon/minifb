#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

// The tables that stretch_image_cached keeps between calls. They only depend on the sizes, so they are rebuilt when a
// size changes, and the memory only grows. Zero it before the first use, and release it with stretch_cache_free
//-------------------------------------
typedef struct {
    void        *memory;
    size_t      memory_size;
    uint32_t    src_width;          // The widths the column table was built for
    uint32_t    dst_width;
    bool        columns_valid;
    bool        allocation_failed;  // Logged once, until an allocation works again
} SStretchCache;

// stretch_image with tables that last between calls
void    stretch_image_cached(SStretchCache *cache,
                             uint32_t *src_image, uint32_t src_x, uint32_t src_y,
                             uint32_t src_width, uint32_t src_height, uint32_t src_pitch,
                             uint32_t *dst_image, uint32_t dst_x, uint32_t dst_y,
                             uint32_t dst_width, uint32_t dst_height, uint32_t dst_pitch);

void    stretch_cache_free(SStretchCache *cache);

#if defined(__cplusplus)
}
#endif
