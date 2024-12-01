#include <satemu/sys/scu_sys.hpp>

#include <satemu/sys/sh2_sys.hpp>

using namespace satemu::scu;

namespace satemu::sys {

SCUSystem::SCUSystem(vdp1::VDP1 &vdp1, vdp2::VDP2 &vdp2, scsp::SCSP &scsp, cdblock::CDBlock &cdblock, SH2System &sysSH2)
    : SCU(vdp1, vdp2, scsp, cdblock)
    , m_sysSH2(sysSH2) {}

void SCUSystem::Reset(bool hard) {
    SCU.Reset(hard);
}

void SCUSystem::TriggerHBlankIN() {
    SCU.m_intrMask.VDP2_HBlankIN = 1;
    // TODO: also increment Timer 0 and trigger Timer 0 interrupt if the counter matches the compare register
    UpdateInterruptLevel();
}

void SCUSystem::TriggerVBlankIN() {
    SCU.m_intrMask.VDP2_VBlankIN = 1;
    UpdateInterruptLevel();
}

void SCUSystem::TriggerVBlankOUT() {
    SCU.m_intrMask.VDP2_VBlankOUT = 1;
    // TODO: also reset Timer 0
    UpdateInterruptLevel();
}

void SCUSystem::UpdateInterruptLevel() {
    // TODO: m_sysSH2.SetInterruptLevel(intrLevel);
}

} // namespace satemu::sys
