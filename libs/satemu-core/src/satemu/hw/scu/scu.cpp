#include <satemu/hw/scu/scu.hpp>

namespace satemu {

SCU::SCU(VDP1 &vdp1, VDP2 &vdp2, SCSP &scsp, CDBlock &cdblock)
    : m_VDP1(vdp1)
    , m_VDP2(vdp2)
    , m_SCSP(scsp)
    , m_CDBlock(cdblock) {
    Reset(true);
}

void SCU::Reset(bool hard) {}

} // namespace satemu
