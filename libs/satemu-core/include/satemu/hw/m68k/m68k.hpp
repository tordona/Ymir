#pragma once

#include "m68k_decode.hpp"
#include "m68k_defs.hpp"

#include <satemu/core_types.hpp>
#include <satemu/hw/hw_defs.hpp>

#include <array>

// -----------------------------------------------------------------------------
// Forward declarations

namespace satemu::scsp {

class SCSP; // doubles as the MC68EC000 bus

} // namespace satemu::scsp

// -----------------------------------------------------------------------------

namespace satemu::m68k {

// Just to hide the fact that we're using the SCSP directly as the bus
using M68kBus = scsp::SCSP;

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
    // A7 is used as the hardware stack pointer, called:
    //   User Stack Pointer (USP) in user mode
    //   Supervisor Stack Pointer (SSP) in supervisor mode (also A7')
    // The stack is selected by bits S and M in CCR.
    // Since M is always zero on MC68EC000, the CPU only has one supervisor mode stack register.
    uint32 SP_swap;

    uint32 PC;

    union SR_t {
        uint16 u16;
        struct {
            // --- condition code register (CCR) ---
            // available in all modes
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
        uint16 flags : 4;
    } SR;

    // -------------------------------------------------------------------------
    // Memory accessors

    M68kBus &m_bus;

    template <mem_access_type T, bool instrFetch>
    T MemRead(uint32 address);

    template <mem_access_type T>
    void MemWrite(uint32 address, T value);

    uint16 FetchInstruction();

    uint8 MemReadByte(uint32 address);
    uint16 MemReadWord(uint32 address);
    uint32 MemReadLong(uint32 address);

    void MemWriteByte(uint32 address, uint8 value);
    void MemWriteWord(uint32 address, uint16 value);
    void MemWriteLong(uint32 address, uint32 value);

    // -------------------------------------------------------------------------
    // Helper functions

    template <mem_access_type T>
    T ReadEffectiveAddress(uint8 M, uint8 Xn);

    template <mem_access_type T>
    void WriteEffectiveAddress(uint8 M, uint8 Xn, T value);

    // Calculates effective addresses for instructions that use control addresses (LEA, JSR, JMP, MOVEM, etc.)
    uint32 CalcEffectiveAddress(uint8 M, uint8 Xn);

    // -------------------------------------------------------------------------
    // Interpreter

    void Execute();

    // -------------------------------------------------------------------------
    // Instruction interpreters

    void Instr_Move_EA_EA(uint16 instr);
    void Instr_Move_EA_SR(uint16 instr);
    void Instr_MoveQ(uint16 instr);
    void Instr_MoveA(uint16 instr);

    void Instr_AddA(uint16 instr);
    void Instr_AndI_EA(uint16 instr);

    void Instr_LEA(uint16 instr);

    void Instr_BRA(uint16 instr);
    void Instr_BSR(uint16 instr);
    void Instr_Bcc(uint16 instr);
    void Instr_DBcc(uint16 instr);
    void Instr_JSR(uint16 instr);

    void Instr_RTS(uint16 instr);

    void Instr_Illegal(uint16 instr);
};

} // namespace satemu::m68k
