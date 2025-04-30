#pragma once

/**
@file
@brief Mode 2 track subheader structure definition.
*/

#include <ymir/core/types.hpp>

namespace ymir::media {

/// @brief Mode 2 track subheader
struct Subheader {
    uint8 fileNum;    ///< File number
    uint8 chanNum;    ///< Channel number
    uint8 submode;    ///< Submode
    uint8 codingInfo; ///< Coding information
};

} // namespace ymir::media
