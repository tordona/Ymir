#pragma once

#include <satemu/util/size_ops.hpp>

namespace satemu::sh2 {

inline constexpr size_t kIPLSize = 512_KiB;

inline constexpr size_t kWRAMLowSize = 1_MiB;
inline constexpr size_t kWRAMHighSize = 1_MiB;

} // namespace satemu::sh2
