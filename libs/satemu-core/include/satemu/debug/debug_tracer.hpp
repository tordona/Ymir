#pragma once

#include <satemu/util/inline.hpp>

#include <memory>

namespace satemu::debug {

// Interface for debug tracers - objects that receive internal state from the emulator while it is executing.
//
// Must be implemented by users of the core library and instantiated with the `Use` method of the `TracerContext`
// instance in `satemu::Saturn`.
struct ITracer {
    virtual ~ITracer() = default;

    // Invoked when an SH2 CPU handles an interrupt.
    virtual void SH2_Interrupt(bool master, uint8 vecNum, uint8 level) = 0;
};

// ---------------------------------------------------------------------------------------------------------------------

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
    // In short, these wrappers should look like this:
    //
    // template <bool debug>
    // <return-type> method(<args>) {
    //     if constexpr (debug) {
    //         if (m_tracer) {
    //             return m_tracer->method(<args>);
    //         }
    //     }
    // }

    template <bool debug>
    void SH2_Interrupt(bool master, uint8 vecNum, uint8 level) {
        if constexpr (debug) {
            if (m_tracer) {
                return m_tracer->SH2_Interrupt(master, vecNum, level);
            }
        }
    }

private:
    std::unique_ptr<ITracer> m_tracer;
};

} // namespace satemu::debug
