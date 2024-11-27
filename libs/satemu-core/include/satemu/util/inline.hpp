#pragma once

#if !defined(NDEBUG)
    #define FORCE_INLINE inline
    #define FORCE_INLINE_LAMBDA
    #define NO_INLINE
#elif (defined(__GNUC__) || defined(__GNUG__) || defined(__clang__))
    #define FORCE_INLINE inline __attribute__((__always_inline__))
    #define FORCE_INLINE_LAMBDA __attribute__((__always_inline__))
    #define NO_INLINE __attribute__((noinline))
#elif defined(_MSC_VER)
    #define FORCE_INLINE __forceinline
    #define FORCE_INLINE_LAMBDA __forceinline
    #define NO_INLINE __declspec(noinline)
#else
    #define FORCE_INLINE inline
    #define FORCE_INLINE_LAMBDA
    #define NO_INLINE
#endif
