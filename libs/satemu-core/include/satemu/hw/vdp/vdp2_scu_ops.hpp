#pragma once

// Forward declarations
namespace satemu::scu {

class SCU;

} // namespace satemu::scu

namespace satemu::vdp2 {

class SCUOperations {
public:
    SCUOperations(scu::SCU &scu);

    void TriggerHBlankIN();
    void TriggerVBlankIN();
    void TriggerVBlankOUT();

private:
    scu::SCU &m_scu;
};

} // namespace satemu::vdp2
