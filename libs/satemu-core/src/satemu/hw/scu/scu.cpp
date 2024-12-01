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

} // namespace satemu::scu
