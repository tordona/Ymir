#pragma once

#include <ymir/core/types.hpp>

#include <array>
#include <functional>

namespace ymir::m68k {

enum class Mnemonic : uint8 {
    Move,   // move
    MoveA,  // movea
    MoveM,  // movem
    MoveP,  // movep
    MoveQ,  // moveq
    Clr,    // clr
    Exg,    // exg
    Ext,    // ext
    Swap,   // swap
    ABCD,   // abcd
    NBCD,   // nbcd
    SBCD,   // sbcd
    Add,    // add
    AddA,   // adda
    AddI,   // addi
    AddQ,   // addq
    AddX,   // addx
    And,    // and
    AndI,   // andi
    Eor,    // eor
    EorI,   // eori
    Neg,    // neg
    NegX,   // negx
    Not,    // not
    Or,     // or
    OrI,    // ori
    Sub,    // sub
    SubA,   // suba
    SubI,   // subi
    SubQ,   // subq
    SubX,   // subx
    DivS,   // divs
    DivU,   // divu
    MulS,   // muls
    MulU,   // mulu
    BChg,   // bchg
    BClr,   // bclr
    BSet,   // bset
    BTst,   // btst
    ASL,    // asl
    ASR,    // asr
    LSL,    // lsl
    LSR,    // lsr
    ROL,    // rol
    ROR,    // ror
    ROXL,   // roxl
    ROXR,   // roxr
    Cmp,    // cmp
    CmpA,   // cmpa
    CmpI,   // cmpi
    CmpM,   // cmpm
    Scc,    // s<cc>
    TAS,    // tas
    Tst,    // tst
    LEA,    // lea
    PEA,    // pea
    Link,   // link
    Unlink, // unlk
    BRA,    // bra
    BSR,    // bsr
    Bcc,    // b<cc>
    DBcc,   // db<cc>
    JSR,    // jsr
    Jmp,    // jmp
    RTE,    // rte
    RTR,    // rtr
    RTS,    // rts
    Chk,    // chk
    Reset,  // reset
    Stop,   // stop
    Trap,   // trap
    TrapV,  // trapv
    Noop,   // nop

    Illegal1010, // illegal instructions with bits 15-12 = 1010
    Illegal1111, // illegal instructions with bits 15-12 = 1111
    Illegal,     // any other illegal instruction, including the designated ILLEGAL instruction 0100 1010 1111 1100
};

enum class Condition : uint8 {
    T,  // always true
    F,  // always false
    HI, // !C and !Z
    LS, // C or Z
    CC, // !C
    CS, // C
    NE, // !Z
    EQ, // Z
    VC, // !V
    VS, // V
    PL, // !N
    MI, // N
    GE, // N == V
    LT, // N != V
    GT, // N == V and !Z
    LE, // N != V or Z
};

enum class OperandSize : uint8 {
    Byte,         // <op>.b
    Word,         // <op>.w
    Long,         // <op>.l
    ByteImplicit, // <op>  (implicit byte transfers, e.g. NBCD)
    WordImplicit, // <op>  (implicit word transfers, e.g. STOP)
    LongImplicit, // <op>  (implicit longword transfers, e.g. MOVE <reg>, <reg>)
    None,         // <op>  (no transfers, e.g. NOP, RESET, TRAPV, etc.)
};

struct Operand {
    enum class Type : uint8 {
        None,

        // Effective addresses
        Dn,           // Dn
        An,           // An
        AtAn,         // (An)
        AtAnPlus,     // (An)+
        MinusAtAn,    // -(An)
        AtDispAn,     // (disp,An)
        AtDispAnIx,   // (disp,An,<ix>)
        AtDispPC,     // (disp,PC)
        AtDispPCIx,   // (disp,PC,<ix>)
        AtImmWord,    // (xxx).w
        AtImmLong,    // (xxx).l
        SImmEmbedded, // #simm (embedded in opcode)
        UImmEmbedded, // #uimm (embedded in opcode)
        SImmFetched,  // #simm (fetched from next word(s))
        UImmFetched,  // #uimm (fetched from next word(s))

        CCR, // CCR
        SR,  // SR
        USP, // USP

        RegList, // <list>  (movem)
    };

    Type type;
    bool read, write;
    union {
        uint8 rn;    // Dn, An: 0-7 = D0-7, 8-15 = A0-7
        uint16 uimm; // #uimm (embedded in opcode)
        sint16 simm; // #simm (embedded in opcode)
    };

    static Operand None() {
        return {.type = Type::None};
    }

    static Operand Dn_R(uint8 rn) {
        return {.type = Type::Dn, .read = true, .write = false, .rn = rn};
    }
    static Operand Dn_W(uint8 rn) {
        return {.type = Type::Dn, .read = false, .write = true, .rn = rn};
    }
    static Operand Dn_RW(uint8 rn) {
        return {.type = Type::Dn, .read = true, .write = true, .rn = rn};
    }

    static Operand An_R(uint8 rn) {
        return {.type = Type::An, .read = true, .write = false, .rn = rn};
    }
    static Operand An_W(uint8 rn) {
        return {.type = Type::An, .read = false, .write = true, .rn = rn};
    }
    static Operand An_RW(uint8 rn) {
        return {.type = Type::An, .read = true, .write = true, .rn = rn};
    }

    static Operand AtAn_R(uint8 rn) {
        return {.type = Type::AtAn, .read = true, .write = false, .rn = rn};
    }
    static Operand AtAn_W(uint8 rn) {
        return {.type = Type::AtAn, .read = false, .write = true, .rn = rn};
    }
    static Operand AtAn_RW(uint8 rn) {
        return {.type = Type::AtAn, .read = true, .write = true, .rn = rn};
    }

    static Operand AtAnPlus_R(uint8 rn) {
        return {.type = Type::AtAnPlus, .read = true, .write = false, .rn = rn};
    }
    static Operand AtAnPlus_W(uint8 rn) {
        return {.type = Type::AtAnPlus, .read = false, .write = true, .rn = rn};
    }
    static Operand AtAnPlus_RW(uint8 rn) {
        return {.type = Type::AtAnPlus, .read = true, .write = true, .rn = rn};
    }

    static Operand MinusAtAn_R(uint8 rn) {
        return {.type = Type::MinusAtAn, .read = true, .write = false, .rn = rn};
    }
    static Operand MinusAtAn_W(uint8 rn) {
        return {.type = Type::MinusAtAn, .read = false, .write = true, .rn = rn};
    }
    static Operand MinusAtAn_RW(uint8 rn) {
        return {.type = Type::MinusAtAn, .read = true, .write = true, .rn = rn};
    }

    static Operand AtDispAn_R(uint8 rn) {
        return {.type = Type::AtDispAn, .read = true, .write = false, .rn = rn};
    }
    static Operand AtDispAn_W(uint8 rn) {
        return {.type = Type::AtDispAn, .read = false, .write = true, .rn = rn};
    }
    static Operand AtDispAn_RW(uint8 rn) {
        return {.type = Type::AtDispAn, .read = true, .write = true, .rn = rn};
    }

    static Operand AtDispAnIx_R(uint8 rn) {
        return {.type = Type::AtDispAnIx, .read = true, .write = false, .rn = rn};
    }
    static Operand AtDispAnIx_W(uint8 rn) {
        return {.type = Type::AtDispAnIx, .read = false, .write = true, .rn = rn};
    }
    static Operand AtDispAnIx_RW(uint8 rn) {
        return {.type = Type::AtDispAnIx, .read = true, .write = true, .rn = rn};
    }

    static Operand AtDispPC_R() {
        return {.type = Type::AtDispPC, .read = true, .write = false};
    }

    static Operand AtDispPCIx_R() {
        return {.type = Type::AtDispPCIx, .read = true, .write = false};
    }

    static Operand AtImmWord_R() {
        return {.type = Type::AtImmWord, .read = true, .write = false};
    }
    static Operand AtImmWord_W() {
        return {.type = Type::AtImmWord, .read = false, .write = true};
    }
    static Operand AtImmWord_RW() {
        return {.type = Type::AtImmWord, .read = true, .write = true};
    }

    static Operand AtImmLong_R() {
        return {.type = Type::AtImmLong, .read = true, .write = false};
    }
    static Operand AtImmLong_W() {
        return {.type = Type::AtImmLong, .read = false, .write = true};
    }
    static Operand AtImmLong_RW() {
        return {.type = Type::AtImmLong, .read = true, .write = true};
    }

    static Operand SImmEmbedded(sint16 simm) {
        return {.type = Type::SImmEmbedded, .simm = simm};
    }
    static Operand UImmEmbedded(uint16 uimm) {
        return {.type = Type::UImmEmbedded, .uimm = uimm};
    }
    static Operand SImmFetched() {
        return {.type = Type::SImmFetched};
    }
    static Operand UImmFetched() {
        return {.type = Type::UImmFetched};
    }

    static Operand CCR_R() {
        return {.type = Type::CCR, .read = true, .write = false};
    }
    static Operand CCR_W() {
        return {.type = Type::CCR, .read = false, .write = true};
    }

    static Operand SR_R() {
        return {.type = Type::SR, .read = true, .write = false};
    }
    static Operand SR_W() {
        return {.type = Type::SR, .read = false, .write = true};
    }

    static Operand USP_R() {
        return {.type = Type::USP, .read = true, .write = false};
    }
    static Operand USP_W() {
        return {.type = Type::USP, .read = false, .write = true};
    }

    static Operand RegList_R() {
        return {.type = Type::RegList, .read = true, .write = false};
    }
    static Operand RegList_W() {
        return {.type = Type::RegList, .read = false, .write = true};
    }
};

struct OpcodeDisasm {
    Mnemonic mnemonic = Mnemonic::Illegal;
    Condition cond = Condition::T;
    OperandSize opSize = OperandSize::None;
    bool privileged = false;
    Operand op1 = Operand::None();
    Operand op2 = Operand::None();
};

struct OperandDetails {
    sint32 immDisp; // #imm, disp
    uint16 regList; // <list> (movem)
    uint8 ix;       // <ix>: 0-7 = D0-7, 8-15 = A0-7
};

struct FullDisasm {
    const OpcodeDisasm &opcode;
    OperandDetails op1;
    OperandDetails op2;
};

struct DisasmTable {
private:
    static constexpr auto alignment = 64;

    DisasmTable();

public:
    static DisasmTable s_instance;

    alignas(alignment) std::array<OpcodeDisasm, 0x10000> disasm;
};

// Disassembles M68K code, reading opcodes using the given fetcher function.
// Every invocation of the function must return successive words starting from the base opcode.
FullDisasm Disassemble(std::function<uint16()> fetcher);

} // namespace ymir::m68k
