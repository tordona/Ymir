#pragma once

#include <satemu/core/types.hpp>

namespace satemu::debug {

// Interface for SH2 tracers.
//
// Must be implemented by users of the core library.
//
// Attach to an instance of `satemu::sh2::SH2` with its `UseTracer(ISH2Tracer *)` method.
//
// This tracer requires the emulator to execute in debug mode.
struct ISH2Tracer {
    virtual ~ISH2Tracer() = default;

    // Invoked when an SH2 CPU handles an interrupt.
    virtual void Interrupt(uint8 vecNum, uint8 level, uint32 pc) {}

    // Invoked when an SH2 CPU handles an exception.
    virtual void Exception(uint8 vecNum, uint32 pc, uint32 sr) {}
};

} // namespace satemu::debug
