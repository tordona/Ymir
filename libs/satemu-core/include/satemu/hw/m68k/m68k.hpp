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

    void SetExternalInterruptLevel(uint8 level);

private:
    // -------------------------------------------------------------------------
    // CPU state

    union Regs {
        std::array<uint32, 8 + 8> DA; // D0-D7 followed by A0-A7
        struct {
            std::array<uint32, 8> D; // Data registers D0-D7
            std::array<uint32, 8> A; // Address registers A0-A7
        };
        struct {
            uint32 D0;
            uint32 D1;
            uint32 D2;
            uint32 D3;
            uint32 D4;
            uint32 D5;
            uint32 D6;
            uint32 D7;
            uint32 A0;
            uint32 A1;
            uint32 A2;
            uint32 A3;
            uint32 A4;
            uint32 A5;
            uint32 A6;
            uint32 SP;
        };
    } regs;

    // This variable stores the value of the inactive stack pointer.
    //
    // A7 is used as the hardware stack pointer, called:
    //   User Stack Pointer (USP) in user mode
    //   Supervisor Stack Pointer (SSP) in supervisor mode (also A7')
    // The stack is selected by bits S and M in CCR.
    // Since M is always zero on MC68EC000, the CPU only has one supervisor mode stack register.
    uint32 SP_swap;

    uint32 PC;

    union RegSR {
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
            uint16 /*M*/ : 1;    // Master/interrupt state (always zero on MC68EC000)
            uint16 S : 1;        // Supervisor/user state
            uint16 /*T0*/ : 1;   // Trace enable 0 (always zero on MC68EC000)
            uint16 /*T1*/ T : 1; // Trace enable 1
        };
        uint16 flags : 4;
    } SR;

    // -------------------------------------------------------------------------
    // Memory accessors

    M68kBus &m_bus;

    template <mem_primitive T, bool instrFetch>
    T MemRead(uint32 address);

    template <mem_primitive T>
    void MemWrite(uint32 address, T value);

    uint16 FetchInstruction();

    // -------------------------------------------------------------------------
    // Exception handling

    uint8 m_externalInterruptLevel;

    // Enters the specified exception vector
    void EnterException(ExceptionVector vector);

    // Handles an interrupt of the specified level.
    // The vector is either provided from an external device or generated internally.
    void HandleInterrupt(ExceptionVector vector, uint8 level);

    // Performs the common exception handling process:
    // - Makes a copy of SR
    // - Enters supervisor mode (SR.S = 1)
    // - Disables tracing (SR.T = 0)
    // - Pushes PC and the unmodified copy of SR to the stack
    // - Sets PC to the address at the exception vector
    void HandleExceptionCommon(ExceptionVector vector, uint8 intrLevel);

    // Checks if the privileged instruction can be executed.
    // Enters the privileged violation exception vector if running in user mode.
    // Returns true if the privileged instruction can be executed.
    bool CheckPrivilege();

    // Enters the interrupt exception handler if there is a pending interrupt not masked by the current interrupt level.
    void CheckInterrupt();

    // -------------------------------------------------------------------------
    // Helper functions

    // Sets SR to the specified value and handles privilege mode switching
    void SetSR(uint16 value);

    // Reads from an effective address
    template <mem_primitive T>
    T ReadEffectiveAddress(uint8 M, uint8 Xn);

    // Writes to an effective address
    template <mem_primitive T>
    void WriteEffectiveAddress(uint8 M, uint8 Xn, T value);

    // Read-modify-write an effective address
    template <mem_primitive T, typename FnModify>
    void ModifyEffectiveAddress(uint8 M, uint8 Xn, FnModify &&modify);

    // Calculates effective addresses for instructions that use control addresses (LEA, JSR, JMP, MOVEM, etc.)
    uint32 CalcEffectiveAddress(uint8 M, uint8 Xn);

    // Update XNZVC flags based on the result of an arithmetic operation:
    //  N is set if the result is negative (MSB set)
    //  Z is set if the result is zero
    //  V is set if the result overflows (addition if sub=false, subtraction if sub=true)
    //  C is set if the operation resulted in a carry or borrow
    //  X is set to C if setX is true
    template <std::integral T, bool sub, bool setX = false>
    void SetArithFlags(T op1, T op2, T result);

    // Update XNZVC flags based on the result of an addition:
    //  N is set if the result is negative (MSB set)
    //  Z is set if the result is zero
    //  V is set if the result overflows
    //  C and X are set if the operation resulted in a carry or borrow
    template <std::integral T>
    void SetAdditionFlags(T op1, T op2, T result) {
        return SetArithFlags<T, false, true>(op1, op2, result);
    }

    // Update XNZVC flags based on the result of a subtraction:
    //  N is set if the result is negative (MSB set)
    //  Z is set if the result is zero
    //  V is set if the result overflows
    //  C and X are set if the operation resulted in a carry or borrow
    template <std::integral T>
    void SetSubtractionFlags(T op1, T op2, T result) {
        return SetArithFlags<T, true, true>(op1, op2, result);
    }

    // Update XNZVC flags based on the result of a comparison operation:
    //  N is set if the result is negative (MSB set)
    //  Z is set if the result is zero
    //  V is set if the result overflows
    //  C is set if the operation resulted in a carry or borrow
    //  X is not changed
    template <std::integral T>
    void SetCompareFlags(T op1, T op2, T result) {
        return SetArithFlags<T, true, false>(op1, op2, result);
    }

    // Update NZVC flags based on the result of a logic or move operation.
    //  N is set if the result is negative (MSB set)
    //  Z is set if the result is zero
    //  V and C are cleared
    //  X is not updated
    template <std::integral T>
    void SetLogicFlags(T result);

    // Update XNZVC flags based on the result of a bit shift or rotation operation:
    //  N is set if the result is negative (MSB set)
    //  Z is set if the result is zero
    //  V is set if the result overflows
    //  C and X are set to the last bit shifted out
    template <std::integral T>
    void SetShiftFlags(T result, bool carry);

    // -------------------------------------------------------------------------
    // Prefetch queue

    // Instruction prefetch queue, containing IRC and IRD in that order.
    // The full prefetch queue actually has 3 registers:
    // - IRC: the instruction prefetched from external memory
    // - IR:  the instruction being decoded
    // - IRD: the instruction being executed
    // IR is omitted for performance.
    std::array<uint16, 2> m_prefetchQueue;

    // Refills the entire prefetch queue.
    void FullPrefetch();

    // Prefetches the next instruction from external memory into IRC and returns the previous value of IRC.
    uint16 PrefetchNext();

    // Transfers IRC into IR, prefetches the next instruction from external memory into IRC and transfers IR into IRD.
    void PrefetchTransfer(); // IRC -> IR; prefetch next into IRC; IR -> IRD

    // -------------------------------------------------------------------------
    // Interpreter

    void Execute();

    // -------------------------------------------------------------------------
    // Instruction interpreters

    void Instr_Move_EA_EA(uint16 instr);
    void Instr_Move_EA_SR(uint16 instr);
    void Instr_MoveA(uint16 instr);
    void Instr_MoveM_EA_Rs(uint16 instr);
    void Instr_MoveM_PI_Rs(uint16 instr);
    void Instr_MoveM_Rs_EA(uint16 instr);
    void Instr_MoveM_Rs_PD(uint16 instr);
    void Instr_MoveQ(uint16 instr);

    void Instr_Clr(uint16 instr);
    void Instr_Swap(uint16 instr);

    void Instr_Add_Dn_EA(uint16 instr);
    void Instr_Add_EA_Dn(uint16 instr);
    void Instr_AddA(uint16 instr);
    void Instr_AddI(uint16 instr);
    void Instr_AddQ_An(uint16 instr);
    void Instr_AddQ_EA(uint16 instr);
    void Instr_AndI_EA(uint16 instr);
    void Instr_Eor_Dn_EA(uint16 instr);
    void Instr_Or_Dn_EA(uint16 instr);
    void Instr_Or_EA_Dn(uint16 instr);
    void Instr_OrI_EA(uint16 instr);
    void Instr_SubI(uint16 instr);
    void Instr_SubQ_An(uint16 instr);
    void Instr_SubQ_EA(uint16 instr);

    void Instr_LSL_I(uint16 instr);
    void Instr_LSL_M(uint16 instr);
    void Instr_LSL_R(uint16 instr);
    void Instr_LSR_I(uint16 instr);
    void Instr_LSR_M(uint16 instr);
    void Instr_LSR_R(uint16 instr);

    void Instr_Cmp(uint16 instr);
    void Instr_CmpA(uint16 instr);
    void Instr_CmpI(uint16 instr);
    void Instr_BTst_I_Dn(uint16 instr);
    void Instr_BTst_I_EA(uint16 instr);
    void Instr_BTst_R_Dn(uint16 instr);
    void Instr_BTst_R_EA(uint16 instr);

    void Instr_LEA(uint16 instr);

    void Instr_BRA(uint16 instr);
    void Instr_BSR(uint16 instr);
    void Instr_Bcc(uint16 instr);
    void Instr_DBcc(uint16 instr);
    void Instr_JSR(uint16 instr);
    void Instr_Jmp(uint16 instr);

    void Instr_RTS(uint16 instr);

    void Instr_Trap(uint16 instr);
    void Instr_TrapV(uint16 instr);

    void Instr_Noop(uint16 instr);

    void Instr_Illegal(uint16 instr);
    void Instr_Illegal1010(uint16 instr);
    void Instr_Illegal1111(uint16 instr);
};

} // namespace satemu::m68k
