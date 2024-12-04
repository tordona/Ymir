#pragma once

#include "m68k_defs.hpp"

// -----------------------------------------------------------------------------
// Forward declarations

namespace satemu::m68k {

class M68kBus;

} // namespace satemu::m68k

// -----------------------------------------------------------------------------

namespace satemu::m68k {

class M68EC000 {
public:
    M68EC000(M68kBus &bus);

    void Reset(bool hard);

    void Step();

private:
    M68kBus &m_bus;
};

} // namespace satemu::m68k
