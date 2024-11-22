#pragma once

#include <cstddef>

constexpr size_t operator""_KiB(size_t sz) {
    return sz * 1024;
}

constexpr size_t operator""_MiB(size_t sz) {
    return sz * 1024 * 1024;
}
