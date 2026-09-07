#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uintptr_t window_id;
    uint8_t  *swizzle_buffer;
    size_t    swizzle_buffer_size;
    struct mfb_timer *timer;
} SWindowData_Web;
