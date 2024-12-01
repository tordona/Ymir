#pragma once

#include <satemu/hw/scu/scu.hpp>

// Forward declarations
namespace satemu::sys {

class SH2System;

} // namespace satemu::sys

// -----------------------------------------------------------------------------

namespace satemu::sys {

class SCUSystem {
public:
    SCUSystem(scu::SCU &scu, SH2System &sysSH2);

    void Reset(bool hard);

    // External interrupt triggers

    void TriggerHBlankIN();
    void TriggerVBlankIN();
    void TriggerVBlankOUT();

private:
    scu::SCU &m_SCU;
    SH2System &m_sysSH2;
};

} // namespace satemu::sys
