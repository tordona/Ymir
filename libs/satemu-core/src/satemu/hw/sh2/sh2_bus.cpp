#include <satemu/hw/sh2/sh2_bus.hpp>

namespace satemu::sh2 {

SH2Bus::SH2Bus(scu::SCU &scu, smpc::SMPC &smpc)
    : m_SCU(scu)
    , m_SMPC(smpc) {
    IPL.fill(0);
    Reset(true);
}

void SH2Bus::Reset(bool hard) {
    WRAMLow.fill(0);
    WRAMHigh.fill(0);
}

void SH2Bus::LoadIPL(std::span<uint8, kIPLSize> ipl) {
    std::copy(ipl.begin(), ipl.end(), IPL.begin());
}

void SH2Bus::AcknowledgeExternalInterrupt() {
    m_SCU.AcknowledgeExternalInterrupt();
}

} // namespace satemu::sh2
