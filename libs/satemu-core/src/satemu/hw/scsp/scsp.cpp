#include <satemu/hw/scsp/scsp.hpp>

using namespace satemu::m68k;

namespace satemu::scsp {

SCSP::SCSP()
    : m_m68k(*this) {
    Reset(true);
}

void SCSP::Reset(bool hard) {
    m_m68k.Reset(true);
    m_WRAM.fill(0);

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

ExceptionVector SCSP::AcknowledgeInterrupt(uint8 level) {
    // TODO: does the SCSP allow setting specific vector numbers?
    return ExceptionVector::AutoVectorRequest;
}

} // namespace satemu::scsp
