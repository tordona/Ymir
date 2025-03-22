#pragma once

#include <satemu/debug/sh2_tracer.hpp>

namespace app {

struct SH2Tracer final : public satemu::debug::ISH2Tracer {
    void ExecuteInstruction(uint32 pc, uint16 opcode, bool delaySlot) final;
    void Interrupt(uint8 vecNum, uint8 level, uint32 pc) final;
    void Exception(uint8 vecNum, uint32 pc, uint32 sr) final;

    struct InstructionInfo {
        uint32 pc;
        uint16 opcode;
        bool delaySlot;
    };

    struct InterruptInfo {
        uint8 vecNum;
        uint8 level;
        uint32 pc;
    };

    struct ExceptionInfo {
        uint8 vecNum;
        uint32 pc;
        uint32 sr;
    };

    std::array<InstructionInfo, 16> instructions;
    size_t instructionsPos = 0;
    size_t instructionsCount = 0;

    std::array<InterruptInfo, 16> interrupts;
    size_t interruptsPos = 0;
    size_t interruptsCount = 0;

    std::array<ExceptionInfo, 16> exceptions;
    size_t exceptionsPos = 0;
    size_t exceptionsCount = 0;
};

} // namespace app
