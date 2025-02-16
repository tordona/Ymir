#pragma once

#include <satemu/core/types.hpp>

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
    Move_EA_EA_B,    // move.b <ea_src>, <ea_dst>
    Move_EA_EA_W,    // move.w <ea_src>, <ea_dst>
    Move_EA_EA_L,    // move.l <ea_src>, <ea_dst>
    Move_EA_CCR,     // move.w <ea>, CCR
    Move_EA_SR,      // move.w <ea>, SR
    Move_CCR_EA,     // move.w CCR, <ea>
    Move_SR_EA,      // move.w SR, <ea>
    Move_An_USP,     // move An, USP
    Move_USP_An,     // move USP, An
    MoveA_W,         // movea.w <ea>, An
    MoveA_L,         // movea.l <ea>, An
    MoveM_EA_Rs_C_W, // movem.w <ea>, <list>  (<ea> uses PC)
    MoveM_EA_Rs_C_L, // movem.l <ea>, <list>  (<ea> uses PC)
    MoveM_EA_Rs_D_W, // movem.w <ea>, <list>  (<ea> does not use PC)
    MoveM_EA_Rs_D_L, // movem.l <ea>, <list>  (<ea> does not use PC)
    MoveM_PI_Rs_W,   // movem.w (An)+, <list>
    MoveM_PI_Rs_L,   // movem.l (An)+, <list>
    MoveM_Rs_EA_W,   // movem.w <list>, <ea>
    MoveM_Rs_EA_L,   // movem.l <list>, <ea>
    MoveM_Rs_PD_W,   // movem.w <list>, -(An)
    MoveM_Rs_PD_L,   // movem.l <list>, -(An)
    MoveP_Ay_Dx_W,   // movep.w (disp,Ay), Dx
    MoveP_Ay_Dx_L,   // movep.l (disp,Ay), Dx
    MoveP_Dx_Ay_W,   // movep.w Dx, (disp,Ay)
    MoveP_Dx_Ay_L,   // movep.l Dx, (disp,Ay)
    MoveQ,           // moveq #<imm>, Dn

    Clr_B,     // clr.b <ea>
    Clr_W,     // clr.w <ea>
    Clr_L,     // clr.l <ea>
    Exg_An_An, // exg Ax, Ay
    Exg_Dn_An, // exg Dx, Ay
    Exg_Dn_Dn, // exg Dx, Dy
    Ext_W,     // ext.w Dn
    Ext_L,     // ext.l Dn
    Swap,      // swap Dn

    ABCD_M, // abcd -(Ay), -(Ax)
    ABCD_R, // abcd Dy, Dx
    NBCD,   // nbcd <ea>
    SBCD_M, // sbcd -(Ay), -(Ax)
    SBCD_R, // sbcd Dy, Dx

    Add_Dn_EA_B, // add.b Dn, <ea>
    Add_Dn_EA_W, // add.w Dn, <ea>
    Add_Dn_EA_L, // add.l Dn, <ea>
    Add_EA_Dn_B, // add.b <ea>, Dn
    Add_EA_Dn_W, // add.w <ea>, Dn
    Add_EA_Dn_L, // add.l <ea>, Dn
    AddA_W,      // adda.w <ea>, An
    AddA_L,      // adda.l <ea>, An
    AddI_B,      // addi.b #<data>, <ea>
    AddI_W,      // addi.w #<data>, <ea>
    AddI_L,      // addi.l #<data>, <ea>
    AddQ_An_W,   // addq.w #<data>, An
    AddQ_An_L,   // addq.l #<data>, An
    AddQ_EA_B,   // addq.b #<data>, <ea>
    AddQ_EA_W,   // addq.w #<data>, <ea>
    AddQ_EA_L,   // addq.l #<data>, <ea>
    AddX_M_B,    // addx.b -(Ay), -(Ax)
    AddX_M_W,    // addx.w -(Ay), -(Ax)
    AddX_M_L,    // addx.l -(Ay), -(Ax)
    AddX_R_B,    // addx.b Dy, Dx
    AddX_R_W,    // addx.w Dy, Dx
    AddX_R_L,    // addx.l Dy, Dx
    And_Dn_EA,   // and.<sz> Dn, <ea>
    And_EA_Dn,   // and.<sz> <ea>, Dn
    AndI_EA,     // andi.<sz> #<data>, <ea>
    AndI_CCR,    // andi.w #<data>, CCR
    AndI_SR,     // andi.w #<data>, SR
    Eor_Dn_EA,   // eor.<sz> Dn, <ea>
    EorI_EA,     // eori.<sz> #<data>, <ea>
    EorI_CCR,    // eori.w #<data>, CCR
    EorI_SR,     // eori.w #<data>, SR
    Neg,         // neg.<sz> <ea>
    NegX,        // negx.<sz> <ea>
    Not,         // not.<sz> <ea>
    Or_Dn_EA,    // or.<sz> Dn, <ea>
    Or_EA_Dn,    // or.<sz> <ea>, Dn
    OrI_EA,      // ori.<sz> #<data>, <ea>
    OrI_CCR,     // ori.w #<data>, CCR
    OrI_SR,      // ori.w #<data>, SR
    Sub_Dn_EA,   // sub.<sz> Dn, <ea>
    Sub_EA_Dn,   // sub.<sz> <ea>, Dn
    SubA,        // suba.<sz> <ea>, An
    SubI,        // subi.<sz> #<data>, <ea>
    SubQ_An,     // subq.<sz> #<data>, An
    SubQ_EA,     // subq.<sz> #<data>, <ea>
    SubX_M,      // subx.<sz> -(Ay), -(Ax)
    SubX_R,      // subx.<sz> Dy, Dx

    DivS, // divs <ea>, Dn
    DivU, // divu <ea>, Dn
    MulS, // muls <ea>, Dn
    MulU, // mulu <ea>, Dn

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
};

struct DecodeTable {
    alignas(std::hardware_destructive_interference_size) std::array<OpcodeType, 0x10000> opcodeTypes;
    // TODO: disassembly info/function pointers?
};

DecodeTable BuildDecodeTable();

extern DecodeTable g_decodeTable;

} // namespace satemu::m68k
