#include <satemu/hw/sh2/sh2.hpp>

#include "sh2_interpreter.hpp"
#include "sh2_mem.hpp"

namespace satemu::sh2 {

SH2::SH2(scu::SCU &scu, smpc::SMPC &smpc)
    : m_bus(scu, smpc) {}

void SH2::Reset(bool hard) {
    auto reset = [&](SH2State &state) {
        state.Reset(hard);
        state.PC = MemReadLong(state, m_bus, state.VBR);
        state.R[15] = MemReadLong(state, m_bus, state.VBR + 4);
    };

    reset(m_masterState);
    reset(m_slaveState);
}

void SH2::LoadIPL(std::span<uint8, kIPLSize> ipl) {
    m_bus.LoadIPL(ipl);
}

void SH2::Step() {
    // TODO: allow switching between interpreter and JIT recompiler
    sh2::interp::Step(m_masterState, m_bus);
    // TODO: step slave SH2 if enabled
    // sh2::interp::Step(m_slaveState, m_bus);
}

void SH2::SetExternalInterruptCallback(CBAcknowledgeExternalInterrupt callback) {
    m_masterState.SetExternalInterruptCallback(callback);
    // TODO: slave callback?
}

void SH2::SetExternalInterrupt(uint8 level, uint8 vecNum) {
    m_masterState.SetExternalInterrupt(level, vecNum);
    // TODO: when to set slave IRL?
    // m_slaveState.SetExternalInterrupt(level, vecNum);
}

} // namespace satemu::sh2
