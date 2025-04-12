#pragma once

#include "state_scheduler.hpp"
#include "state_sh2.hpp"
#include "state_system.hpp"

namespace satemu::state {

inline namespace v1 {

    struct State {
        // TODO: IPL ROM hash
        // TODO: Disc header hash

        SchedulerState scheduler;
        SystemState system;
        SH2State msh2;
        SH2State ssh2;
        struct SCUState {};
        struct VDPState {};
        struct SMPCState {};
        struct SCSPState {};
        struct M68KState {};
        struct CDBlockState {};
    };

} // namespace v1

} // namespace satemu::state
