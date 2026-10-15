#include "MiniFB_cpu_x86.h"
#include "MiniFB_internal.h"

#include <stdint.h>

#if defined(_M_X64) || defined(__x86_64__)

#if defined(_MSC_VER)
    #include <intrin.h>
#else
    #include <cpuid.h>
#endif

//-------------------------------------
typedef struct {
    uint32_t    eax;
    uint32_t    ebx;
    uint32_t    ecx;
    uint32_t    edx;
} cpuid_registers;

// The registers that cpuid returns for leaf and subleaf, or 0 in all of them if the processor does not have that leaf
//-------------------------------------
static cpuid_registers
cpuid(uint32_t leaf, uint32_t subleaf) {
    cpuid_registers result = { 0, 0, 0, 0 };
#if defined(_MSC_VER)
    int registers[4];   // eax, ebx, ecx, edx
    __cpuid(registers, (int) (leaf & 0x80000000u));
    if ((uint32_t) registers[0] < leaf) {
        return result;
    }
    __cpuidex(registers, (int) leaf, (int) subleaf);
    result.eax = (uint32_t) registers[0];
    result.ebx = (uint32_t) registers[1];
    result.ecx = (uint32_t) registers[2];
    result.edx = (uint32_t) registers[3];
#else
    unsigned eax, ebx, ecx, edx;
    if (__get_cpuid_count(leaf, subleaf, &eax, &ebx, &ecx, &edx) != 0) {
        result.eax = eax;
        result.ebx = ebx;
        result.ecx = ecx;
        result.edx = edx;
    }
#endif

    return result;
}

// The features of x86-64-v2 beyond those of every x86-64 processor, as the x86-64 psABI lists them. The compiler may
// use any of them in code built for that level, not only the instructions that the scaling asks for
//-------------------------------------
static bool
processor_has_x64v2(void) {
    const uint32_t sse3       = 1u << 0;    // In ecx of leaf 1
    const uint32_t ssse3      = 1u << 9;
    const uint32_t cmpxchg16b = 1u << 13;
    const uint32_t sse41      = 1u << 19;
    const uint32_t sse42      = 1u << 20;
    const uint32_t popcnt     = 1u << 23;
    const uint32_t lahf_sahf  = 1u << 0;    // In ecx of leaf 0x80000001
    const uint32_t leaf1      = sse3 | ssse3 | cmpxchg16b | sse41 | sse42 | popcnt;

    return (cpuid(1, 0).ecx & leaf1) == leaf1 && (cpuid(0x80000001u, 0).ecx & lahf_sahf) != 0;
}

// XCR0, the register that says which registers the operating system saves when it switches threads
//-------------------------------------
static uint64_t
read_xcr0(void) {
#if defined(_MSC_VER)
    return _xgetbv(0);
#else
    // The intrinsic needs the XSAVE target, which the rest of the file does not have
    uint32_t low, high;
    __asm__ volatile ("xgetbv" : "=a"(low), "=d"(high) : "c"(0));
    return ((uint64_t) high << 32) | low;
#endif
}

// The features that x86-64-v3 adds to x86-64-v2, as the x86-64 psABI lists them, and an operating system that saves
// the registers of 256 bits. Without that, AVX instructions fault even on a processor that has them
//-------------------------------------
static bool
processor_has_x64v3(void) {
    const uint32_t fma     = 1u << 12;  // In ecx of leaf 1
    const uint32_t movbe   = 1u << 22;
    const uint32_t osxsave = 1u << 27;
    const uint32_t avx     = 1u << 28;
    const uint32_t f16c    = 1u << 29;
    const uint32_t bmi1    = 1u << 3;   // In ebx of leaf 7, subleaf 0
    const uint32_t avx2    = 1u << 5;
    const uint32_t bmi2    = 1u << 8;
    const uint32_t lzcnt   = 1u << 5;   // In ecx of leaf 0x80000001
    const uint32_t xmm_ymm = 0x6;       // In XCR0: the state of the xmm registers and of the upper halves
    const uint32_t leaf1   = fma | movbe | osxsave | avx | f16c;
    const uint32_t leaf7   = bmi1 | avx2 | bmi2;

    if (processor_has_x64v2() == false || (cpuid(1, 0).ecx & leaf1) != leaf1) {
        return false;
    }

    // xgetbv only exists when OSXSAVE is set, which the test above checked
    return (cpuid(7, 0).ebx & leaf7) == leaf7 && (cpuid(0x80000001u, 0).ecx & lzcnt) != 0 && (read_xcr0() & xmm_ymm) == xmm_ymm;
}

// Computed once. Two threads that ask at the same time compute the same value, so the race is harmless
//-------------------------------------
bool
mfb_cpu_has_x64v3(void) {
    static int has = -1;
    if (has < 0) {
        has = processor_has_x64v3() ? 1 : 0;
    }

    return has == 1;
}

//-------------------------------------
bool
mfb_cpu_has_build_level(void) {
#if defined(MINIFB_USE_X86_64_V3)
    if (mfb_cpu_has_x64v3() == false) {
        MFB_LOG(MFB_LOG_ERROR, "MiniFB was built for x86-64-v3 (MINIFB_USE_X86_64_V3), which this processor does not have.");
        return false;
    }
#elif defined(MINIFB_USE_X86_64_V2)
    if (processor_has_x64v2() == false) {
        MFB_LOG(MFB_LOG_ERROR, "MiniFB was built for x86-64-v2 (MINIFB_USE_X86_64_V2), which this processor does not have.");
        return false;
    }
#endif

    return true;
}

#else

//-------------------------------------
bool
mfb_cpu_has_x64v3(void) {
    return false;
}

//-------------------------------------
bool
mfb_cpu_has_build_level(void) {
    return true;
}

#endif
