#pragma once

#include "state_scheduler.hpp"

namespace satemu::state {

inline namespace v1 {

    struct State {
        // IPL ROM hash
        // Disc header hash

        SchedulerState scheduler;
        struct SystemState {}; // video standard, clock speed, SH2 slave enable, system memory
        struct SH2State {};    // one for each CPU
        struct SCUState {};
        struct VDPState {};
        struct SMPCState {};
        struct SCSPState {};
        struct M68KState {};
        struct CDBlockState {};
    };

} // namespace v1

} // namespace satemu::state
