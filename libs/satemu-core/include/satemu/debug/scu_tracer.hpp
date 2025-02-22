#pragma once

#include <satemu/core/types.hpp>

namespace satemu::debug {

// Interface for SCU tracers.
//
// Must be implemented by users of the core library and instantiated with the `Use` method of the `SCUTracerContext`
// instance in `satemu::scu::SCU`.
struct ISCUTracer {
    virtual ~ISCUTracer() = default;

    // Invoked when the SCU raises an interrupt.
    virtual void RaiseInterrupt(uint8 index, uint8 level) = 0;

    // Invoked when the SCU acknowledges an interrupt.
    virtual void AcknowledgeInterrupt(uint8 index) = 0;
};

} // namespace satemu::debug
