#pragma once

#include "sh2_bus.hpp"
#include "sh2_state.hpp"

#include <span>

namespace satemu::sh2 {

class SH2 {
public:
    SH2(scu::SCU &scu, smpc::SMPC &smpc);

    void Reset(bool hard);

    void LoadIPL(std::span<uint8, kIPLSize> ipl);

    void Step();

    void SetExternalInterruptCallback(sh2::CBAcknowledgeExternalInterrupt callback);
    void SetExternalInterrupt(uint8 level, uint8 vecNum);

private:
    SH2State m_masterState{true};
    SH2State m_slaveState{false};
    SH2Bus m_bus;
};

} // namespace satemu::sh2
