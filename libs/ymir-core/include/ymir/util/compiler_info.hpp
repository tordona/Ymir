/*
Contains constexpr constants specifying information about the compiler.
Currently supports:
- Clang on Linux and Windows
- GCC on Linux, macOS and MinGW
- MSVC on Windows
 */
#pragma once

#include <string>

namespace compiler {

#if defined(__clang__)

inline constexpr const char *name = "Clang";

namespace version {
    inline constexpr int major = __clang_major__;
    inline constexpr int minor = __clang_minor__;
    inline constexpr int patch = __clang_patchlevel__;
    inline const std::string string = std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
} // namespace version

#elif defined(__GNUC__)

inline constexpr const char *name = "GCC";

namespace version {
    inline constexpr int major = __GNUC__;
    inline constexpr int minor = __GNUC_MINOR__;
    inline constexpr int patch = __GNUC_PATCHLEVEL__;
    inline const std::string string = std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
} // namespace version

#elif defined(_MSC_VER)

inline constexpr const char *name = "MSVC";

namespace version {
    inline constexpr int major = _MSC_VER / 100;
    inline constexpr int minor = _MSC_VER % 100;
    #if defined(_MSC_FULL_VER)
    inline constexpr int patch = _MSC_FULL_VER % 100000;
    inline const std::string string = std::to_string(major) + "." + std::to_string(minor) + "." +
                                      std::to_string(patch) + "." + std::to_string(_MSC_BUILD);
    #else
    inline constexpr int patch = 0;
    inline const std::string string = std::to_string(major) + "." + std::to_string(minor);
    #endif
} // namespace version

#else

inline constexpr const char *name = "<unidentified compiler>";

namespace version {
    inline constexpr int major = 0;
    inline constexpr int minor = 0;
    inline constexpr int patch = 0;
    inline const std::string string = "";
} // namespace version

#endif

} // namespace compiler
