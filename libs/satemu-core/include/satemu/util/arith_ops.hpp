#pragma once

#include <concepts>

namespace util {

// Computes `base` to the power of `exp` where both are integral types.
template <std::integral T, std::unsigned_integral U>
constexpr auto ipow(T base, U exp) {
    if (exp == 0) {
        return 1;
    }
    const auto val = ipow(base, exp / 2);
    auto sqrVal = val * val;
    if (exp & 1) {
        sqrVal *= base;
    }
    return sqrVal;
}

// Converts the given number into binary-coded decimal representation (BCD).
// The number of digits is limited to the number of nibbles available in the return type R.
template <std::integral T, std::integral R = T>
constexpr R to_bcd(T value) {
    // TODO: check behavior with signed integers
    constexpr auto numDigits = sizeof(R) * 2u;
    constexpr T mod = ipow(10, numDigits);

    value %= mod;
    R output = 0;
    R divisor = 1;
    for (int i = 0; i < numDigits; i++) {
        output |= (value / divisor % 10) << (4 * i);
        divisor *= 10;
    }
    return output;
}

} // namespace util
