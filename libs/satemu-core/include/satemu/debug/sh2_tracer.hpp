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

    // Invoked immediately before executing an instruction.
    //
    // `pc` is the current program counter
    // `opcode` is the instruction opcode
    // `delaySlot` indicates if the instruction is executing in a delay slot
    virtual void ExecuteInstruction(uint32 pc, uint16 opcode, bool delaySlot) {}

    // Invoked when the CPU handles an interrupt.
    //
    // `vecNum` is the interrupt vector number
    // `level` is the interrupt level (priority)
    // `source` is the interrupt source
    // `pc` is the value of PC at the moment the interrupt was handled
    virtual void Interrupt(uint8 vecNum, uint8 level, sh2::InterruptSource source, uint32 pc) {}

    // Invoked when the CPU handles an exception.
    //
    // `vecNum` is the exception vector number
    // `pc` is the value of PC at the moment the exception was handled
    // `sr` is the value of SR at the moment the exception was handled
    virtual void Exception(uint8 vecNum, uint32 pc, uint32 sr) {}

    // Invoked when a 32-bit by 32-bit division begins.
    //
    // `dividend` is the value of the dividend (DVDNTL)
    // `divisor` is the value of the divisor (DVSR)
    // `overflowIntrEnable` indicates if the division overflow interrupt is enabled (DVCR.OVFIE)
    virtual void Begin32x32Division(sint32 dividend, sint32 divisor, bool overflowIntrEnable) {}

    // Invoked when a 64-bit by 32-bit division begins.
    //
    // `dividend` is the value of the dividend (DVDNTH:DVDNTL)
    // `divisor` is the value of the divisor (DVSR)
    // `overflowIntrEnable` indicates if the division overflow interrupt is enabled (DVCR.OVFIE)
    virtual void Begin64x32Division(sint64 dividend, sint32 divisor, bool overflowIntrEnable) {}

    // Invoked when a division ends.
    //
    // `quotient` is the resulting quotient (DVDNTL)
    // `remainder` is the resulting remainder (DVDNTH)
    // `overflow` indicates if the division resulted in an overflow
    virtual void EndDivision(sint32 quotient, sint32 remainder, bool overflow) {}

    // Invoked when a DMA transfer begins.
    //
    // `channel` is the DMAC channel number, either 0 or 1.
    // `srcAddress` is the starting source address of the transfer.
    // `dstAddress` is the starting destination address of the transfer.
    // `count` is the number of transfer units to be performed. For 16-byte transfers, this number decrements once per
    // 32-bit transfer.
    // `unitSize` is the size of a single unit of transfer: 1, 2, 4 or 16.
    // `srcInc` indicates the source address increment per unit of transfer.
    // `dstInc` indicates the destination address increment per unit of transfer.
    virtual void DMAXferBegin(uint32 channel, uint32 srcAddress, uint32 dstAddress, uint32 count, uint32 unitSize,
                              sint32 srcInc, sint32 dstInc) {}

    // Invoked when a DMA channel transfers one unit of data.
    // For 16-byte transfers, this function is invoked once per 32-bit transfer with a `unitSize` of 16.
    //
    // `channel` is the DMAC channel number, either 0 or 1.
    // `srcAddress` is the source address of the transfer.
    // `dstAddress` is the destination address of the transfer.
    // `data` is the data transferred.
    // `unitSize` is the size of a single unit of transfer: 1, 2, 4 or 16.
    virtual void DMAXferData(uint32 channel, uint32 srcAddress, uint32 dstAddress, uint32 data, uint32 unitSize) {}

    // Invoked when a DMA transfer finishes.
    //
    // `channel` is the DMAC channel number, either 0 or 1.
    // `irqRaised` indicates if the channel's transfer end interrupt signal was raised.
    virtual void DMAXferEnd(uint32 channel, bool irqRaised) {}
};

} // namespace satemu::debug
