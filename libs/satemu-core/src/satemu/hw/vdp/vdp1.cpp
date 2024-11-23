#include <satemu/hw/vdp/vdp1.hpp>

namespace satemu {

VDP1::VDP1() {
    Reset(true);
}

void VDP1::Reset(bool hard) {
    m_VRAM.fill(0);
    for (auto &fb : m_framebuffers) {
        fb.fill(0);
    }
    m_drawFB = 0;
}

} // namespace satemu
