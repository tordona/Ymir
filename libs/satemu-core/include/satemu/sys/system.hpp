#pragma once

#include "clocks.hpp"

namespace satemu::sys {

enum class VideoStandard { NTSC, PAL };
enum class ClockSpeed { _320, _352 };

struct System {
    VideoStandard videoStandard = VideoStandard::NTSC;
    ClockSpeed clockSpeed = ClockSpeed::_320;

    const ClockRatios &GetClockRatios() const {
        const bool clock352 = clockSpeed == ClockSpeed::_352;
        const bool pal = videoStandard == VideoStandard::PAL;
        return kClockRatios[clock352 | (pal << 1)];
    }
};

} // namespace satemu::sys
