#include <ymir/hw/m68k/m68k_disasm.hpp>

#include "m68k_addr_modes.hpp"

#include <ymir/util/bit_ops.hpp>
#include <ymir/util/unreachable.hpp>

namespace ymir::m68k {

DisasmTable::DisasmTable() {
    for (uint32 instr = 0; instr < 0x10000; instr++) {
        OpcodeDisasm &disasm = this->disasm[instr];

        // ---------------------------------------

        using Op = Operand;
        using Cond = Condition;

        // <op>
        auto make0 = [&](Mnemonic mnemonic) { disasm.mnemonic = mnemonic; };

        // <op>.b <op1>, <op2>
        auto makeOpB = [&](Mnemonic mnemonic, Op op1, Op op2 = Op::None()) {
            disasm.mnemonic = mnemonic;
            disasm.opSize = OperandSize::Byte;
            disasm.op1 = op1;
            disasm.op2 = op2;
        };

        // <op>.w <op1>, <op2>
        auto makeOpW = [&](Mnemonic mnemonic, Op op1, Op op2 = Op::None()) {
            disasm.mnemonic = mnemonic;
            disasm.opSize = OperandSize::Word;
            disasm.op1 = op1;
            disasm.op2 = op2;
        };

        // <op>.l <op1>, <op2>
        auto makeOpL = [&](Mnemonic mnemonic, Op op1, Op op2 = Op::None()) {
            disasm.mnemonic = mnemonic;
            disasm.opSize = OperandSize::Long;
            disasm.op1 = op1;
            disasm.op2 = op2;
        };

        // <op> <op1>, <op2> (implicit byte operand)
        auto makeOpBI = [&](Mnemonic mnemonic, Op op1, Op op2 = Op::None()) {
            disasm.mnemonic = mnemonic;
            disasm.opSize = OperandSize::ByteImplicit;
            disasm.op1 = op1;
            disasm.op2 = op2;
        };

        // <op> <op1>, <op2> (implicit word operand)
        auto makeOpWI = [&](Mnemonic mnemonic, Op op1, Op op2 = Op::None()) {
            disasm.mnemonic = mnemonic;
            disasm.opSize = OperandSize::WordImplicit;
            disasm.op1 = op1;
            disasm.op2 = op2;
        };

        // <op> <op1>, <op2> (implicit long operand)
        auto makeOpLI = [&](Mnemonic mnemonic, Op op1, Op op2 = Op::None()) {
            disasm.mnemonic = mnemonic;
            disasm.opSize = OperandSize::LongImplicit;
            disasm.op1 = op1;
            disasm.op2 = op2;
        };

        // <op> <op1>, <op2> (no transfers)
        auto makeOpN = [&](Mnemonic mnemonic, Op op1, Op op2 = Op::None()) {
            disasm.mnemonic = mnemonic;
            disasm.opSize = OperandSize::None;
            disasm.op1 = op1;
            disasm.op2 = op2;
        };

        // <ea>
        auto eaOp_R = [](uint8 ea) {
            const uint16 Xn = bit::extract<0, 2>(ea);
            const uint16 M = bit::extract<3, 5>(ea);
            switch (M) {
            case 0b000: return Op::Dn_R(Xn);
            case 0b001: return Op::An_R(Xn);
            case 0b010: return Op::AtAn_R(Xn);
            case 0b011: return Op::AtAnPlus_R(Xn);
            case 0b100: return Op::MinusAtAn_R(Xn);
            case 0b101: return Op::AtDispAn_R(Xn);
            case 0b110: return Op::AtDispAnIx_R(Xn);
            case 0b111:
                switch (Xn) {
                case 0b010: return Op::AtDispPC_R();
                case 0b011: return Op::AtDispPCIx_R();
                case 0b000: return Op::AtImmWord_R();
                case 0b001: return Op::AtImmLong_R();
                case 0b100: return Op::UImmFetched();
                }
                break;
            }

            util::unreachable();
        };
        auto eaOp_W = [](uint8 ea) {
            const uint16 Xn = bit::extract<0, 2>(ea);
            const uint16 M = bit::extract<3, 5>(ea);
            switch (M) {
            case 0b000: return Op::Dn_W(Xn);
            case 0b001: return Op::An_W(Xn);
            case 0b010: return Op::AtAn_W(Xn);
            case 0b011: return Op::AtAnPlus_W(Xn);
            case 0b100: return Op::MinusAtAn_W(Xn);
            case 0b101: return Op::AtDispAn_W(Xn);
            case 0b110: return Op::AtDispAnIx_W(Xn);
            case 0b111:
                switch (Xn) {
                case 0b000: return Op::AtImmWord_W();
                case 0b001: return Op::AtImmLong_W();
                }
                break;
            }

            util::unreachable();
        };
        auto eaOp_RW = [](uint8 ea) {
            const uint16 Xn = bit::extract<0, 2>(ea);
            const uint16 M = bit::extract<3, 5>(ea);
            switch (M) {
            case 0b000: return Op::Dn_RW(Xn);
            case 0b001: return Op::An_RW(Xn);
            case 0b010: return Op::AtAn_RW(Xn);
            case 0b011: return Op::AtAnPlus_RW(Xn);
            case 0b100: return Op::MinusAtAn_RW(Xn);
            case 0b101: return Op::AtDispAn_RW(Xn);
            case 0b110: return Op::AtDispAnIx_RW(Xn);
            case 0b111:
                switch (Xn) {
                case 0b000: return Op::AtImmWord_RW();
                case 0b001: return Op::AtImmLong_RW();
                }
                break;
            }

            util::unreachable();
        };

        auto privileged = [&] { disasm.privileged = true; };
        auto cond = [&](Condition cond) { disasm.cond = cond; };

        // ---------------------------------------

        using enum Mnemonic;

        switch (instr >> 12u) {
        case 0x0: {
            const uint16 ea = bit::extract<0, 5>(instr);
            const uint16 sz = bit::extract<6, 7>(instr);
            const uint16 Dx = bit::extract<9, 11>(instr);

            if (instr == 0x003C) {
                makeOpW(OrI, Op::UImmFetched(), Op::CCR_W());
            } else if (instr == 0x007C) {
                makeOpW(OrI, Op::UImmFetched(), Op::SR_W()), privileged();
            } else if (instr == 0x023C) {
                makeOpW(AndI, Op::UImmFetched(), Op::CCR_W());
            } else if (instr == 0x027C) {
                makeOpW(AndI, Op::UImmFetched(), Op::SR_W()), privileged();
            } else if (instr == 0x0A3C) {
                makeOpW(EorI, Op::UImmFetched(), Op::CCR_W());
            } else if (instr == 0x0A7C) {
                makeOpW(EorI, Op::UImmFetched(), Op::SR_W()), privileged();
            } else if (bit::extract<3, 5>(instr) == 0b001 && bit::extract<8>(instr) == 1) {
                const bool szBit = bit::test<6>(instr);
                const uint8 Ay = bit::extract<0, 2>(instr);
                if (bit::test<7>(instr)) {
                    if (szBit) {
                        makeOpL(MoveP, Op::Dn_R(Dx), Op::AtDispAn_W(Ay));
                    } else {
                        makeOpW(MoveP, Op::Dn_R(Dx), Op::AtDispAn_W(Ay));
                    }
                } else {
                    if (szBit) {
                        makeOpL(MoveP, Op::AtDispAn_R(Ay), Op::Dn_W(Dx));
                    } else {
                        makeOpW(MoveP, Op::AtDispAn_R(Ay), Op::Dn_W(Dx));
                    }
                }
            } else if (bit::extract<6, 8>(instr) == 0b100) {
                if ((ea >> 3u) == 0b000) {
                    makeOpL(BTst, Op::Dn_R(Dx), Op::Dn_W(bit::extract<0, 2>(ea)));
                } else if (kValidDataAddrModes[ea]) {
                    makeOpB(BTst, Op::Dn_R(Dx), eaOp_R(ea));
                }
            } else if (bit::extract<6, 8>(instr) == 0b101) {
                if ((ea >> 3u) == 0b000) {
                    makeOpL(BChg, Op::Dn_R(Dx), Op::Dn_W(bit::extract<0, 2>(ea)));
                } else if (kValidDataAlterableAddrModes[ea]) {
                    makeOpB(BChg, Op::Dn_R(Dx), eaOp_RW(ea));
                }
            } else if (bit::extract<6, 8>(instr) == 0b110) {
                if ((ea >> 3u) == 0b000) {
                    makeOpL(BClr, Op::Dn_R(Dx), Op::Dn_W(bit::extract<0, 2>(ea)));
                } else if (kValidDataAlterableAddrModes[ea]) {
                    makeOpB(BClr, Op::Dn_R(Dx), eaOp_RW(ea));
                }
            } else if (bit::extract<6, 8>(instr) == 0b111) {
                if ((ea >> 3u) == 0b000) {
                    makeOpL(BSet, Op::Dn_R(Dx), Op::Dn_W(bit::extract<0, 2>(ea)));
                } else if (kValidDataAlterableAddrModes[ea]) {
                    makeOpB(BSet, Op::Dn_R(Dx), eaOp_RW(ea));
                }
            } else if (bit::extract<6, 11>(instr) == 0b100000) {
                if ((ea >> 3u) == 0b000) {
                    makeOpL(BTst, Op::UImmFetched(), Op::Dn_W(bit::extract<0, 2>(ea)));
                } else if (kValidDataAddrModes[ea]) {
                    makeOpB(BTst, Op::UImmFetched(), eaOp_R(ea));
                }
            } else if (bit::extract<6, 11>(instr) == 0b100001) {
                if ((ea >> 3u) == 0b000) {
                    makeOpL(BChg, Op::UImmFetched(), Op::Dn_W(bit::extract<0, 2>(ea)));
                } else if (kValidDataAlterableAddrModes[ea]) {
                    makeOpB(BChg, Op::UImmFetched(), eaOp_RW(ea));
                }
            } else if (bit::extract<6, 11>(instr) == 0b100010) {
                if ((ea >> 3u) == 0b000) {
                    makeOpL(BClr, Op::UImmFetched(), Op::Dn_W(bit::extract<0, 2>(ea)));
                } else if (kValidDataAlterableAddrModes[ea]) {
                    makeOpB(BClr, Op::UImmFetched(), eaOp_RW(ea));
                }
            } else if (bit::extract<6, 11>(instr) == 0b100011) {
                if ((ea >> 3u) == 0b000) {
                    makeOpL(BSet, Op::UImmFetched(), Op::Dn_W(bit::extract<0, 2>(ea)));
                } else if (kValidDataAlterableAddrModes[ea]) {
                    makeOpB(BSet, Op::UImmFetched(), eaOp_RW(ea));
                }
            } else if (bit::extract<8, 11>(instr) == 0b0000) {
                if (kValidDataAlterableAddrModes[ea]) {
                    switch (sz) {
                    case 0b00: makeOpB(OrI, Op::UImmFetched(), eaOp_RW(ea)); break;
                    case 0b01: makeOpW(OrI, Op::UImmFetched(), eaOp_RW(ea)); break;
                    case 0b10: makeOpL(OrI, Op::UImmFetched(), eaOp_RW(ea)); break;
                    }
                }
            } else if (bit::extract<8, 11>(instr) == 0b0010) {
                if (kValidDataAlterableAddrModes[ea]) {
                    switch (sz) {
                    case 0b00: makeOpB(AndI, Op::UImmFetched(), eaOp_RW(ea)); break;
                    case 0b01: makeOpW(AndI, Op::UImmFetched(), eaOp_RW(ea)); break;
                    case 0b10: makeOpL(AndI, Op::UImmFetched(), eaOp_RW(ea)); break;
                    }
                }
            } else if (bit::extract<8, 11>(instr) == 0b1010) {
                if (kValidDataAlterableAddrModes[ea]) {
                    switch (sz) {
                    case 0b00: makeOpB(EorI, Op::UImmFetched(), eaOp_RW(ea)); break;
                    case 0b01: makeOpW(EorI, Op::UImmFetched(), eaOp_RW(ea)); break;
                    case 0b10: makeOpL(EorI, Op::UImmFetched(), eaOp_RW(ea)); break;
                    }
                }
            } else if (bit::extract<8, 11>(instr) == 0b0100) {
                if (kValidDataAlterableAddrModes[ea]) {
                    switch (sz) {
                    case 0b00: makeOpB(SubI, Op::UImmFetched(), eaOp_RW(ea)); break;
                    case 0b01: makeOpW(SubI, Op::UImmFetched(), eaOp_RW(ea)); break;
                    case 0b10: makeOpL(SubI, Op::UImmFetched(), eaOp_RW(ea)); break;
                    }
                }
            } else if (bit::extract<8, 11>(instr) == 0b0110) {
                if (kValidDataAlterableAddrModes[ea]) {
                    switch (sz) {
                    case 0b00: makeOpB(AddI, Op::UImmFetched(), eaOp_RW(ea)); break;
                    case 0b01: makeOpW(AddI, Op::UImmFetched(), eaOp_RW(ea)); break;
                    case 0b10: makeOpL(AddI, Op::UImmFetched(), eaOp_RW(ea)); break;
                    }
                }
            } else if (bit::extract<8, 11>(instr) == 0b1100) {
                if (kValidDataAddrModes[ea]) {
                    switch (sz) {
                    case 0b00: makeOpB(CmpI, Op::UImmFetched(), eaOp_R(ea)); break;
                    case 0b01: makeOpW(CmpI, Op::UImmFetched(), eaOp_R(ea)); break;
                    case 0b10: makeOpL(CmpI, Op::UImmFetched(), eaOp_R(ea)); break;
                    }
                }
            }
            break;
        }
        case 0x1: [[fallthrough]];
        case 0x2: [[fallthrough]];
        case 0x3:
            if (bit::extract<6, 8>(instr) == 0b001) {
                const uint16 ea = bit::extract<0, 5>(instr);

                if (kValidAddrModes[ea]) {
                    const uint16 An = bit::extract<9, 11>(instr);
                    const uint16 size = bit::extract<12, 13>(instr);
                    switch (size) {
                    case 0b11: makeOpW(MoveA, eaOp_R(ea), Op::An_W(An)); break;
                    case 0b10: makeOpL(MoveA, eaOp_R(ea), Op::An_W(An)); break;
                    }
                }
            } else {
                const uint16 srcEA = bit::extract<0, 5>(instr);
                const uint16 dstEA = (bit::extract<6, 8>(instr) << 3) | bit::extract<9, 11>(instr);

                if (kValidDataAlterableAddrModes[dstEA] && kValidAddrModes[srcEA]) {
                    // Note the swapped bit order between word and longword moves
                    const uint16 size = bit::extract<12, 13>(instr);
                    switch (size) {
                    case 0b01: makeOpB(Move, eaOp_R(srcEA), eaOp_W(dstEA)); break;
                    case 0b11: makeOpW(Move, eaOp_R(srcEA), eaOp_W(dstEA)); break;
                    case 0b10: makeOpL(Move, eaOp_R(srcEA), eaOp_W(dstEA)); break;
                    }
                }
            }
            break;
        case 0x4: {
            const uint16 ea = bit::extract<0, 5>(instr);
            const uint16 sz = bit::extract<6, 7>(instr);
            if (instr == 0x4E70) {
                make0(Reset), privileged();
            } else if (instr == 0x4E71) {
                make0(Noop);
            } else if (instr == 0x4E72) {
                makeOpWI(Stop, Op::UImmFetched()), privileged();
            } else if (instr == 0x4E73) {
                make0(RTE), privileged();
            } else if (instr == 0x4E75) {
                make0(RTS);
            } else if (instr == 0x4E76) {
                make0(TrapV);
            } else if (instr == 0x4E77) {
                make0(RTR);
            } else if (instr == 0x4AFC) {
                make0(Illegal);
            } else if (bit::extract<3, 11>(instr) == 0b100001000) {
                const uint32 Dn = bit::extract<0, 2>(instr);
                makeOpLI(Swap, Op::Dn_RW(Dn));
            } else if (bit::extract<3, 11>(instr) == 0b100010000) {
                const uint32 Dn = bit::extract<0, 2>(instr);
                makeOpWI(Ext, Op::Dn_RW(Dn));
            } else if (bit::extract<3, 11>(instr) == 0b100011000) {
                const uint32 Dn = bit::extract<0, 2>(instr);
                makeOpLI(Ext, Op::Dn_RW(Dn));
            } else if (bit::extract<3, 11>(instr) == 0b111001010) {
                const uint16 An = bit::extract<0, 2>(instr);
                makeOpWI(Link, Op::An_R(An), Op::SImmFetched());
            } else if (bit::extract<3, 11>(instr) == 0b111001011) {
                const uint16 An = bit::extract<0, 2>(instr);
                makeOpLI(Unlink, Op::An_RW(An));
            } else if (bit::extract<3, 11>(instr) == 0b111001100) {
                const uint16 An = bit::extract<0, 2>(instr);
                makeOpLI(Move, Op::An_R(An), Op::USP_W()), privileged();
            } else if (bit::extract<3, 11>(instr) == 0b111001101) {
                const uint16 An = bit::extract<0, 2>(instr);
                makeOpLI(Move, Op::USP_R(), Op::An_W(An)), privileged();
            } else if (bit::extract<4, 11>(instr) == 0b11100100) {
                const uint8 vector = bit::extract<0, 3>(instr);
                makeOpBI(Trap, Op::UImmEmbedded(vector));
            } else if (bit::extract<6, 11>(instr) == 0b000011) {
                if (kValidDataAlterableAddrModes[ea]) {
                    makeOpW(Move, Op::SR_R(), eaOp_W(ea));
                }
            } else if (bit::extract<6, 11>(instr) == 0b001011) {
                if (kValidDataAlterableAddrModes[ea]) {
                    makeOpW(Move, Op::CCR_R(), eaOp_W(ea));
                }
            } else if (bit::extract<6, 11>(instr) == 0b010011) {
                if (kValidDataAddrModes[ea]) {
                    makeOpW(Move, eaOp_R(ea), Op::CCR_W());
                }
            } else if (bit::extract<6, 11>(instr) == 0b011011) {
                if (kValidDataAddrModes[ea]) {
                    makeOpW(Move, eaOp_R(ea), Op::SR_W()), privileged();
                }
            } else if (bit::extract<6, 11>(instr) == 0b100000) {
                if (kValidDataAlterableAddrModes[ea]) {
                    makeOpBI(NBCD, eaOp_RW(ea));
                }
            } else if (bit::extract<6, 11>(instr) == 0b101011) {
                if (kValidDataAlterableAddrModes[ea]) {
                    makeOpBI(TAS, eaOp_RW(ea));
                }
            } else if (bit::extract<6, 11>(instr) == 0b100001) {
                if (kValidControlAddrModes[ea]) {
                    makeOpLI(PEA, eaOp_R(ea));
                }
            } else if (bit::extract<6, 11>(instr) == 0b111010) {
                if (kValidControlAddrModes[ea]) {
                    makeOpLI(JSR, eaOp_R(ea));
                }
            } else if (bit::extract<6, 11>(instr) == 0b111011) {
                if (kValidControlAddrModes[ea]) {
                    makeOpLI(Jmp, eaOp_R(ea));
                }
            } else if (bit::extract<7, 11>(instr) == 0b10001) {
                const bool isPredecrement = (ea >> 3u) == 0b100;
                const bool sz = bit::test<6>(instr);
                if (isPredecrement) {
                    const uint16 An = bit::extract<0, 2>(instr);
                    if (sz) {
                        makeOpL(MoveM, Op::RegList_R(), Op::MinusAtAn_W(An));
                    } else {
                        makeOpW(MoveM, Op::RegList_R(), Op::MinusAtAn_W(An));
                    }
                } else if (kValidControlAlterableAddrModes[ea]) {
                    if (sz) {
                        makeOpL(MoveM, Op::RegList_R(), eaOp_W(ea));
                    }
                }
            } else if (bit::extract<7, 11>(instr) == 0b11001) {
                const bool isPostincrement = (ea >> 3u) == 0b011;
                const bool sz = bit::test<6>(instr);
                if (isPostincrement) {
                    const uint16 An = bit::extract<0, 2>(instr);
                    if (sz) {
                        makeOpL(MoveM, Op::AtAnPlus_R(An), Op::RegList_W());
                    } else {
                        makeOpW(MoveM, Op::AtAnPlus_R(An), Op::RegList_W());
                    }
                } else if (kValidControlAddrModes[ea]) {
                    const uint16 Xn = bit::extract<0, 2>(instr);
                    const uint16 M = bit::extract<3, 5>(instr);
                    if (sz) {
                        makeOpL(MoveM, eaOp_R(ea), Op::RegList_W());
                    } else {
                        makeOpW(MoveM, eaOp_R(ea), Op::RegList_W());
                    }
                }
            } else if (bit::extract<8, 11>(instr) == 0b0000) {
                if (kValidDataAlterableAddrModes[ea]) {
                    switch (sz) {
                    case 0b00: makeOpB(NegX, eaOp_RW(ea)); break;
                    case 0b01: makeOpW(NegX, eaOp_RW(ea)); break;
                    case 0b10: makeOpL(NegX, eaOp_RW(ea)); break;
                    }
                }
            } else if (bit::extract<8, 11>(instr) == 0b0010) {
                if (kValidDataAlterableAddrModes[ea]) {
                    switch (sz) {
                    case 0b00: makeOpB(Clr, eaOp_RW(ea)); break;
                    case 0b01: makeOpW(Clr, eaOp_RW(ea)); break;
                    case 0b10: makeOpL(Clr, eaOp_RW(ea)); break;
                    }
                }
            } else if (bit::extract<8, 11>(instr) == 0b0100) {
                if (kValidDataAlterableAddrModes[ea]) {
                    switch (sz) {
                    case 0b00: makeOpB(Neg, eaOp_RW(ea)); break;
                    case 0b01: makeOpW(Neg, eaOp_RW(ea)); break;
                    case 0b10: makeOpL(Neg, eaOp_RW(ea)); break;
                    }
                }
            } else if (bit::extract<8, 11>(instr) == 0b0110) {
                if (kValidDataAlterableAddrModes[ea]) {
                    switch (sz) {
                    case 0b00: makeOpB(Not, eaOp_RW(ea)); break;
                    case 0b01: makeOpW(Not, eaOp_RW(ea)); break;
                    case 0b10: makeOpL(Not, eaOp_RW(ea)); break;
                    }
                }
            } else if (bit::extract<8, 11>(instr) == 0b1010) {
                if (kValidDataAlterableAddrModes[ea]) {
                    switch (sz) {
                    case 0b00: makeOpB(Tst, eaOp_R(ea)); break;
                    case 0b01: makeOpB(Tst, eaOp_R(ea)); break;
                    case 0b10: makeOpB(Tst, eaOp_R(ea)); break;
                    }
                }
            } else if (bit::extract<6, 8>(instr) == 0b110) {
                if (kValidDataAddrModes[ea]) {
                    const uint16 Dn = bit::extract<9, 11>(instr);
                    makeOpWI(Chk, eaOp_R(ea), Op::Dn_R(Dn));
                }
            } else if (bit::extract<6, 8>(instr) == 0b111) {
                if (kValidControlAddrModes[ea]) {
                    const uint16 An = bit::extract<9, 11>(instr);
                    makeOpLI(LEA, eaOp_R(ea), Op::An_W(An));
                }
            }
            break;
        }
        case 0x5: {
            const uint16 ea = bit::extract<0, 5>(instr);
            if (bit::extract<3, 7>(instr) == 0b11001) {
                const uint32 condNum = bit::extract<8, 11>(instr);
                const uint32 Dn = bit::extract<0, 2>(instr);
                makeOpW(DBcc, Op::Dn_R(Dn), Op::SImmFetched()), cond(static_cast<Cond>(condNum));
            } else if (bit::extract<6, 7>(instr) == 0b11) {
                if (kValidDataAlterableAddrModes[ea]) {
                    const uint32 condNum = bit::extract<8, 11>(instr);
                    makeOpBI(Scc, eaOp_W(ea)), cond(static_cast<Cond>(condNum));
                }
            } else {
                const uint16 M = bit::extract<3, 5>(instr);
                const uint16 sz = bit::extract<6, 7>(instr);
                const uint16 data = bit::extract<9, 11>(instr);
                const uint32 op1 = data == 0 ? 8 : data;
                const bool isAn = M == 0b001;
                if (bit::extract<8>(instr) == 1) {
                    if (isAn) {
                        const uint16 An = bit::extract<0, 2>(instr);
                        switch (sz) {
                        case 0b01: makeOpW(SubQ, Op::UImmEmbedded(op1), Op::An_RW(An)); break;
                        case 0b10: makeOpL(SubQ, Op::UImmEmbedded(op1), Op::An_RW(An)); break;
                        }
                    } else if (kValidAlterableAddrModes[ea]) {
                        switch (sz) {
                        case 0b00: makeOpB(SubQ, Op::UImmEmbedded(op1), eaOp_RW(ea)); break;
                        case 0b01: makeOpW(SubQ, Op::UImmEmbedded(op1), eaOp_RW(ea)); break;
                        case 0b10: makeOpL(SubQ, Op::UImmEmbedded(op1), eaOp_RW(ea)); break;
                        }
                    }
                } else {
                    if (isAn) {
                        const uint16 An = bit::extract<0, 2>(instr);
                        switch (sz) {
                        case 0b01: makeOpW(AddQ, Op::UImmEmbedded(op1), Op::An_RW(An)); break;
                        case 0b10: makeOpL(AddQ, Op::UImmEmbedded(op1), Op::An_RW(An)); break;
                        }
                    } else if (kValidAlterableAddrModes[ea]) {
                        switch (sz) {
                        case 0b00: makeOpB(AddQ, Op::UImmEmbedded(op1), eaOp_RW(ea)); break;
                        case 0b01: makeOpW(AddQ, Op::UImmEmbedded(op1), eaOp_RW(ea)); break;
                        case 0b10: makeOpL(AddQ, Op::UImmEmbedded(op1), eaOp_RW(ea)); break;
                        }
                    }
                }
            }
            break;
        }
        case 0x6: {
            const sint16 disp = static_cast<sint8>(bit::extract<0, 7>(instr));
            const bool longDisp = disp == 0;
            if (longDisp) {
                switch (bit::extract<8, 11>(instr)) {
                case 0b0000: makeOpWI(BRA, Op::SImmFetched()); break;
                case 0b0001: makeOpWI(BSR, Op::SImmFetched()); break;
                default: {
                    const uint32 condNum = bit::extract<8, 11>(instr);
                    makeOpWI(Bcc, Op::SImmFetched()), cond(static_cast<Cond>(condNum));
                    break;
                }
                }
            } else {
                switch (bit::extract<8, 11>(instr)) {
                case 0b0000: makeOpN(BRA, Op::SImmEmbedded(disp)); break;
                case 0b0001: makeOpN(BSR, Op::SImmEmbedded(disp)); break;
                default: {
                    const uint32 condNum = bit::extract<8, 11>(instr);
                    makeOpN(Bcc, Op::SImmEmbedded(disp)), cond(static_cast<Cond>(condNum));
                    break;
                }
                }
            }
            break;
        }
        case 0x7: {
            if (bit::extract<8>(instr) == 0) {
                const sint16 value = static_cast<sint8>(bit::extract<0, 7>(instr));
                const uint32 Dn = bit::extract<9, 11>(instr);
                makeOpLI(MoveQ, Op::SImmEmbedded(value), Op::Dn_W(Dn));
            }
            break;
        }
        case 0x8: {
            const uint16 ea = bit::extract<0, 5>(instr);
            const uint16 sz = bit::extract<6, 7>(instr);
            const uint16 Dn = bit::extract<9, 11>(instr);
            if (bit::extract<6, 8>(instr) == 0b011) {
                if (kValidDataAddrModes[ea]) {
                    makeOpWI(DivU, eaOp_R(ea), Op::Dn_RW(Dn));
                }
            } else if (bit::extract<6, 8>(instr) == 0b111) {
                if (kValidDataAddrModes[ea]) {
                    makeOpWI(DivS, eaOp_R(ea), Op::Dn_RW(Dn));
                }
            } else if (bit::extract<3, 8>(instr) == 0b100000) {
                const uint16 Dy = bit::extract<0, 2>(instr);
                const uint16 Dx = bit::extract<9, 11>(instr);
                makeOpBI(SBCD, Op::Dn_R(Dy), Op::Dn_RW(Dx));
            } else if (bit::extract<3, 8>(instr) == 0b100001) {
                const uint16 Ay = bit::extract<0, 2>(instr);
                const uint16 Ax = bit::extract<9, 11>(instr);
                makeOpBI(SBCD, Op::MinusAtAn_R(Ay), Op::MinusAtAn_RW(Ax));
            } else {
                const bool dir = bit::test<8>(instr);
                if (dir) {
                    if (kValidMemoryAlterableAddrModes[ea]) {
                        switch (sz) {
                        case 0b00: makeOpB(Or, Op::Dn_R(Dn), eaOp_RW(ea)); break;
                        case 0b01: makeOpW(Or, Op::Dn_R(Dn), eaOp_RW(ea)); break;
                        case 0b10: makeOpL(Or, Op::Dn_R(Dn), eaOp_RW(ea)); break;
                        }
                    }
                } else {
                    if (kValidDataAddrModes[ea]) {
                        switch (sz) {
                        case 0b00: makeOpB(Or, eaOp_R(ea), Op::Dn_RW(Dn)); break;
                        case 0b01: makeOpW(Or, eaOp_R(ea), Op::Dn_RW(Dn)); break;
                        case 0b10: makeOpL(Or, eaOp_R(ea), Op::Dn_RW(Dn)); break;
                        }
                    }
                }
            }
            break;
        }
        case 0x9: {
            const uint16 ea = bit::extract<0, 5>(instr);
            const uint16 sz = bit::extract<6, 7>(instr);
            if (bit::extract<6, 7>(instr) == 0b11) {
                if (kValidAddrModes[ea]) {
                    const uint16 An = bit::extract<9, 11>(instr);
                    const bool szBit = bit::test<8>(instr);
                    if (szBit) {
                        makeOpL(SubA, eaOp_R(ea), Op::An_RW(An));
                    } else {
                        makeOpW(SubA, eaOp_R(ea), Op::An_RW(An));
                    }
                }
            } else if (bit::extract<4, 5>(instr) == 0b00 && bit::extract<8>(instr) == 1) {
                const bool rm = bit::test<3>(instr);
                const uint16 Ry = bit::extract<0, 2>(instr);
                const uint16 Rx = bit::extract<9, 11>(instr);
                if (rm) {
                    switch (sz) {
                    case 0b00: makeOpB(SubX, Op::MinusAtAn_R(Ry), Op::MinusAtAn_RW(Rx)); break;
                    case 0b01: makeOpW(SubX, Op::MinusAtAn_R(Ry), Op::MinusAtAn_RW(Rx)); break;
                    case 0b10: makeOpL(SubX, Op::MinusAtAn_R(Ry), Op::MinusAtAn_RW(Rx)); break;
                    }
                } else {
                    switch (sz) {
                    case 0b00: makeOpB(SubX, Op::Dn_R(Ry), Op::Dn_RW(Rx)); break;
                    case 0b01: makeOpW(SubX, Op::Dn_R(Ry), Op::Dn_RW(Rx)); break;
                    case 0b10: makeOpL(SubX, Op::Dn_R(Ry), Op::Dn_RW(Rx)); break;
                    }
                }
            } else {
                const bool dir = bit::test<8>(instr);
                const uint16 Dn = bit::extract<9, 11>(instr);
                if (dir) {
                    if (kValidMemoryAlterableAddrModes[ea]) {
                        switch (sz) {
                        case 0b00: makeOpB(Sub, Op::Dn_R(Dn), eaOp_RW(ea)); break;
                        case 0b01: makeOpW(Sub, Op::Dn_R(Dn), eaOp_RW(ea)); break;
                        case 0b10: makeOpL(Sub, Op::Dn_R(Dn), eaOp_RW(ea)); break;
                        }
                    }
                } else {
                    if (kValidAddrModes[ea]) {
                        switch (sz) {
                        case 0b00: makeOpB(Sub, eaOp_R(ea), Op::Dn_RW(Dn)); break;
                        case 0b01: makeOpW(Sub, eaOp_R(ea), Op::Dn_RW(Dn)); break;
                        case 0b10: makeOpL(Sub, eaOp_R(ea), Op::Dn_RW(Dn)); break;
                        }
                    }
                }
            }
            break;
        }
        case 0xA: make0(Illegal1010); break;
        case 0xB: {
            const uint16 ea = bit::extract<0, 5>(instr);
            const uint16 sz = bit::extract<6, 7>(instr);
            if (sz == 0b11) {
                if (kValidAddrModes[ea]) {
                    const uint16 An = bit::extract<9, 11>(instr);
                    const bool szBit = bit::test<8>(instr);
                    if (szBit) {
                        makeOpL(CmpA, eaOp_R(ea), Op::An_R(An));
                    } else {
                        makeOpW(CmpA, eaOp_R(ea), Op::An_R(An));
                    }
                }
            } else if (bit::extract<8>(instr) == 0) {
                if (kValidAddrModes[ea]) {
                    const uint16 Dn = bit::extract<9, 11>(instr);
                    switch (sz) {
                    case 0b00: makeOpB(Cmp, eaOp_R(ea), Op::Dn_R(Dn)); break;
                    case 0b01: makeOpW(Cmp, eaOp_R(ea), Op::Dn_R(Dn)); break;
                    case 0b10: makeOpL(Cmp, eaOp_R(ea), Op::Dn_R(Dn)); break;
                    }
                }
            } else if (bit::extract<3, 5>(instr) == 0b001) {
                const uint16 Ay = bit::extract<0, 2>(instr);
                const uint16 Ax = bit::extract<9, 11>(instr);
                switch (sz) {
                case 0b00: makeOpB(CmpM, Op::AtAnPlus_R(Ay), Op::AtAnPlus_R(Ax)); break;
                case 0b01: makeOpW(CmpM, Op::AtAnPlus_R(Ay), Op::AtAnPlus_R(Ax)); break;
                case 0b10: makeOpL(CmpM, Op::AtAnPlus_R(Ay), Op::AtAnPlus_R(Ax)); break;
                }
            } else {
                if (kValidDataAlterableAddrModes[ea]) {
                    const uint16 Dn = bit::extract<9, 11>(instr);
                    switch (sz) {
                    case 0b00: makeOpB(Eor, Op::Dn_R(Dn), eaOp_RW(ea)); break;
                    case 0b01: makeOpW(Eor, Op::Dn_R(Dn), eaOp_RW(ea)); break;
                    case 0b10: makeOpL(Eor, Op::Dn_R(Dn), eaOp_RW(ea)); break;
                    }
                }
            }
            break;
        }
        case 0xC: {
            const uint16 ea = bit::extract<0, 5>(instr);
            const uint16 sz = bit::extract<6, 7>(instr);
            const uint32 Ry = bit::extract<0, 2>(instr);
            const uint32 Rx = bit::extract<9, 11>(instr);
            if (bit::extract<3, 8>(instr) == 0b100000) {
                makeOpBI(ABCD, Op::Dn_R(Ry), Op::Dn_RW(Rx));
            } else if (bit::extract<3, 8>(instr) == 0b100001) {
                makeOpBI(ABCD, Op::MinusAtAn_R(Ry), Op::MinusAtAn_RW(Rx));
            } else if (bit::extract<3, 8>(instr) == 0b101000) {
                makeOpLI(Exg, Op::Dn_RW(Ry), Op::Dn_RW(Rx));
            } else if (bit::extract<3, 8>(instr) == 0b101001) {
                makeOpLI(Exg, Op::An_RW(Ry), Op::An_RW(Rx));
            } else if (bit::extract<3, 8>(instr) == 0b110001) {
                makeOpLI(Exg, Op::Dn_RW(Ry), Op::An_RW(Rx));
            } else if (bit::extract<6, 8>(instr) == 0b011) {
                if (kValidDataAddrModes[ea]) {
                    makeOpWI(MulU, eaOp_R(ea), Op::Dn_RW(Rx));
                }
            } else if (bit::extract<6, 8>(instr) == 0b111) {
                if (kValidDataAddrModes[ea]) {
                    makeOpWI(MulS, eaOp_R(ea), Op::Dn_RW(Rx));
                }
            } else {
                const bool dir = bit::test<8>(instr);
                const uint16 Dn = bit::extract<9, 11>(instr);
                if (dir) {
                    if (kValidMemoryAlterableAddrModes[ea]) {
                        switch (sz) {
                        case 0b00: makeOpB(And, Op::Dn_R(Dn), eaOp_RW(ea)); break;
                        case 0b01: makeOpW(And, Op::Dn_R(Dn), eaOp_RW(ea)); break;
                        case 0b10: makeOpL(And, Op::Dn_R(Dn), eaOp_RW(ea)); break;
                        }
                    }
                } else {
                    if (kValidDataAddrModes[ea]) {
                        switch (sz) {
                        case 0b00: makeOpB(And, eaOp_R(ea), Op::Dn_RW(Dn)); break;
                        case 0b01: makeOpW(And, eaOp_R(ea), Op::Dn_RW(Dn)); break;
                        case 0b10: makeOpL(And, eaOp_R(ea), Op::Dn_RW(Dn)); break;
                        }
                    }
                }
            }
            break;
        }
        case 0xD: {
            const uint16 ea = bit::extract<0, 5>(instr);
            const uint16 sz = bit::extract<6, 7>(instr);
            if (sz == 0b11) {
                if (kValidAddrModes[ea]) {
                    const uint16 An = bit::extract<9, 11>(instr);
                    const bool szBit = bit::test<8>(instr);
                    if (szBit) {
                        makeOpL(AddA, eaOp_R(ea), Op::An_RW(An));
                    } else {
                        makeOpW(AddA, eaOp_R(ea), Op::An_RW(An));
                    }
                }
            } else if (bit::extract<4, 5>(instr) == 0b00 && bit::extract<8>(instr) == 1) {
                const bool rm = bit::test<3>(instr);
                const uint16 Ry = bit::extract<0, 2>(instr);
                const uint16 Rx = bit::extract<9, 11>(instr);
                if (rm) {
                    switch (sz) {
                    case 0b00: makeOpB(AddX, Op::MinusAtAn_R(Ry), Op::MinusAtAn_RW(Rx)); break;
                    case 0b01: makeOpW(AddX, Op::MinusAtAn_R(Ry), Op::MinusAtAn_RW(Rx)); break;
                    case 0b10: makeOpL(AddX, Op::MinusAtAn_R(Ry), Op::MinusAtAn_RW(Rx)); break;
                    }
                } else {
                    switch (sz) {
                    case 0b00: makeOpB(AddX, Op::Dn_R(Ry), Op::Dn_RW(Rx)); break;
                    case 0b01: makeOpW(AddX, Op::Dn_R(Ry), Op::Dn_RW(Rx)); break;
                    case 0b10: makeOpL(AddX, Op::Dn_R(Ry), Op::Dn_RW(Rx)); break;
                    }
                }
            } else {
                const bool dir = bit::test<8>(instr);
                const uint16 Dn = bit::extract<9, 11>(instr);
                if (dir) {
                    if (kValidMemoryAlterableAddrModes[ea]) {
                        switch (sz) {
                        case 0b00: makeOpB(Add, Op::Dn_R(Dn), eaOp_RW(ea)); break;
                        case 0b01: makeOpW(Add, Op::Dn_R(Dn), eaOp_RW(ea)); break;
                        case 0b10: makeOpL(Add, Op::Dn_R(Dn), eaOp_RW(ea)); break;
                        }
                    }
                } else {
                    if (kValidAddrModes[ea]) {
                        switch (sz) {
                        case 0b00: makeOpB(Add, eaOp_R(ea), Op::Dn_RW(Dn)); break;
                        case 0b01: makeOpW(Add, eaOp_R(ea), Op::Dn_RW(Dn)); break;
                        case 0b10: makeOpL(Add, eaOp_R(ea), Op::Dn_RW(Dn)); break;
                        }
                    }
                }
            }
            break;
        }
        case 0xE:
            if (bit::extract<6, 7>(instr) == 0b11 && bit::extract<11>(instr) == 0) {
                const uint16 ea = bit::extract<0, 5>(instr);
                const bool dir = bit::test<8>(instr);
                if (kValidMemoryAlterableAddrModes[ea]) {
                    switch (bit::extract<9, 10>(instr)) {
                    case 0b00: makeOpW(dir ? ASL : ASR, eaOp_RW(ea)); break;
                    case 0b01: makeOpW(dir ? LSL : LSR, eaOp_RW(ea)); break;
                    case 0b10: makeOpW(dir ? ROXL : ROXR, eaOp_RW(ea)); break;
                    case 0b11: makeOpW(dir ? ROL : ROR, eaOp_RW(ea)); break;
                    }
                }
            } else {
                const bool reg = bit::test<5>(instr);
                const uint16 sz = bit::extract<6, 7>(instr);
                const bool dir = bit::test<8>(instr);

                if (reg) {
                    const uint16 Dy = bit::extract<0, 2>(instr);
                    const uint16 Dx = bit::extract<9, 11>(instr);
                    switch (bit::extract<3, 4>(instr)) {
                    case 0b00:
                        switch (sz) {
                        case 0b00: makeOpB(dir ? ASL : ASR, Op::Dn_R(Dx), Op::Dn_RW(Dy)); break;
                        case 0b01: makeOpW(dir ? ASL : ASR, Op::Dn_R(Dx), Op::Dn_RW(Dy)); break;
                        case 0b10: makeOpL(dir ? ASL : ASR, Op::Dn_R(Dx), Op::Dn_RW(Dy)); break;
                        }
                        break;
                    case 0b01:
                        switch (sz) {
                        case 0b00: makeOpB(dir ? LSL : LSR, Op::Dn_R(Dx), Op::Dn_RW(Dy)); break;
                        case 0b01: makeOpW(dir ? LSL : LSR, Op::Dn_R(Dx), Op::Dn_RW(Dy)); break;
                        case 0b10: makeOpL(dir ? LSL : LSR, Op::Dn_R(Dx), Op::Dn_RW(Dy)); break;
                        }
                        break;
                    case 0b10:
                        switch (sz) {
                        case 0b00: makeOpB(dir ? ROXL : ROXR, Op::Dn_R(Dx), Op::Dn_RW(Dy)); break;
                        case 0b01: makeOpW(dir ? ROXL : ROXR, Op::Dn_R(Dx), Op::Dn_RW(Dy)); break;
                        case 0b10: makeOpL(dir ? ROXL : ROXR, Op::Dn_R(Dx), Op::Dn_RW(Dy)); break;
                        }
                        break;
                    case 0b11:
                        switch (sz) {
                        case 0b00: makeOpB(dir ? ROL : ROR, Op::Dn_R(Dx), Op::Dn_RW(Dy)); break;
                        case 0b01: makeOpW(dir ? ROL : ROR, Op::Dn_R(Dx), Op::Dn_RW(Dy)); break;
                        case 0b10: makeOpL(dir ? ROL : ROR, Op::Dn_R(Dx), Op::Dn_RW(Dy)); break;
                        }
                        break;
                    }
                } else {
                    const uint16 Dy = bit::extract<0, 2>(instr);
                    uint16 shift = bit::extract<9, 11>(instr);
                    if (shift == 0) {
                        shift = 8;
                    }
                    switch (bit::extract<3, 4>(instr)) {
                    case 0b00:
                        switch (sz) {
                        case 0b00: makeOpB(dir ? ASL : ASR, Op::UImmEmbedded(shift), Op::Dn_RW(Dy)); break;
                        case 0b01: makeOpW(dir ? ASL : ASR, Op::UImmEmbedded(shift), Op::Dn_RW(Dy)); break;
                        case 0b10: makeOpL(dir ? ASL : ASR, Op::UImmEmbedded(shift), Op::Dn_RW(Dy)); break;
                        }
                        break;
                    case 0b01:
                        switch (sz) {
                        case 0b00: makeOpB(dir ? LSL : LSR, Op::UImmEmbedded(shift), Op::Dn_RW(Dy)); break;
                        case 0b01: makeOpW(dir ? LSL : LSR, Op::UImmEmbedded(shift), Op::Dn_RW(Dy)); break;
                        case 0b10: makeOpL(dir ? LSL : LSR, Op::UImmEmbedded(shift), Op::Dn_RW(Dy)); break;
                        }
                        break;
                    case 0b10:
                        switch (sz) {
                        case 0b00: makeOpB(dir ? ROXL : ROXR, Op::UImmEmbedded(shift), Op::Dn_RW(Dy)); break;
                        case 0b01: makeOpW(dir ? ROXL : ROXR, Op::UImmEmbedded(shift), Op::Dn_RW(Dy)); break;
                        case 0b10: makeOpL(dir ? ROXL : ROXR, Op::UImmEmbedded(shift), Op::Dn_RW(Dy)); break;
                        }
                        break;
                    case 0b11:
                        switch (sz) {
                        case 0b00: makeOpB(dir ? ROL : ROR, Op::UImmEmbedded(shift), Op::Dn_RW(Dy)); break;
                        case 0b01: makeOpW(dir ? ROL : ROR, Op::UImmEmbedded(shift), Op::Dn_RW(Dy)); break;
                        case 0b10: makeOpL(dir ? ROL : ROR, Op::UImmEmbedded(shift), Op::Dn_RW(Dy)); break;
                        }
                        break;
                    }
                }
            }
            break;
        case 0xF: make0(Illegal1111); break;
        }
    }
}

FullDisasm Disassemble(std::function<uint16()> fetcher) {
    const uint16 opcode = fetcher();
    const OpcodeDisasm &opDisasm = DisasmTable::s_instance.disasm[opcode];

    FullDisasm disasm{
        .opcode = opDisasm,
    };

    auto translateOperand = [&](const Operand &op, OperandSize opSize, OperandDetails &det) {
        using Type = Operand::Type;
        switch (op.type) {
        case Type::AtDispAn: det.immDisp = static_cast<sint16>(fetcher()); break;
        case Type::AtDispAnIx: {
            const uint16 briefExtWord = fetcher();
            det.immDisp = static_cast<sint8>(bit::extract<0, 7>(briefExtWord));
            det.ixLong = bit::test<11>(briefExtWord);
            det.ix = bit::extract<12, 15>(briefExtWord);
            break;
        }
        case Type::AtDispPC: det.immDisp = static_cast<sint16>(fetcher()) - 4; break;
        case Type::AtDispPCIx: {
            const uint16 briefExtWord = fetcher();
            det.immDisp = static_cast<sint8>(bit::extract<0, 7>(briefExtWord)) - 2;
            det.ixLong = bit::test<11>(briefExtWord);
            det.ix = bit::extract<12, 15>(briefExtWord);
            break;
        }
        case Type::AtImmWord: det.immDisp = static_cast<sint16>(fetcher()); break;
        case Type::AtImmLong: {
            const uint32 addressHigh = fetcher();
            const uint32 addressLow = fetcher();
            det.immDisp = (addressHigh << 16u) | addressLow;
            break;
        }
        case Type::SImmFetched:
            switch (opSize) {
            case OperandSize::Byte: [[fallthrough]];
            case OperandSize::ByteImplicit: det.immDisp = static_cast<sint8>(fetcher()); break;
            case OperandSize::Word: [[fallthrough]];
            case OperandSize::WordImplicit: det.immDisp = static_cast<sint16>(fetcher()); break;
            case OperandSize::Long: [[fallthrough]];
            case OperandSize::LongImplicit:
                det.immDisp = fetcher();
                det.immDisp = (det.immDisp << 16u) | fetcher();
                break;
            default: break;
            }
        case Type::UImmFetched:
            switch (opSize) {
            case OperandSize::Byte: [[fallthrough]];
            case OperandSize::ByteImplicit: det.immDisp = fetcher(); break;
            case OperandSize::Word: [[fallthrough]];
            case OperandSize::WordImplicit: det.immDisp = fetcher(); break;
            case OperandSize::Long: [[fallthrough]];
            case OperandSize::LongImplicit:
                det.immDisp = fetcher();
                det.immDisp = (det.immDisp << 16u) | fetcher();
                break;
            default: break;
            }
        case Type::RegList: det.regList = fetcher(); break;
        default: break;
        }
    };

    translateOperand(opDisasm.op1, opDisasm.opSize, disasm.op1);
    translateOperand(opDisasm.op2, opDisasm.opSize, disasm.op2);

    return disasm;
}

} // namespace ymir::m68k
