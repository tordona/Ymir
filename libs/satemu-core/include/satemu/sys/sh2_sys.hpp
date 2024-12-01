#pragma once

#include <satemu/hw/sh2/sh2.hpp>

namespace satemu::sys {

class SH2System {
public:
    SH2System(scu::SCU &scu, smpc::SMPC &smpc);

    void Reset(bool hard);

    void Step();

    void SetInterruptLevel(uint8 level);

    sh2::SH2 SH2;
};

} // namespace satemu::sys
