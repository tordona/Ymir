#pragma once

#include <satemu/debug/sh2_tracer.hpp>

#include <util/ring_buffer.hpp>

namespace app {

struct SH2Tracer final : public satemu::debug::ISH2Tracer {
    void ExecuteInstruction(uint32 pc, uint16 opcode, bool delaySlot) final;
    void Interrupt(uint8 vecNum, uint8 level, satemu::sh2::InterruptSource source, uint32 pc) final;
    void Exception(uint8 vecNum, uint32 pc, uint32 sr) final;
    void Begin32x32Division(sint32 dividend, sint32 divisor, bool overflowIntrEnable) final;
    void End32x32Division(sint32 quotient, sint32 remainder, bool overflow) final;
    void Begin64x32Division(sint64 dividend, sint32 divisor, bool overflowIntrEnable) final;
    void End64x32Division(sint32 quotient, sint32 remainder, bool overflow) final;

    bool traceInstructions = false;
    bool traceInterrupts = false;
    bool traceExceptions = false;
    bool traceDivisions = false;

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

    struct Division32x32Info {
        sint32 dividend;
        sint32 divisor;
        sint32 quotient;
        sint32 remainder;
        bool overflow;
        bool overflowIntrEnable;
        bool finished;
        uint32 counter;
    };

    struct Division64x32Info {
        sint64 dividend;
        sint32 divisor;
        sint32 quotient;
        sint32 remainder;
        bool overflow;
        bool overflowIntrEnable;
        bool finished;
        uint32 counter;
    };

    util::RingBuffer<InstructionInfo, 16384> instructions;
    util::RingBuffer<InterruptInfo, 1024> interrupts;
    util::RingBuffer<ExceptionInfo, 1024> exceptions;
    util::RingBuffer<Division32x32Info, 1024> divisions32;
    util::RingBuffer<Division64x32Info, 1024> divisions64;

private:
    uint32 m_interruptCounter = 0;
    uint32 m_division32Counter = 0;
    uint32 m_division64Counter = 0;
};

} // namespace app
