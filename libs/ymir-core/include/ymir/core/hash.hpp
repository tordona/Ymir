#pragma once

#include <array>
#include <string>

namespace ymir {

using XXH128Hash = std::array<uint8_t, 16>;

XXH128Hash CalcHash128(const void *input, size_t len, uint64_t seed);

std::string ToString(const XXH128Hash &hash);

} // namespace ymir
