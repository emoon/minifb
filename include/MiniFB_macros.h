#pragma once

//-------------------------------------
// cross-platform deprecation macro, try to use the clean [[deprecated]] if it's avalible, if not, use compiler-specific fallbacks
//-------------------------------------

// C++ [[deprecated]] attribute
#if !defined(__MFB_DEPRECATED) && defined(__has_cpp_attribute)
    #if __has_cpp_attribute(deprecated)
        #define __MFB_DEPRECATED(msg) [[deprecated(msg)]]
    #endif
#endif

// C23 [[deprecated]] attribute
#if !defined(__MFB_DEPRECATED) && defined(__has_c_attribute)
    #if __has_c_attribute(deprecated)
        #define __MFB_DEPRECATED(msg) [[deprecated(msg)]]
    #endif
#endif

// gcc/clang __attribute__ method
#if !defined(__MFB_DEPRECATED) && (defined(__GNUC__) || defined(__clang__))
    #define __MFB_DEPRECATED(msg) __attribute__((deprecated(msg)))
#endif

// msvc __declspec method
#if !defined(__MFB_DEPRECATED) && defined(_MSC_VER)
    #define __MFB_DEPRECATED(msg) __declspec(deprecated(msg))
#endif

// if we can't use any of those, just don't bother
#if !defined(__MFB_DEPRECATED)
    #define __MFB_DEPRECATED(msg)
#endif

// Enumerator deprecation macro.
// Note: __declspec(deprecated) is intentionally not used here because it is
// not portable for enum constants in C mode across compilers.
#if !defined(__MFB_ENUM_DEPRECATED) && defined(__has_cpp_attribute)
    #if __has_cpp_attribute(deprecated)
        #define __MFB_ENUM_DEPRECATED(msg) [[deprecated(msg)]]
    #endif
#endif

#if !defined(__MFB_ENUM_DEPRECATED) && defined(__has_c_attribute)
    #if __has_c_attribute(deprecated)
        #define __MFB_ENUM_DEPRECATED(msg) [[deprecated(msg)]]
    #endif
#endif

#if !defined(__MFB_ENUM_DEPRECATED) && (defined(__GNUC__) || defined(__clang__))
    #define __MFB_ENUM_DEPRECATED(msg) __attribute__((deprecated(msg)))
#endif

#if !defined(__MFB_ENUM_DEPRECATED)
    #define __MFB_ENUM_DEPRECATED(msg)
#endif

//-------------------------------------
#if !defined(__ANDROID__)
    #define MFB_RGB(r, g, b)                                 (((uint32_t) r) << 16) | (((uint32_t) g) << 8) | ((uint32_t) b)
    #define MFB_ARGB(a, r, g, b)    (((uint32_t) a) << 24) | (((uint32_t) r) << 16) | (((uint32_t) g) << 8) | ((uint32_t) b)
#else
    #if defined(HOST_WORDS_BIGENDIAN)
        #define MFB_RGB(r, g, b)                                 (((uint32_t) r) << 16) | (((uint32_t) g) << 8) | ((uint32_t) b)
        #define MFB_ARGB(a, r, g, b)    (((uint32_t) a) << 24) | (((uint32_t) r) << 16) | (((uint32_t) g) << 8) | ((uint32_t) b)
    #else
        #define MFB_RGB(r, g, b)                                 (((uint32_t) b) << 16) | (((uint32_t) g) << 8) | ((uint32_t) r)
        #define MFB_ARGB(a, r, g, b)    (((uint32_t) a) << 24) | (((uint32_t) b) << 16) | (((uint32_t) g) << 8) | ((uint32_t) r)
    #endif
#endif
