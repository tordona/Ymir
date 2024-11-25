#include <satemu/hw/vdp/vdp_common_defs.hpp>

namespace satemu::vdp {

Color888 ConvertRGB555to888(Color555 color) {
    return Color888{
        .r = static_cast<uint32>(color.r) << 3u,
        .g = static_cast<uint32>(color.g) << 3u,
        .b = static_cast<uint32>(color.b) << 3u,
        .msb = color.msb,
    };
}

} // namespace satemu::vdp
