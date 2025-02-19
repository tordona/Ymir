#pragma once

#include <satemu/debug/debug_tracer.hpp>

namespace satemu::debug {

struct ISH2Tracer {
    // Invoked when an SH2 CPU handles an interrupt.
    virtual void Interrupt(uint8 vecNum, uint8 level) = 0;
};

template <bool debug>
void TracerContext::SH2_Interrupt(bool master, uint8 vecNum, uint8 level) {
    if constexpr (debug) {
        if (m_tracer) {
            auto &sh2Tracer = master ? m_tracer->GetMasterSH2Tracer() : m_tracer->GetSlaveSH2Tracer();
            return sh2Tracer.Interrupt(vecNum, level);
        }
    }
}

} // namespace satemu::debug
