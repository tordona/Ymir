#pragma once

#include <satemu/util/inline.hpp>

#include <memory>

namespace satemu::debug {

// Interface for debug tracers - objects that receive internal data from the emulator while it is executing.
struct ITracer {
    virtual ~ITracer() = default;
};

// Holds a tracer and simplifies tracer usage.
struct TracerContext {
    // Instantiates the specified tracer with the arguments passed to its constructor.
    template <std::derived_from<debug::ITracer> T, typename... Args>
    void Use(Args &&...args) {
        m_tracer = std::make_unique<T>(std::forward<Args>(args)...);
    }

    // Frees the tracer.
    void Clear() {
        m_tracer.reset();
    }

    // -----------------------------------------------------------------------------------------------------------------
    // ITracer method wrappers
    //
    // The goal of these wrappers is to reduce the amount of boilerplate code necessary to invoke tracer methods.
    // Every method in ITracer must have a corresponding method here.
    // Methods must take a `bool debug` template parameter and perform a no-op or identity operation if false.
    // Methods must also null-check `m_tracer` before invoking the function.
    // If the target function returns a value, it must be returned unmodified.

    template <bool debug>
    void test() {
        if constexpr (debug) {
            if (m_tracer) {
                // return m_tracer->test();
            }
        }
    }

private:
    std::unique_ptr<ITracer> m_tracer;
};

} // namespace satemu::debug
