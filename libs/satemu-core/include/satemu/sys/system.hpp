#pragma once

#include "clocks.hpp"

#include "system_callbacks.hpp"

#include <vector>

namespace satemu::sys {

struct System {
    config::sys::VideoStandard videoStandard = config::sys::VideoStandard::NTSC;
    ClockSpeed clockSpeed = ClockSpeed::_320;

    const ClockRatios &GetClockRatios() const {
        const bool clock352 = clockSpeed == ClockSpeed::_352;
        const bool pal = videoStandard == config::sys::VideoStandard::PAL;
        return kClockRatios[clock352 | (pal << 1)];
    }

    void UpdateClockRatios() {
        const ClockRatios &clockRatios = GetClockRatios();
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
