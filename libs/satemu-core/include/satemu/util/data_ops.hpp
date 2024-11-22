#pragma once

#include "satemu/core_types.hpp"

#include <concepts>

namespace satemu {

// Specifies valid types for memory accesses
template <typename T>
concept mem_access_type = std::same_as<T, uint8> || std::same_as<T, uint16> || std::same_as<T, uint32>;

// Determines if the given address is in range [base..base+size)
template <uint32 base, uint64 size>
constexpr bool AddressInRange(uint32 address) {
    return address >= base && address < base + size;
}

template <mem_access_type T>
T ReadBE(uint8 *data) {
    T value = 0;
    for (uint32 i = 0; i < sizeof(T); i++) {
        value |= data[i] << ((sizeof(T) - 1u - i) * 8u);
    }
    return value;
}

template <mem_access_type T>
void WriteBE(uint8 *data, T value) {
    for (uint32 i = 0; i < sizeof(T); i++) {
        data[i] = value >> ((sizeof(T) - 1u - i) * 8u);
    }
}

} // namespace satemu
