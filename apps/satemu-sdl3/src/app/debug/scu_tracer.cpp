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

} // namespace app
