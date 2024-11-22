#pragma once

#include "satemu/util/size_ops.hpp"

namespace satemu {

constexpr size_t kIPLSize = 512_KiB;

constexpr size_t kWRAMLowSize = 1_MiB;
constexpr size_t kWRAMHighSize = 1_MiB;

constexpr size_t kM68KWRAMSize = 512_KiB;

} // namespace satemu
