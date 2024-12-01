#include <satemu/sys/scu_sys.hpp>

#include <satemu/sys/sh2_sys.hpp>

using namespace satemu::scu;

namespace satemu::sys {

SCUSystem::SCUSystem(SCU &scu, SH2System &sysSH2)
    : m_SCU(scu)
    , m_sysSH2(sysSH2) {}

void SCUSystem::Reset(bool hard) {
    m_SCU.Reset(hard);
}

void SCUSystem::TriggerHBlankIN() {
    m_SCU.m_intrMask.VDP2_HBlankIN = 1;
    // TODO: also increment Timer 0 and trigger Timer 0 interrupt if the counter matches the compare register
    // TODO: m_sysSH2.SetInterruptLevel(intrLevel);
}

void SCUSystem::TriggerVBlankIN() {
    m_SCU.m_intrMask.VDP2_VBlankIN = 1;
    // TODO: m_sysSH2.SetInterruptLevel(intrLevel);
}

void SCUSystem::TriggerVBlankOUT() {
    m_SCU.m_intrMask.VDP2_VBlankOUT = 1;
    // TODO: also reset Timer 0
    // TODO: m_sysSH2.SetInterruptLevel(intrLevel);
}

} // namespace satemu::sys
