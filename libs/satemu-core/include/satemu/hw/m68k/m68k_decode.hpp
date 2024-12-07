#pragma once

#include <satemu/core_types.hpp>

#include <satemu/util/bit_ops.hpp>

#include <array>

namespace satemu::m68k {

// Condition code table
constexpr auto kCondTable = [] {
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
    Move_EA_SR,  // move.w <ea>, sr
    MoveA,       // movea.<sz> <ea>, an
    MoveM_EA_Rs, // movem.<sz> <ea>, <list>
    MoveM_PI_Rs, // movem.<sz> (An)+, <list>
    MoveM_Rs_EA, // movem.<sz> <list>, <ea>
    MoveM_Rs_PD, // movem.<sz> <list>, -(An)
    MoveQ,       // moveq #<imm>, dn

    Swap, // swap dn

    AddA,     // adda.<sz> <ea>, an
    AddI,     // addi.<sz> #<data>, <ea>
    AndI_EA,  // andi.<sz> #<data>, <ea>
    AddQ_An,  // addq.<sz> #<data>, an
    AddQ_EA,  // addq.<sz> #<data>, <ea>
    Or_Dn_EA, // or.<sz> Dn, <ea>
    Or_EA_Dn, // or.<sz> <ea>, Dn
    SubI,     // subi.<sz> #<data>, <ea>

    Cmp, // cmp.<sz> <ea>, Dn

    LEA, // lea <ea>, an

    BRA,  // bra <label>
    BSR,  // bsr <label>
    Bcc,  // b<cc> <label>
    DBcc, // db<cc>.<sz> dn, <label>
    JSR,  // jsr <ea>

    RTS, // rts

    Illegal,   // any illegal instruction, including the designated ILLEGAL instruction 0100 1010 1111 1100
    Undecoded, // instructions that the decoding table is missing; should not happen
};

struct DecodeTable {
    std::array<OpcodeType, 0x10000> opcodeTypes;
    // TODO: disassembly info/function pointers?
};

DecodeTable BuildDecodeTable();

extern DecodeTable g_decodeTable;

} // namespace satemu::m68k
