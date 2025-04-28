#pragma once

/**
@file
@brief Macros for managing function inlining and flattening.

In Debug builds, these macros have no effect in order to not disrupt the debugging experience.

The macros use appropriate attributes for MSVC, Clang and GCC. For any other compiler, these macros do nothing.
*/

/**
@def FORCE_INLINE
@brief Forces function inlining and marks the function `inline`.
*/

/**
@def NO_INLINE
@brief Prevents function inlining.
*/

/**
@def FLATTEN
@brief Flattens the function.

Essentially inlines all functions called by the flattened function.
*/

#if !defined(NDEBUG) || defined(DISABLE_FORCE_INLINE)
    #define FORCE_INLINE inline
    #define NO_INLINE
    #define FLATTEN
#elif (defined(__GNUC__) || defined(__GNUG__) || defined(__clang__))
    #define FORCE_INLINE [[gnu::always_inline]] inline
    #define NO_INLINE [[gnu::noinline]]
    #define FLATTEN [[gnu::flatten]]
#elif defined(_MSC_VER)
    #define FORCE_INLINE [[msvc::forceinline]] inline
    #define NO_INLINE [[msvc::noinline]]
    #define FLATTEN [[msvc::flatten]]
#else
    #define FORCE_INLINE inline
    #define NO_INLINE
    #define FLATTEN
#endif
