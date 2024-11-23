#include <satemu/hw/sh2/sh2_bus.hpp>

namespace satemu {

SH2Bus::SH2Bus(SCU &scu, SMPC &smpc, SCSP &scsp, CDBlock &cdblock)
    : m_SCU(scu)
    , m_SMPC(smpc)
    , m_SCSP(scsp)
    , m_CDBlock(cdblock) {
    m_IPL.fill(0);
    Reset(true);
}

void SH2Bus::Reset(bool hard) {
    m_WRAMLow.fill(0);
    m_WRAMHigh.fill(0);
}

void SH2Bus::LoadIPL(std::span<uint8, kIPLSize> ipl) {
    std::copy(ipl.begin(), ipl.end(), m_IPL.begin());
}

} // namespace satemu
