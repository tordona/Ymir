#include <satemu/hw/scsp/scsp.hpp>

namespace satemu::scsp {

SCSP::SCSP() {
    Reset(true);
}

void SCSP::Reset(bool hard) {
    m_m68kWRAM.fill(0);
}

} // namespace satemu::scsp
