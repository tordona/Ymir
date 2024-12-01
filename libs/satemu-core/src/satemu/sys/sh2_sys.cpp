#include <satemu/sys/sh2_sys.hpp>

#include "sh2/sh2_interpreter.hpp"
#include "sh2/sh2_mem.hpp"

using namespace satemu::sh2;

namespace satemu::sys {

SH2System::SH2System(SH2 &sh2)
    : m_sh2(sh2) {}

void SH2System::Reset(bool hard) {
    auto reset = [&](SH2State &state) {
        state.Reset(hard);
        state.PC = MemReadLong(state, m_sh2.bus, state.VBR);
        state.R[15] = MemReadLong(state, m_sh2.bus, state.VBR + 4);
    };

    reset(m_sh2.masterState);
    reset(m_sh2.slaveState);
}

void SH2System::Step() {
    // TODO: allow switching between interpreter and JIT recompiler
    sh2::interp::Step(m_sh2.masterState, m_sh2.bus);
    // TODO: step slave SH2 if enabled
    // sh2::interp::Step(m_sh2.slaveState, m_sh2.bus);
}

} // namespace satemu::sys
