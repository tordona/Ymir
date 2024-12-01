#pragma once

#include "sh2_bus.hpp"
#include "sh2_state.hpp"

namespace satemu::sh2 {

struct SH2 {
    SH2(scu::SCU &scu, smpc::SMPC &smpc)
        : bus(scu, smpc) {}

    SH2State masterState{true};
    SH2State slaveState{false};
    SH2Bus bus;
};

} // namespace satemu::sh2
