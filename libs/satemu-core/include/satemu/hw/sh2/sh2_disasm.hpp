#pragma once

#include <satemu/core/types.hpp>

#include <array>
#include <new>

namespace satemu::sh2 {

enum class Mnemonic : uint8 {
    NOP,     // nop
    SLEEP,   // sleep
    MOV,     // mov
    MOVA,    // mova
    MOVT,    // movt
    CLRT,    // clrt
    SETT,    // sett
    EXTU,    // extu
    EXTS,    // exts
    SWAP,    // swap
    XTRCT,   // xtrct
    LDC,     // ldc
    LDS,     // lds
    STC,     // stc
    STS,     // sts
    ADD,     // add
    ADDC,    // addc
    ADDV,    // addv
    AND,     // and
    NEG,     // neg
    NEGC,    // negc
    NOT,     // not
    OR,      // or
    ROTCL,   // rotcl
    ROTCR,   // rotcr
    ROTL,    // rotl
    ROTR,    // rotr
    SHAL,    // shal
    SHAR,    // shar
    SHLL,    // shll
    SHLL2,   // shll2
    SHLL8,   // shll8
    SHLL16,  // shll16
    SHLR,    // shlr
    SHLR2,   // shlr2
    SHLR8,   // shlr8
    SHLR16,  // shlr16
    SUB,     // sub
    SUBC,    // subc
    SUBV,    // subv
    XOR,     // xor
    DT,      // dt
    CLRMAC,  // clrmac
    MAC,     // mac
    MUL,     // mul
    MULS,    // muls
    MULU,    // mulu
    DMULS,   // dmuls
    DMULU,   // dmulu
    DIV0S,   // div0s
    DIV0U,   // div0u
    DIV1,    // div1
    CMP_EQ,  // cmp/eq
    CMP_GE,  // cmp/ge
    CMP_GT,  // cmp/gt
    CMP_HI,  // cmp/hi
    CMP_HS,  // cmp/hs
    CMP_PL,  // cmp/pl
    CMP_PZ,  // cmp/pz
    CMP_STR, // cmp/str
    TAS,     // tas
    TST,     // tst
    BF,      // bf
    BFS,     // bf/s
    BT,      // bt
    BTS,     // bt/s
    BRA,     // bra
    BRAF,    // braf
    BSR,     // bsr
    BSRF,    // bsrf
    JMP,     // jmp
    JSR,     // jsr
    TRAPA,   // trapa
    RTE,     // rte
    RTS,     // rts
    Illegal, // (illegal instruction)
};

enum class OperandSize : uint8 {
    Byte,         // <op>.b
    Word,         // <op>.w
    Long,         // <op>.l
    LongImplicit, // <op>  (reg-reg transfers)
    None,         // <op>  (no transfers, e.g. NOP, SLEEP, etc.)
};

struct Operand {
    enum class Type : uint8 {
        None,
        Imm,       // #imm
        Rn,        // Rn
        AtRn,      // @Rn
        AtRnPlus,  // @Rn+
        AtMinusRn, // @-Rn
        AtDispRn,  // @(disp,Rn)
        AtR0Rn,    // @(R0,Rn)
        AtDispGBR, // @(disp,GBR)
        AtR0GBR,   // @(R0,GBR)
        AtDispPC,  // @(disp,PC)
        DispPC,    // disp[+PC]
        RnPC,      // Rn[+PC]
        SR,        // SR
        GBR,       // GBR
        VBR,       // VBR
        MACH,      // MACH
        MACL,      // MACL
        PR,        // PR
    };
    Type type;

    static Operand None() {
        return {.type = Type::None};
    }

    // #imm
    static Operand Imm(sint32 imm) {
        return {.type = Type::Imm, .immDisp = imm};
    }

    // Rn
    static Operand Rn(uint8 rn) {
        return {.type = Type::Rn, .reg = rn};
    }

    // @Rn
    static Operand AtRn(uint8 rn) {
        return {.type = Type::AtRn, .reg = rn};
    }

    // @Rn+
    static Operand AtRnPlus(uint8 rn) {
        return {.type = Type::AtRnPlus, .reg = rn};
    }

    // @-Rn
    static Operand AtMinusRn(uint8 rn) {
        return {.type = Type::AtMinusRn, .reg = rn};
    }

    // @(disp,Rn)
    static Operand AtDispRn(uint8 rn, sint32 disp) {
        return {.type = Type::AtDispRn, .reg = rn, .immDisp = disp};
    }

    // @(R0,Rn)
    static Operand AtR0Rn(uint8 rn) {
        return {.type = Type::AtR0Rn, .reg = rn};
    }

    // @(disp,GBR)
    static Operand AtDispGBR(sint32 disp) {
        return {.type = Type::AtDispGBR, .immDisp = disp};
    }

    // @(R0,GBR)
    static Operand AtR0GBR() {
        return {.type = Type::AtR0GBR};
    }

    // @(disp,PC)
    static Operand AtDispPC(sint32 disp) {
        return {.type = Type::AtDispPC, .immDisp = disp};
    }

    // disp[+PC]
    static Operand DispPC(sint32 disp) {
        return {.type = Type::DispPC, .immDisp = disp};
    }

    // Rn[+PC]
    static Operand RnPC(uint8 rn) {
        return {.type = Type::RnPC, .reg = rn};
    }

    // SR
    static Operand SR() {
        return {.type = Type::SR};
    }

    // GBR
    static Operand GBR() {
        return {.type = Type::GBR};
    }

    // VBR
    static Operand VBR() {
        return {.type = Type::VBR};
    }

    // MACH
    static Operand MACH() {
        return {.type = Type::MACH};
    }
    // MACL
    static Operand MACL() {
        return {.type = Type::MACL};
    }

    // PR
    static Operand PR() {
        return {.type = Type::PR};
    }

    uint8 reg;
    sint32 immDisp;
};

struct OpcodeDisasm {
    bool validInDelaySlot = true;
    Mnemonic mnemonic = Mnemonic::Illegal;
    OperandSize opSize = OperandSize::None;
    Operand op1 = Operand::None();
    Operand op2 = Operand::None();
};

struct DisasmTable {
private:
    static constexpr auto alignment = std::hardware_destructive_interference_size;

public:
    alignas(alignment) std::array<OpcodeDisasm, 0x10000> disasm;
};

DisasmTable BuildDisasmTable();

extern DisasmTable g_disasmTable;

} // namespace satemu::sh2
