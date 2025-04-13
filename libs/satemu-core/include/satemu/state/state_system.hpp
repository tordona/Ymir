#pragma once

#include <satemu/core/configuration_defs.hpp>
#include <satemu/sys/clocks.hpp>
#include <satemu/sys/memory_defs.hpp>

#include <satemu/state/state_hash.hpp>

namespace satemu::state {

inline namespace v1 {

    struct SystemState {
        config::sys::VideoStandard videoStandard;
        sys::ClockSpeed clockSpeed;
        bool slaveSH2Enabled;

        Hash128 iplRomHash;

        std::array<uint8, sys::kWRAMLowSize> WRAMLow;
        std::array<uint8, sys::kWRAMHighSize> WRAMHigh;
    };

} // namespace v1

} // namespace satemu::state
