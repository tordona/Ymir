#include "sh2_tracer.hpp"

using namespace satemu;

namespace app {

void SH2Tracer::ExecuteInstruction(uint32 pc, uint16 opcode, bool delaySlot) {
    if (!traceInstructions) {
        return;
    }

    instructions.Write({pc, opcode, delaySlot});
}

void SH2Tracer::Interrupt(uint8 vecNum, uint8 level, sh2::InterruptSource source, uint32 pc) {
    if (!traceInterrupts) {
        return;
    }

    interrupts.Write({vecNum, level, source, pc, m_interruptCounter++});
}

void SH2Tracer::Exception(uint8 vecNum, uint32 pc, uint32 sr) {
    if (!traceExceptions) {
        return;
    }

    exceptions.Write({vecNum, pc, sr});
}

void SH2Tracer::Begin32x32Division(sint32 dividend, sint32 divisor, bool overflowIntrEnable) {
    if (!traceDivisions) {
        return;
    }

    divisions32.Write({.dividend = dividend,
                       .divisor = divisor,
                       .overflowIntrEnable = overflowIntrEnable,
                       .finished = false,
                       .counter = m_division32Counter++});
}

void SH2Tracer::End32x32Division(sint32 quotient, sint32 remainder, bool overflow) {
    if (!traceDivisions) {
        return;
    }

    auto &div = divisions32.GetLast();
    div.quotient = quotient;
    div.remainder = remainder;
    div.overflow = overflow;
    div.finished = true;
}

void SH2Tracer::Begin64x32Division(sint64 dividend, sint32 divisor, bool overflowIntrEnable) {
    if (!traceDivisions) {
        return;
    }

    divisions64.Write({.dividend = dividend,
                       .divisor = divisor,
                       .overflowIntrEnable = overflowIntrEnable,
                       .finished = false,
                       .counter = m_division64Counter++});
}

void SH2Tracer::End64x32Division(sint32 quotient, sint32 remainder, bool overflow) {
    if (!traceDivisions) {
        return;
    }

    auto &div = divisions64.GetLast();
    div.quotient = quotient;
    div.remainder = remainder;
    div.overflow = overflow;
    div.finished = true;
}

} // namespace app
