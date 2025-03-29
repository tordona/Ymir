#pragma once

#include <cstddef>

inline constexpr std::size_t operator""_KiB(std::size_t sz) {
    return sz * 1024;
}

inline constexpr std::size_t operator""_MiB(std::size_t sz) {
    return sz * 1024 * 1024;
}

inline constexpr std::size_t operator""_GiB(std::size_t sz) {
    return sz * 1024 * 1024 * 1024;
}

inline constexpr std::size_t operator""_TiB(std::size_t sz) {
    return sz * 1024 * 1024 * 1024 * 1024;
}

namespace util {

inline constexpr double ToKiB(std::size_t sz) {
    return sz / 1024.0;
}

inline constexpr double ToMiB(std::size_t sz) {
    return sz / 1024.0 / 1024.0;
}

inline constexpr double ToGiB(std::size_t sz) {
    return sz / 1024.0 / 1024.0 / 1024.0;
}

inline constexpr double ToTiB(std::size_t sz) {
    return sz / 1024.0 / 1024.0 / 1024.0 / 1024.0;
}

} // namespace util
