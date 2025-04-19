#pragma once

#include <ymir/core/types.hpp>

namespace ymir::media {

// Mode 2 track subheader
struct Subheader {
    uint8 fileNum;
    uint8 chanNum;
    uint8 submode;
    uint8 codingInfo;
};

} // namespace ymir::media
