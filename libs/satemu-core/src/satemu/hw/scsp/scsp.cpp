#include <satemu/hw/scsp/scsp.hpp>

namespace satemu::scsp {

SCSP::SCSP()
    : m_m68k(m_bus) {
    Reset(true);
}

void SCSP::Reset(bool hard) {
    m_m68k.Reset(true);
    m_bus.Reset();
}

void SCSP::Advance(uint64 cycles) {
    // TODO: proper cycle counting
    for (uint64 i = 0; i < cycles; i++) {
        m_m68k.Step();
    }
}

} // namespace satemu::scsp
