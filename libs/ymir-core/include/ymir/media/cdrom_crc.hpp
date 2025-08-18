#pragma once

#include <ymir/core/types.hpp>

#include <span>

namespace ymir::media {

uint32 CalcCRC(std::span<uint8, 2064> sector);

} // namespace ymir::media
