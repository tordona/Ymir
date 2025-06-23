#pragma once

/**
@file
@brief Ymir library version definitions.
*/

#if Ymir_DEV_BUILD
    #define Ymir_FULL_VERSION Ymir_VERSION "-dev"
#else
    #define Ymir_FULL_VERSION Ymir_VERSION
#endif

namespace ymir::version {

/// @brief The library version string in the format "<major>.<minor>.<patch>[-<prerelease>][+<build>]".
///
/// The string follows the Semantic Versioning system, with mandatory major, minor and patch numbers and optional
/// prerelease and build components.
inline constexpr auto string = Ymir_VERSION;

/// @brief The library version string in the format "<major>.<minor>.<patch>[-<prerelease>][+<build>][-dev]".
///
/// The string mostly follows the Semantic Versioning system, with mandatory major, minor and patch numbers and optional
/// prerelease and build components. It also adds a `-dev` suffix for development builds.
inline constexpr auto fullstring = Ymir_FULL_VERSION;

inline constexpr auto major = static_cast<unsigned>(Ymir_VERSION_MAJOR); ///< The library's major version
inline constexpr auto minor = static_cast<unsigned>(Ymir_VERSION_MINOR); ///< The library's minor version
inline constexpr auto patch = static_cast<unsigned>(Ymir_VERSION_PATCH); ///< The library's patch version
inline constexpr auto prerelease = Ymir_VERSION_PRERELEASE;              ///< The library's prerelease version
inline constexpr auto build = Ymir_VERSION_BUILD;                        ///< The library's build version

} // namespace ymir::version
