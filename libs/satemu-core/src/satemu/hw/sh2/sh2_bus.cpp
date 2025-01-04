#include <satemu/hw/sh2/sh2_bus.hpp>

#include <satemu/hw/sh2/sh2.hpp>

namespace satemu::sh2 {

SH2Bus::SH2Bus(SH2 &masterSH2, SH2 &slaveSH2, scu::SCU &scu, smpc::SMPC &smpc)
    : m_masterSH2(masterSH2)
    , m_slaveSH2(slaveSH2)
    , m_SCU(scu)
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

void SH2Bus::WriteMINIT(uint16 value) {
    m_slaveSH2.TriggerFRTInputCapture();
}

void SH2Bus::WriteSINIT(uint16 value) {
    m_masterSH2.TriggerFRTInputCapture();
}

} // namespace satemu::sh2
