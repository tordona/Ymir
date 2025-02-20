#pragma once

#include <satemu/debug/debug_tracer.hpp>

#include <satemu/core/types.hpp>

namespace satemu::debug {

struct ISH2Tracer {
    // Invoked when an SH2 CPU handles an interrupt.
    virtual void Interrupt(uint8 vecNum, uint8 level) = 0;
};

struct TracerContext::SH2 {
    SH2(bool master)
        : master(master) {}

    ISH2Tracer *tracer = nullptr;
    bool master;

    template <bool debug>
    void Interrupt(uint8 vecNum, uint8 level) {
        if constexpr (debug) {
            if (tracer) {
                return tracer->Interrupt(vecNum, level);
            }
        }
    }
};

} // namespace satemu::debug
