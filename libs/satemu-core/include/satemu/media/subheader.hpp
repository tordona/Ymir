#pragma once

#include <satemu/core_types.hpp>

namespace satemu::media {

// Mode 2 track subheader
struct Subheader {
    uint8 fileNum;
    uint8 chanNum;
    uint8 submode;
    uint8 codingInfo;
};

} // namespace satemu::media
