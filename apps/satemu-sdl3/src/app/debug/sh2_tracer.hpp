#pragma once

#include <satemu/debug/sh2_tracer.hpp>

#include <util/ring_buffer.hpp>

namespace app {

struct SH2Tracer final : satemu::debug::ISH2Tracer {
    void ResetInterruptCounter() {
        m_interruptCounter = 0;
    }

    void ResetDivisionCounter() {
        m_divisionCounter = 0;
    }

    bool traceInstructions = false;
    bool traceInterrupts = false;
    bool traceExceptions = false;
    bool traceDivisions = false;
    bool traceDMA = false;

    struct InstructionInfo {
        uint32 pc;
        uint16 opcode;
        bool delaySlot;
    };

    struct InterruptInfo {
        uint8 vecNum;
        uint8 level;
        satemu::sh2::InterruptSource source;
        uint32 pc;
        uint32 counter;
    };

    struct ExceptionInfo {
        uint8 vecNum;
        uint32 pc;
        uint32 sr;
    };

    struct DivisionInfo {
        sint64 dividend;
        sint32 divisor;
        sint32 quotient;
        sint32 remainder;
        bool overflow;
        bool overflowIntrEnable;
        bool finished;
        bool div64;
        uint32 counter;
    };

    util::RingBuffer<InstructionInfo, 16384> instructions;
    util::RingBuffer<InterruptInfo, 1024> interrupts;
    util::RingBuffer<ExceptionInfo, 1024> exceptions;
    util::RingBuffer<DivisionInfo, 1024> divisions;

    struct DivisionStatistics {
        uint64 div32s = 0;
        uint64 div64s = 0;
        uint64 overflows = 0;
        uint64 interrupts = 0;

        void Clear() {
            div32s = 0;
            div64s = 0;
            overflows = 0;
            interrupts = 0;
        }
    } divStats;

private:
    uint32 m_interruptCounter = 0;
    uint32 m_divisionCounter = 0;

    // -------------------------------------------------------------------------
    // ISH2Tracer implementation

    void ExecuteInstruction(uint32 pc, uint16 opcode, bool delaySlot) final;
    void Interrupt(uint8 vecNum, uint8 level, satemu::sh2::InterruptSource source, uint32 pc) final;
    void Exception(uint8 vecNum, uint32 pc, uint32 sr) final;

    void Begin32x32Division(sint32 dividend, sint32 divisor, bool overflowIntrEnable) final;
    void Begin64x32Division(sint64 dividend, sint32 divisor, bool overflowIntrEnable) final;
    void EndDivision(sint32 quotient, sint32 remainder, bool overflow) final;

    void DMAXferBegin(uint32 channel, uint32 srcAddress, uint32 dstAddress, uint32 count, uint32 unitSize,
                      sint32 srcInc, sint32 dstInc) final;
    void DMAXferData(uint32 channel, uint32 srcAddress, uint32 dstAddress, uint32 data, uint32 unitSize) final;
    void DMAXferEnd(uint32 channel, bool irqRaised) final;
};

} // namespace app
