#pragma once

#include <satemu/core_types.hpp>

namespace satemu::vdp {

union Color555 {
    uint16 u16;
    struct {
        uint16 r : 5;
        uint16 g : 5;
        uint16 b : 5;
        uint16 CC : 1;
    };
};

struct Color888 {
    uint32 u32;
    struct {
        uint32 r : 8;
        uint32 g : 8;
        uint32 b : 8;
        uint32 : 7;
        uint32 CC : 1;
    };
};

Color888 ConvertRGB555to888(Color555 color);

} // namespace satemu::vdp
