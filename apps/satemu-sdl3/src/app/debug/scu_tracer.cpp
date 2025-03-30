#include "scu_tracer.hpp"

namespace app {

void SCUTracer::RaiseInterrupt(uint8 index, uint8 level) {
    if (!traceInterrupts) {
        return;
    }

    interrupts.Write({m_interruptCounter++, index, level});
}

void SCUTracer::AcknowledgeInterrupt(uint8 index) {
    if (!traceInterrupts) {
        return;
    }

    interrupts.Write({m_interruptCounter++, index, 0xFF});
}

void SCUTracer::DebugPortWrite(uint8 ch) {
    if (ch == '\n') {
        debugMessages.Write(m_debugMessageBuffer);
        m_debugMessageBuffer.clear();
    } else if (ch != '\r') {
        m_debugMessageBuffer.push_back(ch);
    }
}

void SCUTracer::DSPDMA(bool toD0, uint32 addrD0, uint8 addrDSP, uint8 count, uint8 addrInc, bool hold) {
    if (!traceDSPDMA) {
        return;
    }

    dspDmaTransfers.Write({m_dspDmaCounter++, addrD0, addrDSP, count, addrInc, toD0, hold});
}

} // namespace app
