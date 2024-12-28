#pragma once

#if !defined(NDEBUG) || defined(DISABLE_FORCE_INLINE)
    #define FORCE_INLINE inline
    #define NO_INLINE
    #define FLATTEN
#elif (defined(__GNUC__) || defined(__GNUG__) || defined(__clang__))
    #define FORCE_INLINE [[gnu::always_inline]] inline
    #define NO_INLINE [[gnu::noinline]]
    #define FLATTEN [[gnu::flatten]]
#elif defined(_MSC_VER)
    #define FORCE_INLINE [[msvc::forceinline]]
    #define NO_INLINE [[msvc::noinline]]
    #define FLATTEN [[msvc::flatten]]
#else
    #define FORCE_INLINE inline
    #define NO_INLINE
    #define FLATTEN
#endif
