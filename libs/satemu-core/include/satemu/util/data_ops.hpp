#pragma once

#include <satemu/core/types.hpp>

#include "bit_ops.hpp"
#include "inline.hpp"

#include <bit>
#include <concepts>
#include <cstring>
#include <span>

namespace util {

// Determines if the given address is in range [start..end]
template <uint32 start, uint32 end>
FORCE_INLINE constexpr bool AddressInRange(uint32 address) {
    return address >= start && address <= end;
}

template <typename T>
FORCE_INLINE T ReadBE(const uint8 *data) {
    T value = 0;
    std::memcpy(&value, data, sizeof(T));
    if constexpr (std::endian::native == std::endian::little) {
        value = bit::byte_swap(value);
    }
    return value;
}

template <typename T>
FORCE_INLINE void WriteBE(uint8 *data, T value) {
    if constexpr (std::endian::native == std::endian::little) {
        value = bit::byte_swap(value);
    }
    std::memcpy(data, &value, sizeof(T));
}

template <typename T>
FORCE_INLINE T ReadLE(const uint8 *data) {
    T value = 0;
    std::memcpy(&value, data, sizeof(T));
    if constexpr (std::endian::native == std::endian::big) {
        value = bit::byte_swap(value);
    }
    return value;
}

template <typename T>
FORCE_INLINE void WriteLE(uint8 *data, T value) {
    if constexpr (std::endian::native == std::endian::big) {
        value = bit::byte_swap(value);
    }
    std::memcpy(data, &value, sizeof(T));
}

template <std::integral T>
FORCE_INLINE T DecimalToInt(std::span<uint8> numericText) {
    T result = 0;
    for (auto ch : numericText) {
        if (ch < '0' || ch > '9') {
            break;
        }
        result = result * 10 + (static_cast<T>(ch) - '0');
    }
    return result;
}

template <bool lowerByte, bool upperByte, uint32 lb, uint32 ub, std::integral TSrc>
FORCE_INLINE void SplitReadWord(uint16 &dstValue, TSrc srcValue) {
    static constexpr uint32 dstlb = lowerByte ? lb : 8;
    static constexpr uint32 dstub = upperByte ? ub : 7;

    static constexpr uint32 srclb = dstlb - lb;
    static constexpr uint32 srcub = dstub - lb;

    bit::deposit_into<dstlb, dstub>(dstValue, bit::extract<srclb, srcub>(srcValue));
}

template <bool lowerByte, bool upperByte, uint32 lb, uint32 ub, std::integral TDst>
FORCE_INLINE void SplitWriteWord(TDst &dstValue, uint16 srcValue) {
    if constexpr (lowerByte && upperByte) {
        bit::deposit_into<0, ub - lb>(dstValue, bit::extract<lb, ub>(srcValue));
    } else {
        static constexpr uint32 srclb = lowerByte ? lb : 8;
        static constexpr uint32 srcub = upperByte ? ub : 7;

        static constexpr uint32 dstlb = srclb - lb;
        static constexpr uint32 dstub = srcub - lb;

        bit::deposit_into<dstlb, dstub>(dstValue, bit::extract<srclb, srcub>(srcValue));
    }
}

} // namespace util
