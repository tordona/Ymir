#include <satemu/sys/sys.hpp>

namespace satemu {

Saturn::Saturn()
    : m_sh2bus(m_SCU, m_SMPC, m_SCSP, m_CDBlock)
    , m_masterSH2(m_sh2bus, true)
    , m_slaveSH2(m_sh2bus, false) {
    Reset(true);
}

void Saturn::Reset(bool hard) {
    m_sh2bus.Reset(hard);
    m_masterSH2.Reset(hard);
    m_SCU.Reset(hard);
    m_SMPC.Reset(hard);
    m_SCSP.Reset(hard);
    m_CDBlock.Reset(hard);
}

void Saturn::LoadIPL(std::span<uint8, kIPLSize> ipl) {
    m_sh2bus.LoadIPL(ipl);
}

void Saturn::Step() {
    m_masterSH2.Step();
}

} // namespace satemu
