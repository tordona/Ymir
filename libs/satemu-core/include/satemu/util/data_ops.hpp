#pragma once

#include "satemu/core_types.hpp"

#include "inline.hpp"

namespace util {

// Determines if the given address is in range [base..base+size)
template <uint32 base, uint64 size>
ALWAYS_INLINE constexpr bool AddressInRange(uint32 address) {
    return address >= base && address < base + size;
}

template <typename T>
ALWAYS_INLINE T ReadBE(uint8 *data) {
    T value = 0;
    for (uint32 i = 0; i < sizeof(T); i++) {
        value |= data[i] << ((sizeof(T) - 1u - i) * 8u);
    }
    return value;
}

template <typename T>
ALWAYS_INLINE void WriteBE(uint8 *data, T value) {
    for (uint32 i = 0; i < sizeof(T); i++) {
        data[i] = value >> ((sizeof(T) - 1u - i) * 8u);
    }
}

} // namespace util
