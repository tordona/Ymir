#pragma once

#include <satemu/util/inline.hpp>

#include <memory>

namespace satemu::debug {

// Interface for debug probes.
struct IProbe {
    virtual ~IProbe() = default;

    virtual void test() = 0;
};

// Holds a probe and simplifies probe usage.
struct ProbeContext {
    // Instantiates the specified probe with the arguments passed to its constructor.
    template <std::derived_from<debug::IProbe> T, typename... Args>
    void Use(Args &&...args) {
        m_probe = std::make_unique<T>(std::forward<Args>(args)...);
    }

    // Frees the probe.
    void Clear() {
        m_probe.reset();
    }

    // -----------------------------------------------------------------------------------------------------------------
    // IProbe method wrappers
    //
    // The goal of these wrappers is to reduce the amount of boilerplate code necessary to invoke probe methods.
    // Every method in IProbe must have a corresponding method here.
    // Methods must take a `bool debug` template parameter and perform a no-op or identity operation if false.
    // Methods must also null-check `m_probe` before invoking the function.
    // If the target function returns a value, it must be returned unmodified.

    template <bool debug>
    void test() {
        if constexpr (debug) {
            if (m_probe) {
                return m_probe->test();
            }
        }
    }

private:
    std::unique_ptr<IProbe> m_probe;
};

} // namespace satemu::debug
