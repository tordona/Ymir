#pragma once

#include "state_scheduler.hpp"
#include "state_system.hpp"

namespace satemu::state {

inline namespace v1 {

    struct State {
        // TODO: IPL ROM hash
        // TODO: Disc header hash

        SchedulerState scheduler;
        SystemState system;
        struct SH2State {}; // one for each CPU
        struct SCUState {};
        struct VDPState {};
        struct SMPCState {};
        struct SCSPState {};
        struct M68KState {};
        struct CDBlockState {};
    };

} // namespace v1

} // namespace satemu::state
