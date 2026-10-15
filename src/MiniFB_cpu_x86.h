#pragma once

#include <stdbool.h>

#if defined(__cplusplus)
extern "C" {
#endif

// Whether this processor has x86-64-v3, which includes AVX2, and the operating system saves its registers. Always
// false outside 64-bit x86
bool    mfb_cpu_has_x64v3(void);

// Whether this processor has the level that the whole library was built for (MINIFB_USE_X86_64_V2 or MINIFB_USE_X86_64_V3),
// with an error in the log when it does not. Always true for the default level and outside 64-bit x86
bool    mfb_cpu_has_build_level(void);

#if defined(__cplusplus)
}
#endif
