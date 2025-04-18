#pragma once

#include <array>
#include <string>

namespace satemu {

using XXH128Hash = std::array<uint8_t, 16>;

std::string ToString(const XXH128Hash &hash);

} // namespace satemu
