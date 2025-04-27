#pragma once

#include "inline.hpp"

#include <array>
#include <bit>
#include <climits>
#include <concepts>
#include <cstddef>
#include <type_traits>

namespace bit {

// Determines if the given unsigned integral is a power of two.
template <std::unsigned_integral T>
FORCE_INLINE constexpr bool is_power_of_two(T x) {
    return x != 0 && (x & (x - 1)) == 0;
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

// Tests if a bit is set.
template <std::size_t pos, std::integral T>
[[nodiscard]] FORCE_INLINE constexpr bool test(T value) {
    static_assert(pos < sizeof(T) * 8, "pos out of range");
    return (value >> pos) & 1;
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

namespace detail {

    template <class T, std::size_t... N>
    constexpr T byte_swap_impl(T value, std::index_sequence<N...>) {
        return ((((value >> (N * CHAR_BIT)) & (T)(unsigned char)(-1)) << ((sizeof(T) - 1 - N) * CHAR_BIT)) | ...);
    };

} // namespace detail

// Byte swaps the given value
template <std::unsigned_integral T>
constexpr T byte_swap(T value) {
    return detail::byte_swap_impl<T>(value, std::make_index_sequence<sizeof(T)>{});
}

// Byte swaps the given value if the endianness doesn't match native endianness
template <std::endian endianness, std::unsigned_integral T>
constexpr T endian_swap(T value) {
    if constexpr (endianness == std::endian::native) {
        return value;
    } else {
        return byte_swap(value);
    }
}

// Byte swaps the given value if the native endianness is not big-endian
template <std::unsigned_integral T>
constexpr T big_endian_swap(T value) {
    return endian_swap<std::endian::big>(value);
}

// Byte swaps the given value if the native endianness is not little-endian
template <std::unsigned_integral T>
constexpr T little_endian_swap(T value) {
    return endian_swap<std::endian::little>(value);
}

// Reverses the bits of the given value
constexpr uint8 reverse(uint8 n) {
    n = (n << 4u) | (n >> 4u);
    n = ((n & 0x33u) << 2u) | ((n >> 2u) & 0x33u);
    n = ((n & 0x55u) << 1u) | ((n >> 1u) & 0x55u);
    return n;
}

// Reverses the bits of the given value
constexpr uint16 reverse(uint16 n) {
    n = (n << 8u) | (n >> 8u);
    n = ((n & 0x0F0Fu) << 4u) | ((n >> 4u) & 0x0F0Fu);
    n = ((n & 0x3333u) << 2u) | ((n >> 2u) & 0x3333u);
    n = ((n & 0x5555u) << 1u) | ((n >> 1u) & 0x5555u);
    return n;
}

// Reverses the bits of the given value
constexpr uint32 reverse(uint32 n) {
    n = (n << 16u) | (n >> 16u);
    n = ((n & 0x00FF00FFu) << 8u) | ((n >> 8u) & 0x00FF00FFu);
    n = ((n & 0x0F0F0F0Fu) << 4u) | ((n >> 4u) & 0x0F0F0F0Fu);
    n = ((n & 0x33333333u) << 2u) | ((n >> 2u) & 0x33333333u);
    n = ((n & 0x55555555u) << 1u) | ((n >> 1u) & 0x55555555u);
    return n;
}

// Reverses the bits of the given value
constexpr uint64 reverse(uint64 n) {
    n = (n << 32ull) | (n >> 32ull);
    n = ((n & 0x0000FFFF0000FFFFull) << 16ull) | ((n >> 16ull) & 0x0000FFFF0000FFFFull);
    n = ((n & 0x00FF00FF00FF00FFull) << 8ull) | ((n >> 8ull) & 0x00FF00FF00FF00FFull);
    n = ((n & 0x0F0F0F0F0F0F0F0Full) << 4ull) | ((n >> 4ull) & 0x0F0F0F0F0F0F0F0Full);
    n = ((n & 0x3333333333333333ull) << 2ull) | ((n >> 2ull) & 0x3333333333333333ull);
    n = ((n & 0x5555555555555555ull) << 1ull) | ((n >> 1ull) & 0x5555555555555555ull);
    return n;
}

} // namespace bit
