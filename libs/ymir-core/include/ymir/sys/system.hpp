#pragma once

#include "clocks.hpp"

#include "system_internal_callbacks.hpp"

#include <ymir/state/state_system.hpp>

#include <vector>

namespace ymir::sys {

struct System {
    core::config::sys::VideoStandard videoStandard = core::config::sys::VideoStandard::NTSC;
    ClockSpeed clockSpeed = ClockSpeed::_320;

    const ClockRatios &GetClockRatios() const {
        const bool clock352 = clockSpeed == ClockSpeed::_352;
        const bool pal = videoStandard == core::config::sys::VideoStandard::PAL;
        return kClockRatios[(pal << 1) | (clock352 << 0)];
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

    // -------------------------------------------------------------------------
    // Save states

    void SaveState(state::SystemState &state) const {
        state.videoStandard = videoStandard;
        state.clockSpeed = clockSpeed;
    }

    bool ValidateState(const state::SystemState &state) const {
        if (videoStandard != core::config::sys::VideoStandard::NTSC &&
            videoStandard != core::config::sys::VideoStandard::PAL) {
            return false;
        }
        if (clockSpeed != ClockSpeed::_320 && clockSpeed != ClockSpeed::_352) {
            return false;
        }
        return true;
    }

    void LoadState(const state::SystemState &state) {
        videoStandard = state.videoStandard;
        clockSpeed = state.clockSpeed;

        UpdateClockRatios();
    }

private:
    std::vector<CBClockSpeedChange> m_clockSpeedChangeCallbacks;
};

} // namespace ymir::sys
