#include <satemu/hw/scsp/scsp.hpp>

namespace satemu::scsp {

SCSP::SCSP()
    : m_m68k(m_bus) {
    Reset(true);
}

void SCSP::Reset(bool hard) {
    m_m68k.Reset(true);
    m_bus.Reset();

    m_cpuEnabled = false;
}

void SCSP::Advance(uint64 cycles) {
    if (m_cpuEnabled) {
        // TODO: proper cycle counting
        for (uint64 i = 0; i < cycles; i++) {
            m_m68k.Step();
        }
    }
}

void SCSP::SetCPUEnabled(bool enabled) {
    fmt::println("SCSP: {} the MC68EC00 processor", (enabled ? "enabling" : "disabling"));
    if (enabled) {
        m_m68k.Reset(true); // false? does it matter?
    }
    m_cpuEnabled = enabled;
}

} // namespace satemu::scsp
