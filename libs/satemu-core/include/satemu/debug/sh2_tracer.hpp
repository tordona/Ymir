#pragma once

#include <satemu/core/types.hpp>

namespace satemu::debug {

// Interface for SH2 tracers.
//
// Must be implemented by users of the core library and instantiated with the `Use` method of the `SH2TracerContext`
// instance in `satemu::sh2::SH2`.
struct ISH2Tracer {
    virtual ~ISH2Tracer() = default;

    // Invoked when an SH2 CPU handles an interrupt.
    virtual void Interrupt(uint8 vecNum, uint8 level, uint32 pc) = 0;

    // Invoked when an SH2 CPU handles an exception.
    virtual void Exception(uint8 vecNum, uint32 pc, uint32 sr) = 0;
};

} // namespace satemu::debug
