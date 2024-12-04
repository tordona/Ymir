#pragma once

#include "m68k_defs.hpp"

#include <satemu/core_types.hpp>

#include <array>

// -----------------------------------------------------------------------------
// Forward declarations

namespace satemu::m68k {

class M68kBus;

} // namespace satemu::m68k

// -----------------------------------------------------------------------------

namespace satemu::m68k {

class MC68EC000 {
public:
    MC68EC000(M68kBus &bus);

    void Reset(bool hard);

    void Step();

private:
    // -------------------------------------------------------------------------
    // CPU state

    std::array<uint32, 8> D; // Data registers D0-D7
    std::array<uint32, 8> A; // Address registers A0-A7

    // This variable stores the value of the inactive stack pointer.
    //
    // A7 is also used as the hardware stack pointer, called:
    //   USP in user mode
    //   SSP in supervisor mode
    // The stack is selected by bits S and M in CCR.
    // Since M is always zero on MC68EC000, the CPU only has one supervisor mode stack register.
    uint32 SP_swap;

    uint32 PC;

    union CCR_t {
        uint16 u16;
        struct {
            uint16 C : 1; // Carry/borrow flag
            uint16 V : 1; // Overflow flag
            uint16 Z : 1; // Zero flag
            uint16 N : 1; // Negative flag
            uint16 X : 1; // Extend flag
            // --- supervisor mode only ---
            uint16 : 3;
            uint16 IPM : 3; // Interrupt priority mask (I2-I0)
            uint16 : 1;
            uint16 M : 1;  // Master/interrupt state (always zero on MC68EC000)
            uint16 S : 1;  // Supervisor/user state
            uint16 T0 : 1; // Trace enable 0 (always zero on MC68EC000)
            uint16 T1 : 1; // Trace enable 1
        };
    } CCR;

    // -------------------------------------------------------------------------
    // Memory accessors

    M68kBus &m_bus;
};

} // namespace satemu::m68k
