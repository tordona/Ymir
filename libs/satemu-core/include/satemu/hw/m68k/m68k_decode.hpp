#pragma once

#include <satemu/core_types.hpp>

#include <array>

namespace satemu::m68k {

enum class OpcodeType : uint8 {
    Move_EA_EA, // move.<sz> <ea_src>, <ea_dst>
    Move_EA_SR, // move.w <ea>, sr
    MoveQ,      // moveq #<imm>, dn

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
