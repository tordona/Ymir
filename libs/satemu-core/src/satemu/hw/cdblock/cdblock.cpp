#include <satemu/hw/cdblock/cdblock.hpp>

namespace satemu::cdblock {

CDBlock::CDBlock() {
    Reset(true);
}

void CDBlock::Reset(bool hard) {
    m_CR[0] = 0x0043; // ' C'
    m_CR[1] = 0x4442; // 'DB'
    m_CR[2] = 0x4C4F; // 'LO'
    m_CR[3] = 0x434B; // 'CK'

    m_HIRQ = 0;
}

} // namespace satemu::cdblock
