#pragma once

#include <satemu/core_types.hpp>

#include <satemu/util/bit_ops.hpp>

#include <array>
#include <new>

namespace satemu::m68k {

// Condition code table
inline constexpr auto kCondTable = [] {
    std::array<bool, 16 * 16> arr{};
    for (uint32 nzvc = 0; nzvc < 16; nzvc++) {
        const bool n = bit::extract<3>(nzvc);
        const bool z = bit::extract<2>(nzvc);
        const bool v = bit::extract<1>(nzvc);
        const bool c = bit::extract<0>(nzvc);

        arr[(0u << 4u) | nzvc] = true;          // T
        arr[(1u << 4u) | nzvc] = false;         // F
        arr[(2u << 4u) | nzvc] = !c && !z;      // HI
        arr[(3u << 4u) | nzvc] = c || z;        // LS
        arr[(4u << 4u) | nzvc] = !c;            // CC
        arr[(5u << 4u) | nzvc] = c;             // CS
        arr[(6u << 4u) | nzvc] = !z;            // NE
        arr[(7u << 4u) | nzvc] = z;             // EQ
        arr[(8u << 4u) | nzvc] = !v;            // VC
        arr[(9u << 4u) | nzvc] = v;             // VS
        arr[(10u << 4u) | nzvc] = !n;           // PL
        arr[(11u << 4u) | nzvc] = n;            // MI
        arr[(12u << 4u) | nzvc] = n == v;       // GE
        arr[(13u << 4u) | nzvc] = n != v;       // LT
        arr[(14u << 4u) | nzvc] = n == v && !z; // GT
        arr[(15u << 4u) | nzvc] = n != v || z;  // LE
    }
    return arr;
}();

// -----------------------------------------------------------------------------

enum class OpcodeType : uint8 {
    Move_EA_EA,  // move.<sz> <ea_src>, <ea_dst>
    Move_EA_SR,  // move.w <ea>, SR
    MoveA,       // movea.<sz> <ea>, An
    MoveM_EA_Rs, // movem.<sz> <ea>, <list>
    MoveM_PI_Rs, // movem.<sz> (An)+, <list>
    MoveM_Rs_EA, // movem.<sz> <list>, <ea>
    MoveM_Rs_PD, // movem.<sz> <list>, -(An)
    MoveP_Ay_Dx, // movep.<sz> (disp,Ay), Dx
    MoveP_Dx_Ay, // movep.<sz> Dx, (disp,Ay)
    MoveQ,       // moveq #<imm>, Dn

    Clr,       // clr.<sz> <ea>
    Exg_An_An, // exg Ax, Ay
    Exg_Dn_An, // exg Dx, Ay
    Exg_Dn_Dn, // exg Dx, Dy
    Ext_W,     // ext.w Dn
    Ext_L,     // ext.l Dn
    Swap,      // swap Dn

    Add_Dn_EA, // add.<sz> Dn, <ea>
    Add_EA_Dn, // add.<sz> <ea>, Dn
    AddA,      // adda.<sz> <ea>, An
    AddI,      // addi.<sz> #<data>, <ea>
    AddQ_An,   // addq.<sz> #<data>, An
    AddQ_EA,   // addq.<sz> #<data>, <ea>
    AddX_M,    // addx.<sz> -(Ay), -(Ax)
    AddX_R,    // addx.<sz> Dy, Dx
    And_Dn_EA, // and.<sz> Dn, <ea>
    And_EA_Dn, // and.<sz> <ea>, Dn
    AndI_EA,   // andi.<sz> #<data>, <ea>
    Eor_Dn_EA, // eor.<sz> Dn, <ea>
    EorI_EA,   // eori.<sz> #<data>, <ea>
    Neg,       // neg.<sz> <ea>
    NegX,      // negx.<sz> <ea>
    Not,       // not.<sz> <ea>
    Or_Dn_EA,  // or.<sz> Dn, <ea>
    Or_EA_Dn,  // or.<sz> <ea>, Dn
    OrI_EA,    // ori.<sz> #<data>, <ea>
    Sub_Dn_EA, // sub.<sz> Dn, <ea>
    Sub_EA_Dn, // sub.<sz> <ea>, Dn
    SubA,      // suba.<sz> <ea>, An
    SubI,      // subi.<sz> #<data>, <ea>
    SubQ_An,   // subq.<sz> #<data>, An
    SubQ_EA,   // subq.<sz> #<data>, <ea>
    SubX_M,    // subx.<sz> -(Ay), -(Ax)
    SubX_R,    // subx.<sz> Dy, Dx

    BChg_I_Dn, // bchg.<sz> #<data>, Dn
    BChg_I_EA, // bchg.<sz> #<data>, <ea>
    BChg_R_Dn, // bchg.<sz> Dn, Dn
    BChg_R_EA, // bchg.<sz> Dn, <ea>
    BClr_I_Dn, // bclr.<sz> #<data>, Dn
    BClr_I_EA, // bclr.<sz> #<data>, <ea>
    BClr_R_Dn, // bclr.<sz> Dn, Dn
    BClr_R_EA, // bclr.<sz> Dn, <ea>
    BSet_I_Dn, // bset.<sz> #<data>, Dn
    BSet_I_EA, // bset.<sz> #<data>, <ea>
    BSet_R_Dn, // bset.<sz> Dn, Dn
    BSet_R_EA, // bset.<sz> Dn, <ea>
    BTst_I_Dn, // btst.<sz> #<data>, Dn
    BTst_I_EA, // btst.<sz> #<data>, <ea>
    BTst_R_Dn, // btst.<sz> Dn, Dn
    BTst_R_EA, // btst.<sz> Dn, <ea>

    ASL_I,  // asl.<sz> #<data>, <ea>
    ASL_M,  // asl.<sz> <ea>
    ASL_R,  // asl.<sz> Dn, <ea>
    ASR_I,  // asr.<sz> #<data>, <ea>
    ASR_M,  // asr.<sz> <ea>
    ASR_R,  // asr.<sz> Dn, <ea>
    LSL_I,  // lsl.<sz> #<data>, <ea>
    LSL_M,  // lsl.<sz> <ea>
    LSL_R,  // lsl.<sz> Dn, <ea>
    LSR_I,  // lsr.<sz> #<data>, <ea>
    LSR_M,  // lsr.<sz> <ea>
    LSR_R,  // lsr.<sz> Dn, <ea>
    ROL_I,  // rol.<sz> #<data>, <ea>
    ROL_M,  // rol.<sz> <ea>
    ROL_R,  // rol.<sz> Dn, <ea>
    ROR_I,  // ror.<sz> #<data>, <ea>
    ROR_M,  // ror.<sz> <ea>
    ROR_R,  // ror.<sz> Dn, <ea>
    ROXL_I, // roxl.<sz> #<data>, <ea>
    ROXL_M, // roxl.<sz> <ea>
    ROXL_R, // roxl.<sz> Dn, <ea>
    ROXR_I, // roxr.<sz> #<data>, <ea>
    ROXR_M, // roxr.<sz> <ea>
    ROXR_R, // roxr.<sz> Dn, <ea>

    Cmp,  // cmp.<sz> <ea>, An
    CmpA, // cmpa <ea>, An
    CmpI, // cmpi.<sz> #<data>, <ea>
    CmpM, // cmpm.<sz> (Ay)+, (Ax)+
    Scc,  // scc <ea>
    TAS,  // tas <ea>
    Tst,  // tst.<sz> <ea>

    LEA, // lea <ea>, An
    PEA, // pea <ea>

    Link,   // link An, #<disp>
    Unlink, // unlk An

    BRA,  // bra <label>
    BSR,  // bsr <label>
    Bcc,  // b<cc> <label>
    DBcc, // db<cc>.<sz> Dn, <label>
    JSR,  // jsr <ea>
    Jmp,  // jmp <ea>

    RTE, // rte
    RTR, // rtr
    RTS, // rts

    Chk,   // chk <ea>, Dn
    Reset, // reset
    Stop,  // stop #<imm>
    Trap,  // trap #<vector>
    TrapV, // trapv

    Noop, // nop

    Illegal,     // any illegal instruction, including the designated ILLEGAL instruction 0100 1010 1111 1100
    Illegal1010, // illegal instructions with bits 15-12 = 1010
    Illegal1111, // illegal instructions with bits 15-12 = 1111
    Undecoded,   // instructions that the decoding table is missing; should not happen
};

struct DecodeTable {
    alignas(std::hardware_destructive_interference_size) std::array<OpcodeType, 0x10000> opcodeTypes;
    // TODO: disassembly info/function pointers?
};

DecodeTable BuildDecodeTable();

extern DecodeTable g_decodeTable;

} // namespace satemu::m68k
