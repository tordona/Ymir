#pragma once

#include "inline.hpp"

#include <concepts>
#include <type_traits>

namespace bit {

template <unsigned B, std::integral T>
ALWAYS_INLINE constexpr auto sign_extend(T x) {
    using ST = std::make_signed_t<T>;
    struct {
        ST x : B;
    } s{static_cast<ST>(x)};
    return s.x;
}

// Extracts a range of bits from the value.
// start and end are both inclusive.
template <size_t start, size_t end = start, typename T>
ALWAYS_INLINE constexpr T extract(T value) {
    static_assert(start < sizeof(T) * 8, "start out of range");
    static_assert(end < sizeof(T) * 8, "end out of range");
    static_assert(end >= start, "end cannot be before start");

    using UT = std::make_unsigned_t<T>;

    constexpr size_t length = end - start + 1;
    constexpr UT mask = static_cast<UT>(~(~0 << length));
    return (value >> start) & mask;
}

} // namespace bit
