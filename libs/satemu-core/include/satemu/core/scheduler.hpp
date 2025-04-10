#pragma once

#include <satemu/core/types.hpp>

#include <satemu/util/inline.hpp>

#include <array>
#include <cassert>
#include <limits>

namespace satemu::core {

using IDtype = uint8;
using EventID = IDtype;
using UserID = IDtype;

// User IDs for all events in the emulator
namespace events {

    // VDP phase update
    inline constexpr UserID VDPPhase = 0x10;

    // SCSP sample
    inline constexpr UserID SCSPSample = 0x20;

    // CD block drive state update
    inline constexpr UserID CDBlockDriveState = 0x30;

    // CD block command processing
    inline constexpr UserID CDBlockCommand = 0x31;

    // SCU timer 1 interrupt
    inline constexpr UserID SCUTimer1 = 0x40;

    // SMPC command processing
    inline constexpr UserID SMPCCommand = 0x50;

} // namespace events

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
    EventID RegisterEvent(UserID userID, void *userContext, EventCallback callback) {
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
        assert(id < kNumEvents);
        Event &event = m_events[id];
        const uint64 scaledCount = m_currCount * event.countNumerator / event.countDenominator;
        ScheduleEvent(id, scaledCount + interval);
    }

    // Schedules the specified event to happen at the specified cycle count
    void ScheduleAt(EventID id, uint64 target) {
        assert(id < kNumEvents);
        ScheduleEvent(id, target);
    }

    // Removes the specified event from the schedule
    void Cancel(EventID id) {
        assert(id < kNumEvents);
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

private:
    static constexpr uint64 kNoDeadline = ~static_cast<uint64>(0);
    static constexpr size_t kNumEvents = 6;

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
        for (size_t index = 0; index < kNumEvents; ++index) {
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
    std::array<Event, kNumEvents> m_events;
    std::array<UserID, kNumEvents> m_userIDs;
    size_t m_nextEventIndex;
    std::array<EventID, std::numeric_limits<UserID>::max() + 1> m_eventPtrs;
};

} // namespace satemu::core
