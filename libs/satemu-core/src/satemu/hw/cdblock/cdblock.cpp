#include <satemu/hw/cdblock/cdblock.hpp>

#include <satemu/hw/scu/scu.hpp>

namespace satemu::cdblock {

CDBlock::CDBlock(scu::SCU &scu)
    : m_scu(scu) {
    Reset(true);
}

void CDBlock::Reset(bool hard) {
    m_CR[0] = 0x0043; // ' C'
    m_CR[1] = 0x4442; // 'DB'
    m_CR[2] = 0x4C4F; // 'LO'
    m_CR[3] = 0x434B; // 'CK'

    m_HIRQ = 0;
    m_HIRQMASK = 0;
}

void CDBlock::Advance(uint64 cycles) {}

void CDBlock::UpdateInterrupts() {
    if (m_HIRQ & m_HIRQMASK) {
        m_scu.TriggerExternalInterrupt0();
    }
}

} // namespace satemu::cdblock
