#pragma once

#include "event.hpp"

#include <mutex>
#include <queue>

namespace util {

template <typename T>
class ConcurrentQueue {
public:
    /**
     * @brief Writes an object to the queue.
     * @param obj The object to copy into the queue.
     * @param notifyAll true notifies all waiting threads. false notifies one thread.
     */
    void Offer(const T &obj, bool notifyAll = true) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.push(obj);
        m_queueNotEmptyEvent.Set(notifyAll);
    }

    /**
     * @brief Writes an object to the queue.
     * @param obj The object to move into the queue.
     * @param notifyAll true notifies all waiting threads. false notifies one thread.
     */
    void Offer(T &&obj, bool notifyAll = true) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.push(obj);
        m_queueNotEmptyEvent.Set(notifyAll);
    }

    /**
     * @brief Retrieves and removes an object from the queue, waiting if necessary.
     * @param obj The object to copy into.
     * @return Always true.
     */
    bool Poll(T &obj) {
        m_queueNotEmptyEvent.Wait(false);
        std::lock_guard<std::mutex> lock(m_mutex);
        obj = m_queue.front();
        m_queue.pop();
        if (m_queue.empty()) {
            m_queueNotEmptyEvent.Reset();
        }
        return true;
    }
    /**
     * @brief Retrieves and removes an object from the queue, waiting if necessary.
     * @param obj The object to move into.
     * @return Always true.
     */
    bool Poll(T &&obj) {
        m_queueNotEmptyEvent.Wait(false);
        std::lock_guard<std::mutex> lock(m_mutex);
        obj = m_queue.front();
        m_queue.pop();
        if (m_queue.empty()) {
            m_queueNotEmptyEvent.Reset();
        }
        return true;
    }

    /**
     * @brief Attempts to retrieve an object from the queue. If the queue is empty, no object is retrieved and the
     * method returns immediately. The return value indicates if an object was retrieved from the queue. This method
     * does not wait when the queue is empty.
     * @param obj The object to copy into.
     * @return true if an object was retrieved, false if not.
     */
    bool TryPoll(T &obj) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_queue.empty()) {
            return false;
        }
        obj = m_queue.front();
        m_queue.pop();
        if (m_queue.empty()) {
            m_queueNotEmptyEvent.Reset();
        }
        return true;
    }

    /**
     * @brief Attempts to retrieve an object from the queue. If the queue is empty, no object is retrieved and the
     * method returns immediately. The return value indicates if an object was retrieved from the queue. This method
     * does not wait when the queue is empty.
     * @param obj The object to move into.
     * @return true if an object was retrieved, false if not.
     */
    bool TryPoll(T &&obj) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_queue.empty()) {
            return false;
        }
        obj = m_queue.front();
        m_queue.pop();
        if (m_queue.empty()) {
            m_queueNotEmptyEvent.Reset();
        }
        return true;
    }

    /**
     * @brief Clears the queue.
     */
    void Clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        while (!m_queue.empty()) {
            m_queue.pop();
        }
        m_queueNotEmptyEvent.Reset();
    }

private:
    std::queue<T> m_queue;
    std::mutex m_mutex;
    Event m_queueNotEmptyEvent;
};

} // namespace util
