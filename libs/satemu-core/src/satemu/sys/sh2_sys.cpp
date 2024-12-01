#include <satemu/sys/sh2_sys.hpp>

#include "sh2/sh2_interpreter.hpp"
#include "sh2/sh2_mem.hpp"

using namespace satemu::sh2;

namespace satemu::sys {

SH2System::SH2System(scu::SCU &scu, smpc::SMPC &smpc)
    : SH2(scu, smpc) {}

void SH2System::Reset(bool hard) {
    auto reset = [&](SH2State &state) {
        state.Reset(hard);
        state.PC = MemReadLong(state, SH2.bus, state.VBR);
        state.R[15] = MemReadLong(state, SH2.bus, state.VBR + 4);
    };

    reset(SH2.masterState);
    reset(SH2.slaveState);
}

void SH2System::Step() {
    // TODO: allow switching between interpreter and JIT recompiler
    sh2::interp::Step(SH2.masterState, SH2.bus);
    // TODO: step slave SH2 if enabled
    // sh2::interp::Step(SH2.slaveState, SH2.bus);
}

void SH2System::SetInterruptLevel(uint8 level) {
    SH2.masterState.SetInterruptLevel(level);
    // TODO: when to set slave IRL?
    // SH2.slaveState.SetInterruptLevel(level);
}

} // namespace satemu::sys
