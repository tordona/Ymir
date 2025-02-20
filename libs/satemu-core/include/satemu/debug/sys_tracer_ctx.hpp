#pragma once

#include "sys_tracer.hpp"

#include <concepts>

namespace satemu::debug {

// Holds a system tracer and simplifies tracer usage.
struct SystemTracerContext {
    // Makes the context use the specified tracer.
    // Set to nullptr to disable tracing for this component.
    void Use(ISystemTracer *tracer) {
        m_tracer = tracer;
    }

private:
    ISystemTracer *m_tracer;
};

} // namespace satemu::debug
