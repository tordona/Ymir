#pragma once

#include <satemu/core/scheduler_defs.hpp>

#include <satemu/core/types.hpp>

namespace satemu::state {

inline namespace v1 {

    struct SchedulerState {
        struct Event {
            uint64 target;
            uint64 countNumerator;
            uint64 countDenominator;
            core::UserEventID id;
        };

        uint64 currCount;
        std::array<Event, core::kNumScheduledEvents> events;
    };

} // namespace v1

} // namespace satemu::state
