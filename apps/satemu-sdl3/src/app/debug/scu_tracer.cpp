#include "scu_tracer.hpp"

namespace app {

void SCUTracer::RaiseInterrupt(uint8 index, uint8 level) {
    PushInterrupt({index, level});
}

void SCUTracer::AcknowledgeInterrupt(uint8 index) {
    PushInterrupt({index, 0xFF});
}

void SCUTracer::PushInterrupt(InterruptInfo info) {
    interrupts[interruptsPos++] = info;
    if (interruptsPos >= interrupts.size()) {
        interruptsPos = 0;
    }
    if (interruptsCount < interrupts.size()) {
        interruptsCount++;
    }
}

} // namespace app
