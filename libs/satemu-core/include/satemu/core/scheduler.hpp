#pragma once

#include <satemu/core_types.hpp>

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
    using EventCallback = void (*)(EventContext &eventContext, void *userContext, uint64 cyclesLate);

    static constexpr EventID kInvalidEvent = ~static_cast<EventID>(0);

    Scheduler() {
        m_eventPtrs.fill(kInvalidEvent);
        m_eventTarget.fill(kNoDeadline);
        m_nextEventIndex = 0;
        Reset();
    }

    // Resets the scheduler's current and target counters
    void Reset() {
        m_currCount = 0;
        m_nextCount = kNoDeadline;
        for (auto target : m_eventTarget) {
            m_nextCount = std::min(m_nextCount, target);
        }
    }

    // Registers an event. The returned ID must be used to refer to the event.
    EventID RegisterEvent(UserID userID, uint64 countFactor, void *userContext, EventCallback callback) {
        assert(m_eventPtrs[userID] == kInvalidEvent);                    // ensure user IDs are unique
        assert(m_nextEventIndex <= std::numeric_limits<EventID>::max()); // IDtype value space exhausted
        EventID id = m_nextEventIndex;
        m_eventPtrs[userID] = id;
        m_eventId[m_nextEventIndex] = id;
        m_eventUserID[m_nextEventIndex] = userID;
        m_eventUserContext[m_nextEventIndex] = userContext;
        m_eventCallback[m_nextEventIndex] = callback;
        m_eventCountFactor[m_nextEventIndex] = countFactor;
        ++m_nextEventIndex;
        return id;
    }

    EventID RegisterEvent(UserID userID, void *userContext, EventCallback callback) {
        return RegisterEvent(userID, 1, userContext, callback);
    }

    // Changes an event's callback.
    void SetEventCallback(EventID id, EventCallback callback) {
        assert(id < kNumEvents);
        m_eventCallback[id] = callback;
    }

    // Changes an event's context.
    void SetEventContext(EventID id, void *userContext) {
        assert(id < kNumEvents);
        m_eventUserContext[id] = userContext;
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
        ScheduleEvent(id, m_currCount + interval);
    }

    // Schedules the specified event to happen at the specified cycle count
    void ScheduleAt(EventID id, uint64 target) {
        assert(id < kNumEvents);
        ScheduleEvent(id, target);
    }

    // Removes the specified event from the schedule
    void Cancel(EventID id) {
        assert(id < kNumEvents);
        m_eventTarget[id] = kNoDeadline;
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
    static constexpr size_t kNumEvents = 5;

    // Schedules an event to execute at the specified time
    void ScheduleEvent(EventID id, uint64 target) {
        m_eventTarget[id] = target;
        m_nextCount = std::min(m_nextCount, target);
    }

    // Executes all scheduled events up to the current count
    FORCE_INLINE void Execute() {
        const uint64 currCount = m_currCount;
        for (size_t index = 0; index < kNumEvents; ++index) {
            if (m_eventTarget[index] == kNoDeadline) {
                continue;
            }
            uint64 target = m_eventTarget[index];
            const uint64 scaledCurrCount = currCount * m_eventCountFactor[index];
            if (scaledCurrCount >= target) {
                const EventCallback callback = m_eventCallback[index];
                void *const userContext = m_eventUserContext[index];
                while (scaledCurrCount >= target) {
                    EventContext eventContext;
                    callback(eventContext, userContext, scaledCurrCount - target);
                    switch (eventContext.action) {
                    case EventContext::Action::Unschedule: target = kNoDeadline; break;
                    case EventContext::Action::RescheduleFromNow:
                        target = scaledCurrCount + eventContext.interval;
                        break;
                    case EventContext::Action::RescheduleFromPrevious: target += eventContext.interval; break;
                    }
                }
                m_eventTarget[index] = target;
            }
        }
        m_nextCount = m_eventTarget[0];
        for (size_t i = 1; i < m_eventTarget.size(); ++i) {
            m_nextCount = std::min(m_nextCount, m_eventTarget[i]);
        }
    }

    uint64 m_currCount;
    uint64 m_nextCount;
    std::array<EventID, kNumEvents> m_eventId;
    std::array<uint64, kNumEvents> m_eventTarget;
    std::array<uint64, kNumEvents> m_eventCountFactor;
    std::array<UserID, kNumEvents> m_eventUserID;
    std::array<void *, kNumEvents> m_eventUserContext;
    std::array<EventCallback, kNumEvents> m_eventCallback;
    size_t m_nextEventIndex;
    std::array<EventID, std::numeric_limits<UserID>::max() + 1> m_eventPtrs;
};

} // namespace satemu::core
