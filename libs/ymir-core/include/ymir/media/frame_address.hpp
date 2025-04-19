#pragma once

#include <ymir/core/types.hpp>

namespace ymir::media {

inline uint32 TimestampToFrameAddress(uint32 min, uint32 sec, uint32 frame) {
    return min * 75 * 60 + sec * 75 + frame;
}

inline uintmax_t TimestampToFileOffset(uint32 min, uint32 sec, uint32 frame, uint32 sectorSize) {
    return static_cast<uintmax_t>(TimestampToFrameAddress(min, sec, frame)) * sectorSize;
}

} // namespace ymir::media
