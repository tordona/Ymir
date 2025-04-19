#pragma once

#include <condition_variable>
#include <mutex>

namespace util {

// An event suspends threads that wait on it until it is signaled.
class Event {
public:
    explicit Event(bool set = false)
        : m_set(set) {}

    void Wait(bool autoReset) {
        std::unique_lock<std::mutex> lock(m_mutex);
        while (!m_set) {
            m_condVar.wait(lock);
        }
        if (autoReset) {
            m_set = false;
        }
    }

    void Set(bool notifyAll = true) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_set = true;
        if (notifyAll) {
            m_condVar.notify_all();
        } else {
            m_condVar.notify_one();
        }
    }

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
