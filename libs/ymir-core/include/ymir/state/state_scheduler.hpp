#pragma once

#include <ymir/core/scheduler_defs.hpp>

#include <ymir/core/types.hpp>

namespace ymir::state {

namespace v1 {

    struct SchedulerState {
        struct EventState {
            uint64 target;
            uint64 countNumerator;
            uint64 countDenominator;
            core::UserEventID id;
        };

        uint64 currCount;
        alignas(16) std::array<EventState, core::kNumScheduledEvents> events;
    };

} // namespace v1

namespace v2 {

    using v1::SchedulerState;

} // namespace v2

inline namespace v3 {

    using v2::SchedulerState;

} // namespace v3

} // namespace ymir::state
