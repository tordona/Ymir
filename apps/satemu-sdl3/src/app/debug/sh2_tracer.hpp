#pragma once

#include <satemu/debug/sh2_tracer.hpp>

#include <util/ring_buffer.hpp>

namespace app {

struct SH2Tracer final : public satemu::debug::ISH2Tracer {
    void ExecuteInstruction(uint32 pc, uint16 opcode, bool delaySlot) final;
    void Interrupt(uint8 vecNum, uint8 level, satemu::sh2::InterruptSource source, uint32 pc) final;
    void Exception(uint8 vecNum, uint32 pc, uint32 sr) final;

    bool traceInstructions = false;
    bool traceInterrupts = false;
    bool traceExceptions = false;

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
    };

    struct ExceptionInfo {
        uint8 vecNum;
        uint32 pc;
        uint32 sr;
    };

    util::RingBuffer<InstructionInfo, 16384> instructions;
    util::RingBuffer<InterruptInfo, 1024> interrupts;
    util::RingBuffer<ExceptionInfo, 1024> exceptions;
};

} // namespace app
