#pragma once

#include <satemu/core/types.hpp>

#include <satemu/hw/sh2/sh2_intc.hpp>

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

    // Invoked immediately before the SH2 CPU executes an instruction.
    //
    // `pc` is the current program counter
    // `opcode` is the instruction opcode
    // `delaySlot` indicates if the instruction is executing in a delay slot
    virtual void ExecuteInstruction(uint32 pc, uint16 opcode, bool delaySlot) {}

    // Invoked when the SH2 CPU handles an interrupt.
    virtual void Interrupt(uint8 vecNum, uint8 level, sh2::InterruptSource source, uint32 pc) {}

    // Invoked when the SH2 CPU handles an exception.
    virtual void Exception(uint8 vecNum, uint32 pc, uint32 sr) {}
};

} // namespace satemu::debug
