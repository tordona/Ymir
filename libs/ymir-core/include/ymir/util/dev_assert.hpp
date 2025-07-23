#pragma once

/**
@file
@brief Development-time assertions.

Defines the `util::dev_assert(bool)` function which checks for a precondition, breaking into the debugger if it fails.
Useful to check for unexpected or unimplemented cases.

Development assertions must be enabled by defining the `Ymir_DEV_ASSERTIONS` macro with a truthy value.
*/
#include "inline.hpp"

namespace util {

/// @brief Performs a development-time assertion, breaking into the debugger if the condition fails.
/// @param[in] condition the condition to check
FORCE_INLINE void dev_assert(bool condition) {
#if Ymir_DEV_ASSERTIONS
    if (!condition) {
        __debugbreak();
    }
#endif
}

} // namespace util
