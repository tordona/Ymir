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

    // Invoked when a byte is writted to mednafen's debug port.
    virtual void DebugPortWrite(uint8 ch) {}

    // Invoked when a DSP DMA transfer begins.
    //
    // `toD0` indicates the direction of the transfer: from DSP to D0 (true) or from D0 to DSP (false).
    // `addrD0` is the address on the D0 bus.
    // `addrDSP` is the addres on the DSP: 0-3 for Data RAM banks 0-3, 4 for Program RAM.
    // `count` is the number of longword transfers to be performed.
    // `addrInc` is the D0 address increment per transfer.
    // `hold` indicates if the D0 address will be updated (false) or not (true) after the transfer.
    virtual void DSPDMA(bool toD0, uint32 addrD0, uint8 addrDSP, uint8 count, uint8 addrInc, bool hold) {}
};

} // namespace satemu::debug
