#pragma once

#include <satemu/util/inline.hpp>

#include "scu_tracer.hpp"

namespace satemu::debug {

// Holds an SCU tracer and simplifies tracer usage.
struct SCUTracerContext {
    // Makes the context use the specified tracer.
    // Set to nullptr to disable tracing for this component.
    void Use(ISCUTracer *tracer) {
        m_tracer = tracer;
    }

    FORCE_INLINE void RaiseInterrupt(uint8 index, uint8 level) {
        if (m_tracer) {
            return m_tracer->RaiseInterrupt(index, level);
        }
    }

    FORCE_INLINE void AcknowledgeInterrupt(uint8 index) {
        if (m_tracer) {
            return m_tracer->AcknowledgeInterrupt(index);
        }
    }

private:
    ISCUTracer *m_tracer = nullptr;
};

} // namespace satemu::debug
