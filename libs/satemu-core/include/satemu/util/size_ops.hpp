#pragma once

#include <cstddef>

inline constexpr std::size_t operator""_KiB(std::size_t sz) {
    return sz * 1024;
}

inline constexpr std::size_t operator""_MiB(std::size_t sz) {
    return sz * 1024 * 1024;
}
