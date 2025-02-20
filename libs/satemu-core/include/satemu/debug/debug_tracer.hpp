#pragma once

#include <satemu/util/inline.hpp>

#include <memory>

namespace satemu::debug {

// -----------------------------------------------------------------------------------------------------------------
// Forward declarations

// Interfaces for debug tracers for each component

struct ISH2Tracer;
// struct IVDPTracer;

// -----------------------------------------------------------------------------------------------------------------

// Interface for debug tracers - objects that receive internal state from the emulator while it is executing.
//
// Must be implemented by users of the core library and instantiated with the `Use` method of the `TracerContext`
// instance in `satemu::Saturn`.
struct ITracer {
    virtual ~ITracer() = default;

    virtual ISH2Tracer &GetMasterSH2Tracer() = 0;
    virtual ISH2Tracer &GetSlaveSH2Tracer() = 0;

    // virtual IVDPTracer &GetVDPTracer() = 0;
};

// ---------------------------------------------------------------------------------------------------------------------

// Holds a tracer and simplifies tracer usage.
struct TracerContext {
    TracerContext();
    ~TracerContext();

    // Instantiates the specified tracer with the arguments passed to its constructor.
    template <std::derived_from<debug::ITracer> T, typename... Args>
    void Use(Args &&...args) {
        m_tracer = std::make_unique<T>(std::forward<Args>(args)...);
        UpdateContexts();
    }

    // Frees the tracer.
    void Clear() {
        m_tracer.reset();
        UpdateContexts();
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

    struct SH2;

    SH2 &GetMasterSH2Tracer() {
        return *m_masterSH2;
    }

    SH2 &GetSlaveSH2Tracer() {
        return *m_slaveSH2;
    }

private:
    std::unique_ptr<ITracer> m_tracer;

    std::unique_ptr<SH2> m_masterSH2;
    std::unique_ptr<SH2> m_slaveSH2;

    void UpdateContexts();
};

} // namespace satemu::debug
