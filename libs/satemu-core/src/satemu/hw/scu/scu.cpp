#include <satemu/hw/scu/scu.hpp>

namespace satemu::scu {

SCU::SCU(vdp1::VDP1 &vdp1, vdp2::VDP2 &vdp2, scsp::SCSP &scsp, cdblock::CDBlock &cdblock)
    : m_VDP1(vdp1)
    , m_VDP2(vdp2)
    , m_SCSP(scsp)
    , m_CDBlock(cdblock) {
    Reset(true);
}

void SCU::Reset(bool hard) {
    m_intrMask.u32 = 0;
    m_intrStatus.u32 = 0;
}

void SCU::TriggerHBlankIN() {
    m_intrMask.VDP2_HBlankIN = 1;
    // TODO: also increment Timer 0 and trigger Timer 0 interrupt if the counter matches the compare register
}

void SCU::TriggerVBlankIN() {
    m_intrMask.VDP2_VBlankIN = 1;
}

void SCU::TriggerVBlankOUT() {
    m_intrMask.VDP2_VBlankOUT = 1;
    // TODO: also reset Timer 0
}

} // namespace satemu::scu
