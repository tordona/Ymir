#include <satemu/sys/sys.hpp>

namespace satemu {

Saturn::Saturn()
    : m_sh2bus(m_SCU, m_SMPC)
    , m_masterSH2(m_sh2bus, true)
    , m_slaveSH2(m_sh2bus, false)
    , m_SCU(m_VDP1, m_VDP2, m_SCSP, m_CDBlock)
    , m_VDP2(m_SCU) {
    Reset(true);
}

void Saturn::Reset(bool hard) {
    m_sh2bus.Reset(hard);
    m_masterSH2.Reset(hard);
    m_slaveSH2.Reset(hard);
    m_SCU.Reset(hard);
    m_SMPC.Reset(hard);
    m_SCSP.Reset(hard);
    m_CDBlock.Reset(hard);
    m_VDP1.Reset(hard);
    m_VDP2.Reset(hard);
}

void Saturn::LoadIPL(std::span<uint8, kIPLSize> ipl) {
    m_sh2bus.LoadIPL(ipl);
}

void Saturn::Step() {
    m_masterSH2.Step();

    // TODO: proper timings
    // TODO: remove when using a scheduler
    m_VDP2.Advance(1);
}

} // namespace satemu
