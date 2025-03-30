#pragma once

#include "inline.hpp"

#include <array>
#include <bit>
#include <concepts>
#include <cstddef>
#include <type_traits>

namespace bit {

// Determines if the given unsigned integral is a power of two.
template <std::unsigned_integral T>
FORCE_INLINE constexpr bool is_power_of_two(T x) {
    return (x & (x - 1)) == 0;
}

// Returns the next power of two not less than x.
template <std::unsigned_integral T>
FORCE_INLINE constexpr T next_power_of_two(T x) {
    x--;
    x |= x >> 1u;
    x |= x >> 2u;
    x |= x >> 4u;
    if constexpr (sizeof(T) > 1) {
        x |= x >> 8u;
    }
    if constexpr (sizeof(T) > 2) {
        x |= x >> 16u;
    }
    if constexpr (sizeof(T) > 4) {
        x |= x >> 32ull;
    }
    x++;
    return x;
}

// Sign-extend from a constant bit width.
// The return type is the signed equivalent of T.
template <unsigned B, std::integral T>
[[nodiscard]] FORCE_INLINE constexpr auto sign_extend(T x) {
    using ST = std::make_signed_t<T>;
    struct {
        ST x : B;
    } s{static_cast<ST>(x)};
    return s.x;
}

// Extracts a range of bits from the value.
// start and end are both inclusive.
template <std::size_t start, std::size_t end = start, std::integral T>
[[nodiscard]] FORCE_INLINE constexpr T extract(T value) {
    static_assert(start < sizeof(T) * 8, "start out of range");
    static_assert(end < sizeof(T) * 8, "end out of range");
    static_assert(end >= start, "end cannot be before start");

    using UT = std::make_unsigned_t<T>;

    constexpr std::size_t length = end - start;
    constexpr UT mask = static_cast<UT>(~(~0 << length << 1));
    return (value >> start) & mask;
}

// Extracts a range of bits from the value, converting them into a signed integer.
// start and end are both inclusive.
template <std::size_t start, std::size_t end = start, std::integral T>
[[nodiscard]] FORCE_INLINE auto extract_signed(T value) {
    return sign_extend<end - start + 1>(extract<start, end>(value));
}

// Deposits a range of bits into the value and returns the modified value.
// start and end are both inclusive.
template <std::size_t start, std::size_t end = start, std::integral T, std::integral TV = T>
[[nodiscard]] FORCE_INLINE constexpr T deposit(T base, TV value) {
    static_assert(start < sizeof(T) * 8, "start out of range");
    static_assert(end < sizeof(T) * 8, "end out of range");
    static_assert(end >= start, "end cannot be before start");

    using UT = std::make_unsigned_t<T>;

    constexpr std::size_t length = end - start;
    constexpr UT mask = static_cast<UT>(~(~0 << length << 1));
    base &= ~(mask << start);
    base |= (value & mask) << start;
    return base;
}

// Deposits a range of bits into the destination value.
// start and end are both inclusive.
template <std::size_t start, std::size_t end = start, std::integral T, std::integral TV = T>
FORCE_INLINE constexpr void deposit_into(T &dest, TV value) {
    dest = deposit<start, end>(dest, value);
}

// Compresses the selected bits of the value into the least significant bits of the output.
template <std::size_t mask, std::integral T>
FORCE_INLINE constexpr T gather(T value) {
    // TODO: use _pext_u32/64 if available

    // Hacker's Delight, volume 2, page 153
    value &= mask;               // Clear irrelevant bits
    constexpr T mk = ~mask << 1; // We will count 0s to the right
    T m = mask;

    constexpr T iters = std::countr_zero(sizeof(T)) + 3;
    for (T i = 0; i < iters; i++) {
        T mp = mk ^ (mk << 1); // Parallel suffix
        mp ^= mp << 2;
        mp ^= mp << 4;
        for (T j = 3; j < iters; j++) {
            mp ^= mp << ((T)1 << j);
        }

        T mv = mp & m; // Bits to move

        m = (m ^ mv) | (mv >> ((T)1 << i)); // Compress m
        T t = value & mv;
        value = (value ^ t) | (t >> ((T)1 << i)); // Compress x
        mk &= ~mp;
    }
    return value;
}

// Expands the least significant bits of the value into the corresponding bits of the given mask.
template <std::size_t mask, std::integral T>
FORCE_INLINE constexpr T scatter(T value) {
    // TODO: use _pdep_u32/64 if available

    // Hacker's Delight, volume 2, page 157
    T m0 = mask;                       // Save original mask
    T mk = static_cast<T>(~mask << 1); // We will count 0s to the right
    T m = mask;

    constexpr T iters = std::countr_zero(sizeof(T)) + 3;
    std::array<T, iters> array{};
    for (T i = 0; i < iters; i++) {
        T mp = mk ^ (mk << 1); // Parallel suffix
        mp ^= mp << 2;
        mp ^= mp << 4;
        for (T j = 3; j < iters; j++) {
            mp ^= mp << ((T)1 << j);
        }

        T mv = mp & m; // Bits to move

        array[i] = mv;

        m = (m ^ mv) | (mv >> ((T)1 << i)); // Compress m
        mk &= ~mp;
    }

    for (T i = iters - 1; i >= 0 && i < iters; i--) {
        T mv = array[i];
        T t = value << ((T)1 << i);
        value = (value & ~mv) | (t & mv);
    }

    return value & m0; // Clear out extraneous bits
}

} // namespace bit
