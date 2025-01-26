#include <satemu/hw/scsp/scsp_dsp.hpp>

namespace satemu::scsp {

DSP::DSP() {
    Reset();
}

void DSP::Reset() {
    program.fill({.u64 = 0});
    temp.fill(0);
    soundMem.fill(0);
    coeffs.fill(0);
    addrs.fill(0);
    mixStack.fill(0);
    effectOut.fill(0);
    audioIn.fill(0);

    ringBufferLeadAddress = 0;
    ringBufferLength = 0;
}

} // namespace satemu::scsp
