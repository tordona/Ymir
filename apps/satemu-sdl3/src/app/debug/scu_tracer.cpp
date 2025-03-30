#include "scu_tracer.hpp"

namespace app {

void SCUTracer::PushInterrupt(InterruptInfo info) {
    interrupts.Write(info);
}

void SCUTracer::RaiseInterrupt(uint8 index, uint8 level) {
    PushInterrupt({index, level});
}

void SCUTracer::AcknowledgeInterrupt(uint8 index) {
    PushInterrupt({index, 0xFF});
}

void SCUTracer::DebugPortWrite(uint8 ch) {
    if (ch == '\n') {
        debugMessages.Write(m_debugMessageBuffer);
        m_debugMessageBuffer.clear();
    } else if (ch != '\r') {
        m_debugMessageBuffer.push_back(ch);
    }
}

void SCUTracer::BeginDSPDMA(bool toD0, uint32 addrD0, uint8 addrDSP, uint8 count, uint8 addrInc, bool hold, uint8 pc) {
    // TODO
}

void SCUTracer::DSPDMATransfer(uint32 addrD0, uint8 offsetDSP, uint32 value) {
    // TODO
}

void SCUTracer::EndDSPDMA() {
    // TODO
}

} // namespace app
