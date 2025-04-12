#pragma once

#include "scheduler_defs.hpp"

#include <satemu/state/state_scheduler.hpp>

#include <satemu/util/inline.hpp>

#include <array>
#include <cassert>
#include <limits>

namespace satemu::core {

class Scheduler;

// Contains the context for a scheduled event.
// Passed as a parameter to scheduled event handlers to let them reschedule the event relative to the previous trigger
// or the current cycle count.
// By default, events are not rescheduled unless requested by the methods in this struct.
struct EventContext {
    // Reschedules the event with an offset from the current deadline.
    void RescheduleFromPrevious(uint64 interval) {
        action = Action::RescheduleFromPrevious;
        this->interval = interval;
    }

    // Reschedules the event with an offset from the current cycle count.
    void RescheduleFromNow(uint64 interval) {
        action = Action::RescheduleFromNow;
        this->interval = interval;
    }

private:
    enum class Action { Unschedule, RescheduleFromPrevious, RescheduleFromNow };
    Action action = Action::Unschedule;
    uint64 interval = 0;
    friend class Scheduler;
};

class Scheduler {
public:
    // Callback signature for scheduled events
    using EventCallback = void (*)(EventContext &eventContext, void *userContext);

    static constexpr EventID kInvalidEvent = ~static_cast<EventID>(0);

    Scheduler() {
        m_eventPtrs.fill(kInvalidEvent);
        for (Event &event : m_events) {
            event.target = kNoDeadline;
        }
        m_nextEventIndex = 0;
        Reset();
    }

    // Resets the scheduler's current and target counters
    void Reset() {
        m_currCount = 0;
        m_nextCount = kNoDeadline;
        for (const Event &event : m_events) {
            m_nextCount = std::min(m_nextCount, event.target);
        }
    }

    // Registers an event. The returned ID must be used to refer to the event.
    EventID RegisterEvent(UserEventID userID, void *userContext, EventCallback callback) {
        assert(m_eventPtrs[userID] == kInvalidEvent);                    // ensure user IDs are unique
        assert(m_nextEventIndex <= std::numeric_limits<EventID>::max()); // IDtype value space exhausted
        EventID id = m_nextEventIndex;
        m_eventPtrs[userID] = id;
        Event &event = m_events[id];
        m_userIDs[id] = userID;
        event.userContext = userContext;
        event.callback = callback;
        event.countNumerator = 1;
        event.countDenominator = 1;
        ++m_nextEventIndex;
        return id;
    }

    // Sets the event cycle counting factor.
    void SetEventCountFactor(EventID id, uint64 numerator, uint64 denominator) {
        assert(numerator > 0);
        assert(denominator > 0);
        Event &event = m_events[id];

        const uint64 oldTarget = event.target;
        const uint64 oldScaledCount = m_currCount * event.countNumerator / event.countDenominator;
        const sint64 remaining = oldTarget - oldScaledCount;
        const sint64 rescaledCount = m_currCount * numerator / denominator;
        event.target = rescaledCount + remaining;

        event.countNumerator = numerator;
        event.countDenominator = denominator;

        if (m_nextCount == oldTarget) {
            RecalcSchedule();
        }
    }

    uint64 CurrentCount() const {
        return m_currCount;
    }

    uint64 NextCount() const {
        return m_nextCount;
    }

    const uint64 *NextCountPtr() const {
        return &m_nextCount;
    }

    sint64 RemainingCount() const {
        return (sint64)m_nextCount - (sint64)m_currCount;
    }

    // Schedules the specified event to happen $interval cycles from the current count
    void ScheduleFromNow(EventID id, uint64 interval) {
        assert(id < kNumScheduledEvents);
        Event &event = m_events[id];
        const uint64 scaledCount = m_currCount * event.countNumerator / event.countDenominator;
        ScheduleEvent(id, scaledCount + interval);
    }

    // Schedules the specified event to happen at the specified cycle count
    void ScheduleAt(EventID id, uint64 target) {
        assert(id < kNumScheduledEvents);
        ScheduleEvent(id, target);
    }

    // Removes the specified event from the schedule
    void Cancel(EventID id) {
        assert(id < kNumScheduledEvents);
        Event &event = m_events[id];
        event.target = kNoDeadline;
    }

    // Advances the scheduler by the specified count and fire scheduled events
    FORCE_INLINE void Advance(uint64 count) {
        m_currCount += count;
        if (m_currCount >= m_nextCount) {
            Execute();
        }
    }

    // -------------------------------------------------------------------------
    // Save states

    void SaveState(state::SchedulerState &state) const {
        state.currCount = m_currCount;
        for (size_t i = 0; i < kNumScheduledEvents; i++) {
            state.events[i].id = m_userIDs[i];
            state.events[i].target = m_events[i].target;
            state.events[i].countNumerator = m_events[i].countNumerator;
            state.events[i].countDenominator = m_events[i].countDenominator;
        }
    }

    bool ValidateState(state::SchedulerState &state) const {
        for (size_t i = 0; i < kNumScheduledEvents; i++) {
            const size_t eventIndex = m_eventPtrs[state.events[i].id];
            if (eventIndex == kInvalidEvent) {
                return false;
            }
        }
        return true;
    }

    void LoadState(state::SchedulerState &state) {
        m_currCount = state.currCount;
        for (size_t i = 0; i < kNumScheduledEvents; i++) {
            const size_t eventIndex = m_eventPtrs[state.events[i].id];
            assert(eventIndex != kInvalidEvent);
            m_events[eventIndex].target = state.events[i].target;
            m_events[eventIndex].countNumerator = state.events[i].countNumerator;
            m_events[eventIndex].countDenominator = state.events[i].countDenominator;
        }
        RecalcSchedule();
    }

private:
    static constexpr uint64 kNoDeadline = ~static_cast<uint64>(0);

    struct Event {
        uint64 target;
        uint64 countNumerator;
        uint64 countDenominator;
        void *userContext;
        EventCallback callback;

        FORCE_INLINE uint64 CalcTargetScaledByReciprocal() const {
            return (target * countDenominator + countNumerator - 1) / countNumerator;
        }
    };

    // Schedules an event to execute at the specified time
    FORCE_INLINE void ScheduleEvent(EventID id, uint64 target) {
        Event &event = m_events[id];
        event.target = target;
        const uint64 scaledTarget = event.CalcTargetScaledByReciprocal();
        m_nextCount = std::min(m_nextCount, scaledTarget);
    }

    // Executes all scheduled events up to the current count
    FORCE_INLINE void Execute() {
        const uint64 currCount = m_currCount;
        for (size_t index = 0; index < kNumScheduledEvents; ++index) {
            Event &event = m_events[index];
            if (event.target == kNoDeadline) {
                continue;
            }
            uint64 target = event.target;
            const uint64 scaledCurrCount = currCount * event.countNumerator / event.countDenominator;
            if (scaledCurrCount >= target) {
                const EventCallback callback = event.callback;
                void *const userContext = event.userContext;
                while (scaledCurrCount >= target) {
                    EventContext eventContext;
                    callback(eventContext, userContext);
                    switch (eventContext.action) {
                    case EventContext::Action::Unschedule: target = kNoDeadline; break;
                    case EventContext::Action::RescheduleFromNow:
                        target = scaledCurrCount + eventContext.interval;
                        break;
                    case EventContext::Action::RescheduleFromPrevious: target += eventContext.interval; break;
                    }
                }
                event.target = target;
            }
        }

        RecalcSchedule();
    }

    FORCE_INLINE void RecalcSchedule() {
        m_nextCount = kNoDeadline;
        for (const Event &event : m_events) {
            if (event.target == kNoDeadline) {
                continue;
            }
            const uint64 scaledTarget = event.CalcTargetScaledByReciprocal();
            m_nextCount = std::min(m_nextCount, scaledTarget);
        }
    }

    uint64 m_currCount;
    uint64 m_nextCount;
    std::array<Event, kNumScheduledEvents> m_events;
    std::array<UserEventID, kNumScheduledEvents> m_userIDs;
    size_t m_nextEventIndex;
    std::array<EventID, std::numeric_limits<UserEventID>::max() + 1> m_eventPtrs;
};

} // namespace satemu::core
