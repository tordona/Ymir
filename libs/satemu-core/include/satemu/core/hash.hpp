#pragma once

#include "types.hpp"

#include <array>
#include <string>

namespace satemu {

using Hash128 = std::array<uint8, 16>;

std::string ToString(const Hash128 &hash);

} // namespace satemu
