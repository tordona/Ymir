#pragma once

#include <ymir/core/configuration_defs.hpp>
#include <ymir/sys/clocks.hpp>
#include <ymir/sys/memory_defs.hpp>

#include <ymir/core/hash.hpp>

namespace ymir::state {

namespace v1 {

    struct SystemState {
        core::config::sys::VideoStandard videoStandard;
        sys::ClockSpeed clockSpeed;
        bool slaveSH2Enabled;

        alignas(16) XXH128Hash iplRomHash;

        alignas(16) std::array<uint8, sys::kWRAMLowSize> WRAMLow;
        alignas(16) std::array<uint8, sys::kWRAMHighSize> WRAMHigh;
    };

} // namespace v1

inline namespace v2 {

    using v1::SystemState;

} // namespace v2

} // namespace ymir::state
