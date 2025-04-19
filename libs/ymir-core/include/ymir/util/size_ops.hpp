#pragma once

inline constexpr unsigned long long operator""_KiB(unsigned long long sz) {
    return sz * 1024;
}

inline constexpr unsigned long long operator""_MiB(unsigned long long sz) {
    return sz * 1024 * 1024;
}

inline constexpr unsigned long long operator""_GiB(unsigned long long sz) {
    return sz * 1024 * 1024 * 1024;
}

inline constexpr unsigned long long operator""_TiB(unsigned long long sz) {
    return sz * 1024 * 1024 * 1024 * 1024;
}

namespace util {

inline constexpr double ToKiB(unsigned long long sz) {
    return sz / 1024.0;
}

inline constexpr double ToMiB(unsigned long long sz) {
    return sz / 1024.0 / 1024.0;
}

inline constexpr double ToGiB(unsigned long long sz) {
    return sz / 1024.0 / 1024.0 / 1024.0;
}

inline constexpr double ToTiB(unsigned long long sz) {
    return sz / 1024.0 / 1024.0 / 1024.0 / 1024.0;
}

} // namespace util
