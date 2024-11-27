#pragma once

#include <satemu/core_types.hpp>

#include <satemu/util/inline.hpp>

namespace satemu::vdp {

union Color555 {
    uint16 u16;
    struct {
        uint16 r : 5;
        uint16 g : 5;
        uint16 b : 5;
        uint16 msb : 1; // CC in CRAM, transparency in cells when using RGB format
    };
};

union Color888 {
    uint32 u32;
    struct {
        uint32 r : 8;
        uint32 g : 8;
        uint32 b : 8;
        uint32 : 7;
        uint32 msb : 1; // CC in CRAM, transparency in cells when using RGB format
    };
};

FORCE_INLINE Color888 ConvertRGB555to888(Color555 color) {
    return Color888{
        .r = static_cast<uint32>(color.r) << 3u,
        .g = static_cast<uint32>(color.g) << 3u,
        .b = static_cast<uint32>(color.b) << 3u,
        .msb = color.msb,
    };
}

} // namespace satemu::vdp
