#pragma once

#include <functional>
#include <vector>

namespace util {

// An observable type stores a value of type `T` and allows other objects to observe and react to changes.
//
// Observer functions run immediately after they're added to the observer and on the act of changing the value.
// Be aware of this behavior to handle multithreaded scenarios properly.
template <typename T>
class Observable {
public:
    // The observer function type
    using Observer = void(const T &);

    Observable(const T &value)
        : m_value(value) {}

    Observable(T &&value) {
        std::swap(value, m_value);
    }

    Observable() = default;
    Observable(const Observable &) = delete;
    Observable(Observable &&) = default;

    Observable &operator=(const Observable &) = delete;
    Observable &operator=(Observable &&) = default;

    Observable &operator=(T value) {
        m_value = value;
        for (auto &observer : m_observers) {
            observer(value);
        }
        return *this;
    }

    T *operator->() {
        return &m_value;
    }
    const T *operator->() const {
        return &m_value;
    }

    T &operator*() {
        return m_value;
    }
    const T &operator*() const {
        return m_value;
    }

    // Adds an observer to this observable.
    // The observer is immediately invoked with the current value.
    void Observe(std::function<Observer> &&observer) {
        auto &fn = m_observers.emplace_back(std::move(observer));
        fn(m_value);
    }

    // Gets the current value.
    T Get() const {
        return m_value;
    }

    operator T() const {
        return m_value;
    }

private:
    T m_value;

    std::vector<std::function<Observer>> m_observers;
};

} // namespace util
