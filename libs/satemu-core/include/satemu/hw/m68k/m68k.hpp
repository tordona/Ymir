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

    // Floating-point registers
    // TODO: the M68k uses IEEE 754 double-extended-precision (80-bit) floating-point registers
    std::array<double, 8> FP;

    // Floating-point control register
    union FPCR_t {
        uint16 u16;
        struct {
            // --- mode control ---
            uint16 _zero : 4;
            uint16 RND : 2;  // Rounding mode
            uint16 PREC : 2; // Rounding precision
            // --- exception enable ---
            uint16 INEX1 : 1; // Inexact decimal input
            uint16 INEX2 : 1; // Inexact operation
            uint16 DZ : 1;    // Division by zero
            uint16 UNFL : 1;  // Underflow
            uint16 OVFL : 1;  // Overflow
            uint16 OPERR : 1; // Operand error
            uint16 SNAN : 1;  // Signaling not-a-number
            uint16 BSUN : 1;  // Branch/set on unordered
        };
    } FPCR;

    // Floating-point status register
    union FPSR_t {
        uint32 u32;
        struct {
            // Accrued exception byte
            union {
                uint8 u8;
                struct {
                    uint8 : 3;
                    uint8 INEX : 1; // Inexact
                    uint8 DZ : 1;   // Division by zero
                    uint8 UNFL : 1; // Underflow
                    uint8 OVFL : 1; // Overflow
                    uint8 IOP : 1;  // Invalid operation
                };
            } AEXC;

            // Exception status byte
            union {
                uint8 u8;
                struct {
                    uint8 INEX1 : 1; // Inexact decimal input
                    uint8 INEX2 : 1; // Inexact operation
                    uint8 DZ : 1;    // Division by zero
                    uint8 UNFL : 1;  // Underflow
                    uint8 OVFL : 1;  // Overflow
                    uint8 OPERR : 1; // Operand error
                    uint8 SNAN : 1;  // Signaling not-a-number
                    uint8 BSUN : 1;  // Branch/set on unordered
                };
            } EXC;

            // Quotient byte
            uint8 quotient;

            // Floating-point condition code byte
            union {
                uint8 u8;
                struct {
                    uint8 NaN : 1; // Not-a-number or unordered
                    uint8 I : 1;   // Infinity
                    uint8 Z : 1;   // Zero
                    uint8 N : 1;   // Negative
                    uint8 : 4;
                };
            } FPCC;
        };
    } FPSR;

    uint32 FPIAR; // Floating-point instruction address register

    // -------------------------------------------------------------------------
    // Memory accessors

    M68kBus &m_bus;
};

} // namespace satemu::m68k
