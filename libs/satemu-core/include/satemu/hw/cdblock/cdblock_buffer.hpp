#pragma once

#include <satemu/core_types.hpp>

#include <array>

namespace satemu::cdblock {

struct Buffer {
    std::array<uint8, 2352> data;
    uint16 size;
    uint32 frameAddress;
};

} // namespace satemu::cdblock
