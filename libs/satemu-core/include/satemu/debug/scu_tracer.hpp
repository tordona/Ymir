#pragma once

#include <satemu/core/types.hpp>

namespace satemu::debug {

// Interface for SCU tracers.
//
// Must be implemented by users of the core library.
//
// Attach to an instance of `satemu::scu::SCU` with its `UseTracer(ISCUTracer *)` method.
struct ISCUTracer {
    virtual ~ISCUTracer() = default;

    // Invoked when the SCU raises an interrupt.
    virtual void RaiseInterrupt(uint8 index, uint8 level) {}

    // Invoked when the SCU acknowledges an interrupt.
    virtual void AcknowledgeInterrupt(uint8 index) {}
};

} // namespace satemu::debug
