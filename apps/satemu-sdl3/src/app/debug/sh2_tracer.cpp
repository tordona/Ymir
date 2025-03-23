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

} // namespace app
