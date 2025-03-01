#pragma once

#include <satemu/util/inline.hpp>

#include "sh2_tracer.hpp"

namespace satemu::debug {

// Holds an SH2 tracer and simplifies tracer usage.
struct SH2TracerContext {
    // Makes the context use the specified tracer.
    // Set to nullptr to disable tracing for this component.
    void Use(ISH2Tracer *tracer) {
        m_tracer = tracer;
    }

    template <bool debug>
    FORCE_INLINE void Interrupt(uint8 vecNum, uint8 level) {
        if constexpr (debug) {
            if (m_tracer) {
                return m_tracer->Interrupt(vecNum, level);
            }
        }
    }

    template <bool debug>
    FORCE_INLINE void Exception(uint8 vecNum, uint32 pc, uint32 sr) {
        if constexpr (debug) {
            if (m_tracer) {
                return m_tracer->Exception(vecNum, pc, sr);
            }
        }
    }

private:
    ISH2Tracer *m_tracer = nullptr;
};

} // namespace satemu::debug
