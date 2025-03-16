#include "sh2_tracer.hpp"

namespace app {

void SH2Tracer::Interrupt(uint8 vecNum, uint8 level, uint32 pc) {
    interrupts[interruptsPos++] = {vecNum, level, pc};
    if (interruptsPos >= interrupts.size()) {
        interruptsPos = 0;
    }
    if (interruptsCount < interrupts.size()) {
        interruptsCount++;
    }
}

void SH2Tracer::Exception(uint8 vecNum, uint32 pc, uint32 sr) {
    exceptions[exceptionsPos++] = {vecNum, pc, sr};
    if (exceptionsPos >= exceptions.size()) {
        exceptionsPos = 0;
    }
    if (exceptionsCount < exceptions.size()) {
        exceptionsCount++;
    }
}

} // namespace app
