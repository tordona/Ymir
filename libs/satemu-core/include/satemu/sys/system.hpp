#pragma once

#include "clocks.hpp"

#include <satemu/util/callback.hpp>

#include <vector>

namespace satemu::sys {

using CBClockSpeedChange = util::RequiredCallback<void(const ClockRatios &clockRatios)>;

struct System {
    VideoStandard videoStandard = VideoStandard::NTSC;
    ClockSpeed clockSpeed = ClockSpeed::_320;

    void UpdateClockRatios() {
        const bool clock352 = clockSpeed == ClockSpeed::_352;
        const bool pal = videoStandard == VideoStandard::PAL;
        const ClockRatios &clockRatios = kClockRatios[clock352 | (pal << 1)];
        for (auto &cb : m_clockSpeedChangeCallbacks) {
            cb(clockRatios);
        }
    }

    void AddClockSpeedChangeCallback(CBClockSpeedChange callback) {
        m_clockSpeedChangeCallbacks.push_back(callback);
    }

private:
    std::vector<CBClockSpeedChange> m_clockSpeedChangeCallbacks;
};

} // namespace satemu::sys
