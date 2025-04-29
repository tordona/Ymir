#pragma once

/**
@file
@brief Ymir library version definitions.
*/

namespace ymir::version {

inline constexpr auto string = Ymir_VERSION; ///< The library version string in the format "<major>.<minor>.<patch>"
inline constexpr auto major = Ymir_VERSION_MAJOR; ///< The major version of the library
inline constexpr auto minor = Ymir_VERSION_MINOR; ///< The minor version of the library
inline constexpr auto patch = Ymir_VERSION_PATCH; ///< The patch version of the library

} // namespace ymir::version
