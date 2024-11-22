#pragma once

#if !defined(NDEBUG)
    #define ALWAYS_INLINE inline
    #define ALWAYS_INLINE_LAMBDA
#elif (defined(__GNUC__) || defined(__GNUG__) || defined(__clang__))
    #define ALWAYS_INLINE inline __attribute__((__always_inline__))
    #define ALWAYS_INLINE_LAMBDA __attribute__((__always_inline__))
#elif defined(_MSC_VER)
    #define ALWAYS_INLINE __forceinline
    #define ALWAYS_INLINE_LAMBDA __forceinline
#else
    #define ALWAYS_INLINE inline
    #define ALWAYS_INLINE_LAMBDA
#endif
