#pragma once

/**
@file
@brief Defines `util::Event`, a synchronization primitive that blocks threads until a signal is raised.
*/

#include <condition_variable>
#include <mutex>

namespace util {

/// @brief An event that blocks threads until it is signaled.
///
/// Example usage:
///
/// ```cpp
/// // A simple blocking queue that blocks consumer thread until there are
/// template <typename T>
/// class BlockingQueue {
///     // Inserts a value in the queue.
///     void Offer(T value) {
///         // Wait until the queue is not full.
///         // `false` causes the signal to not be reset once received.
///         // This will block the thread until the event is signaled.
///         m_queueNotFullEvent.Wait(false);
///
///         // Push the element to the queue
///         // NOTE: synchronization omitted for brevity
///         m_queue.offer(value);
///
///         // Signal not empty event now that there is at least one element on the queue.
///         // This will unblock all threads waiting for this event to be signaled.
///         m_queueNotEmptyEvent.Set();
///     }
///
///     // Removes a value from the queue.
///     T Poll() {
///         // Wait until the queue is not empty.
///         // `false` causes the signal to not be reset once received.
///         // This will block the thread until the event is signaled.
///         m_queueNotEmptyEvent.Wait(false);
///
///         // Pop the element from the queue
///         // NOTE: synchronization omitted for brevity
///         T value = m_queue.poll();
///
///         // Signal not full event now that there is at least one free space on the queue.
///         // This will unblock all threads waiting for this event to be signaled.
///         m_queueNotFullEvent.Set();
///     }
/// private:
///     Queue<T> m_queue = ...; // some queue implementation
///
///     // Initialize events to their default states.
///     // The queue starts out empty, so the "not full" event is signaled and the "not empty" event is not.
///     util::Event m_queueNotFullEvent{true};
///     util::Event m_queueNotEmptyEvent{false};
/// };
///
///
/// ```
class Event {
public:
    /// @brief Constructs a new event with an initial signal state.
    /// @param[in] set the signal state
    [[nodiscard]] explicit Event(bool set = false) noexcept
        : m_set(set) {}

    /// @brief Waits until the event is signaled, optionally resetting it.
    /// @param[in] autoReset whether to automatically clear the event after being signaled
    void Wait(bool autoReset) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_condVar.wait(lock, [this] { return m_set; });
        if (autoReset) {
            m_set = false;
        }
    }

    /// @brief Signals the event and notifies all waiting threads.
    void Set() {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_set = true;
        m_condVar.notify_all();
    }

    /// @brief Resets (clears) the event signal.
    void Reset() {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_set = false;
    }

private:
    std::mutex m_mutex;
    std::condition_variable m_condVar;
    bool m_set;
};

} // namespace util
