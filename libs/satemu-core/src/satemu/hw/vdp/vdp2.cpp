#include <satemu/hw/vdp/vdp2.hpp>

namespace satemu {

VDP2::VDP2() {
    Reset(true);
}

void VDP2::Reset(bool hard) {
    m_VRAM.fill(0);
    m_CRAM.fill(0);
    TVMD.u16 = 0x0;
    RAMCTL.u16 = 0x0;
}

} // namespace satemu
