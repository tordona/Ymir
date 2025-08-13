#include "scsp_tracer.hpp"

namespace app {

void SCSPTracer::SlotSample(uint32 index, sint16 output) {
    slotOutputs[index].Write(output);
}

} // namespace app
