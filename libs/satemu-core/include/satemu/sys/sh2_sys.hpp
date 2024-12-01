#pragma once

#include <satemu/hw/sh2/sh2.hpp>

namespace satemu::sys {

class SH2System {
public:
    SH2System(scu::SCU &scu, smpc::SMPC &smpc);

    void Reset(bool hard);

    void LoadIPL(std::span<uint8, kIPLSize> ipl);

    void Step();

    void SetInterruptLevel(uint8 level);

private:
    sh2::SH2 m_SH2;
};

} // namespace satemu::sys
