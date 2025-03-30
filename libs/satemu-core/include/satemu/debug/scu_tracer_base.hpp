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
    //
    // `index` is the interrupt index. See documentation for `scu::InterruptStatus`.
    // `level` is the interrupt level.
    virtual void RaiseInterrupt(uint8 index, uint8 level) {}

    // Invoked when the SCU acknowledges an interrupt.
    //
    // `index` is the interrupt index. See documentation for `scu::InterruptStatus`.
    virtual void AcknowledgeInterrupt(uint8 index) {}

    // Invoked when a byte is writted to mednafen's debug port.
    //
    // `ch` is the character written to the port.
    virtual void DebugPortWrite(uint8 ch) {}

    // Invoked when a DMA transfer begins.
    // Also invoked on every indirect transfer entry.
    //
    // `channel` is the channel index.
    // `srcAddr` is the starting source address.
    // `dstAddr` is the starting destination address.
    // `xferCount` is the number of bytes to be transferred.
    // `srcAddrInc` is the source address increment per transfer.
    // `dstAddrInc` is the destination address increment per transfer.
    // `indirect` indicates if this is a direct (false) or indirect transfer (true).
    // `indirectAddr` is the address of the indirect transfer data.
    virtual void DMA(uint8 channel, uint32 srcAddr, uint32 dstAddr, uint32 xferCount, uint32 srcAddrInc,
                     uint32 dstAddrInc, bool indirect, uint32 indirectAddr) {}

    // -----------------------------------------------------------------------------------------------------------------
    // DSP

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
