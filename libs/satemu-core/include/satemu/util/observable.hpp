#pragma once

#include <cstdint>
#include <functional>
#include <type_traits>
#include <vector>

namespace util {

// An observable type stores a value of type `T` and allows other objects to observe and react to changes.
//
// Observer functions run immediately after they're added to the observer and on the act of changing the value.
// Be aware of this behavior to handle multithreaded scenarios properly.
template <typename T>
class Observable {
public:
    // The observer function type.
    // The value is passed by value if it fits in a pointer/register, otherwise it is passed by const reference.
    using Observer = std::conditional_t<sizeof(T) <= sizeof(uintptr_t), void(T), void(const T &)>;

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
        Notify();
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
    void Observe(std::function<Observer> &&observer) {
        m_fnObservers.emplace_back(std::move(observer));
    }

    // Adds a simple observer to this observable that copies the value to the given reference.
    // Also copies the current value to the given reference.
    void Observe(T &valueRef) {
        m_valObservers.emplace_back(&valueRef);
        valueRef = m_value;
    }

    // Notifies all observers of the current value.
    void Notify() {
        for (auto &observer : m_fnObservers) {
            observer(m_value);
        }
        for (auto *observer : m_valObservers) {
            *observer = m_value;
        }
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

    std::vector<std::function<Observer>> m_fnObservers;
    std::vector<T *> m_valObservers;
};

} // namespace util
