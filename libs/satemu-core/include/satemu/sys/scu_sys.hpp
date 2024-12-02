#pragma once

#include <satemu/hw/scu/scu.hpp>

// Forward declarations
namespace satemu::sys {

class SH2System;

} // namespace satemu::sys

// -----------------------------------------------------------------------------

namespace satemu::sys {

// TODO: consider merging back into SCU
class SCUSystem {
public:
    SCUSystem(vdp1::VDP1 &vdp1, vdp2::VDP2 &vdp2, scsp::SCSP &scsp, cdblock::CDBlock &cdblock, SH2System &sysSH2);

    void Reset(bool hard);

    // External interrupt triggers

    void TriggerHBlankIN();
    void TriggerVBlankIN();
    void TriggerVBlankOUT();
    void TriggerSystemManager();

    scu::SCU SCU;

private:
    SH2System &m_sysSH2;

    void UpdateInterruptLevel(bool acknowledge);
};

} // namespace satemu::sys
