#include <satemu/hw/vdp/vdp2_scu_ops.hpp>

#include <satemu/hw/scu/scu.hpp>

namespace satemu::vdp2 {

SCUOperations::SCUOperations(scu::SCU &scu)
    : m_scu(scu) {}

void SCUOperations::TriggerHBlankIN() {
    m_scu.TriggerHBlankIN();
}

void SCUOperations::TriggerVBlankIN() {
    m_scu.TriggerVBlankIN();
}

void SCUOperations::TriggerVBlankOUT() {
    m_scu.TriggerVBlankOUT();
}

} // namespace satemu::vdp2
