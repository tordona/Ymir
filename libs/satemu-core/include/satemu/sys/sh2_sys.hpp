#pragma once

#include <satemu/hw/sh2/sh2.hpp>

namespace satemu::sys {

class SH2System {
public:
    SH2System(sh2::SH2 &sh2);

    void Reset(bool hard);

    void Step();

    void SetInterruptLevel(uint8 level);

private:
    sh2::SH2 &m_SH2;
};

} // namespace satemu::sys
