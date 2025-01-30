#pragma once

#include <satemu/media/subheader.hpp>

#include <satemu/core_types.hpp>

#include <array>

namespace satemu::cdblock {

struct Buffer {
    std::array<uint8, 2352> data;
    uint16 size;
    uint32 frameAddress;
    media::Subheader subheader;
};

} // namespace satemu::cdblock
