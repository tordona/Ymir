#pragma once

#include <satemu/core_types.hpp>

#include "inline.hpp"

namespace util {

// Determines if the given address is in range [start..end]
template <uint32 start, uint64 end>
FORCE_INLINE constexpr bool AddressInRange(uint32 address) {
    return address >= start && address < end;
}

template <typename T>
FORCE_INLINE T ReadBE(const uint8 *data) {
    T value = 0;
    for (uint32 i = 0; i < sizeof(T); i++) {
        value |= data[i] << ((sizeof(T) - 1u - i) * 8u);
    }
    return value;
}

template <typename T>
FORCE_INLINE void WriteBE(uint8 *data, T value) {
    for (uint32 i = 0; i < sizeof(T); i++) {
        data[i] = value >> ((sizeof(T) - 1u - i) * 8u);
    }
}

template <typename T>
FORCE_INLINE T ReadLE(const uint8 *data) {
    T value = 0;
    for (uint32 i = 0; i < sizeof(T); i++) {
        value |= data[i] << (i * 8u);
    }
    return value;
}

template <typename T>
FORCE_INLINE void WriteLE(uint8 *data, T value) {
    for (uint32 i = 0; i < sizeof(T); i++) {
        data[i] = value >> (i * 8u);
    }
}

} // namespace util
