#include <satemu/hw/m68k/m68k.hpp>

#include <satemu/util/bit_ops.hpp>

namespace satemu::m68k {

// All valid addressing modes
static constexpr auto kValidAddrModes = [] {
    std::array<bool, 0b111'111 + 1> arr{};
    arr.fill(false);
    for (int i = 0b000; i <= 0b111; i++) {
        arr[(0b000 << 3) | i] = true; // Dn
        arr[(0b001 << 3) | i] = true; // An
        arr[(0b010 << 3) | i] = true; // (An)
        arr[(0b011 << 3) | i] = true; // (An)+
        arr[(0b100 << 3) | i] = true; // -(An)
        arr[(0b101 << 3) | i] = true; // (disp, An)
        arr[(0b110 << 3) | i] = true; // (disp, An, Xn)
    }
    arr[0b111'010] = true; // (disp, PC)
    arr[0b111'011] = true; // (disp, PC, Xn)
    arr[0b111'000] = true; // (xxx).w
    arr[0b111'001] = true; // (xxx).l
    arr[0b111'100] = true; // #imm
    return arr;
}();

// Valid data addressing modes
static constexpr auto kValidDataAddrModes = [] {
    std::array<bool, 0b111'111 + 1> arr = kValidAddrModes;
    for (int i = 0b000; i <= 0b111; i++) {
        arr[(0b001 << 3) | i] = false; // An
    }
    return arr;
}();

// Valid memory addressing modes
static constexpr auto kValidMemoryAddrModes = [] {
    std::array<bool, 0b111'111 + 1> arr = kValidAddrModes;
    for (int i = 0b000; i <= 0b111; i++) {
        arr[(0b000 << 3) | i] = false; // Dn
        arr[(0b001 << 3) | i] = false; // An
    }
    return arr;
}();

// Valid control addressing modes
static constexpr auto kValidControlAddrModes = [] {
    std::array<bool, 0b111'111 + 1> arr = kValidAddrModes;
    for (int i = 0b000; i <= 0b111; i++) {
        arr[(0b000 << 3) | i] = false; // Dn
        arr[(0b001 << 3) | i] = false; // An
        arr[(0b011 << 3) | i] = false; // (An)+
        arr[(0b100 << 3) | i] = false; // -(An)
    }
    arr[0b111'100] = false; // #imm
    return arr;
}();

// Valid alterable addressing modes
static constexpr auto kValidAlterableAddrModes = [] {
    std::array<bool, 0b111'111 + 1> arr = kValidAddrModes;
    arr[0b111'010] = false; // (disp, PC)
    arr[0b111'011] = false; // (disp, PC, Xn)
    arr[0b111'000] = false; // (xxx).w
    arr[0b111'001] = false; // (xxx).l
    arr[0b111'100] = false; // #imm
    return arr;
}();

// Valid data alterable addressing modes
static constexpr auto kValidDataAlterableAddrModes = [] {
    std::array<bool, 0b111'111 + 1> arr = kValidDataAddrModes;
    for (int i = 0; i < arr.size(); i++) {
        arr[i] &= kValidAlterableAddrModes[i];
    }
    return arr;
}();

// Valid memory alterable addressing modes
static constexpr auto kValidMemoryAlterableAddrModes = [] {
    std::array<bool, 0b111'111 + 1> arr = kValidMemoryAddrModes;
    for (int i = 0; i < arr.size(); i++) {
        arr[i] &= kValidAlterableAddrModes[i];
    }
    return arr;
}();

// Valid control alterable addressing modes
static constexpr auto kValidControlAlterableAddrModes = [] {
    std::array<bool, 0b111'111 + 1> arr = kValidControlAddrModes;
    for (int i = 0; i < arr.size(); i++) {
        arr[i] &= kValidAlterableAddrModes[i];
    }
    return arr;
}();

DecodeTable BuildDecodeTable() {
    DecodeTable table{};
    table.opcodeTypes.fill(OpcodeType::Undecoded);

    for (uint32 instr = 0; instr < 0x10000; instr++) {
        auto &opcode = table.opcodeTypes[instr];

        auto legalIf = [](OpcodeType type, bool cond) { return cond ? type : OpcodeType::Illegal; };

        switch (instr >> 12u) {
        case 0x0: {
            const uint16 ea = bit::extract<0, 5>(instr);
            const uint16 sz = bit::extract<6, 7>(instr);
            if (instr == 0x023C) {
                // TODO: AndI_CCR
            } else if (instr == 0x027C) {
                // TODO: AndI_SR
            } else if (bit::extract<3, 5>(instr) == 0b001 && bit::extract<8>(instr) == 1) {
                // TODO: MOVEP
            } else if (bit::extract<6, 8>(instr) == 0b100) {
                if ((ea >> 3u) == 0b000) {
                    opcode = OpcodeType::BTst_R_Dn;
                } else {
                    opcode = legalIf(OpcodeType::BTst_R_EA, kValidDataAddrModes[ea]);
                }
            } else if (bit::extract<6, 11>(instr) == 0b100000) {
                if ((ea >> 3u) == 0b000) {
                    opcode = OpcodeType::BTst_I_Dn;
                } else {
                    opcode = OpcodeType::BTst_I_EA;
                }
            } else if (bit::extract<8, 11>(instr) == 0b0010) {
                opcode = legalIf(OpcodeType::AndI_EA, sz != 0b11 && kValidDataAlterableAddrModes[ea]);
            } else if (bit::extract<8, 11>(instr) == 0b0100) {
                opcode = legalIf(OpcodeType::SubI, sz != 0b11 && kValidDataAlterableAddrModes[ea]);
            } else if (bit::extract<8, 11>(instr) == 0b0110) {
                opcode = legalIf(OpcodeType::AddI, sz != 0b11 && kValidDataAlterableAddrModes[ea]);
            } else if (bit::extract<8, 11>(instr) == 0b1100) {
                opcode = legalIf(OpcodeType::CmpI, sz != 0b11 && kValidDataAddrModes[ea]);
            }
            break;
        }
        case 0x1:
        case 0x2:
        case 0x3:
            if (bit::extract<6, 8>(instr) == 0b001) {
                if ((instr >> 12u) != 0b01) {
                    opcode = OpcodeType::MoveA;
                }
            } else {
                const uint16 srcEA = bit::extract<0, 5>(instr);
                const uint16 dstEA = (bit::extract<6, 8>(instr) << 3) | bit::extract<9, 11>(instr);
                opcode = legalIf(OpcodeType::Move_EA_EA, kValidDataAlterableAddrModes[dstEA] && kValidAddrModes[srcEA]);
            }
            break;
        case 0x4: {
            const uint16 ea = bit::extract<0, 5>(instr);
            if (instr == 0x4E75) {
                opcode = OpcodeType::RTS;
            } else if (bit::extract<3, 11>(instr) == 0b100001000) {
                opcode = OpcodeType::Swap;
            } else if (bit::extract<6, 11>(instr) == 0b011011) {
                opcode = legalIf(OpcodeType::Move_EA_SR, kValidDataAddrModes[ea]);
            } else if (bit::extract<6, 11>(instr) == 0b111010) {
                opcode = legalIf(OpcodeType::JSR, kValidControlAddrModes[ea]);
            } else if (bit::extract<6, 11>(instr) == 0b111011) {
                opcode = legalIf(OpcodeType::Jmp, kValidControlAddrModes[ea]);
            } else if (bit::extract<7, 11>(instr) == 0b10001) {
                const bool isPredecrement = (ea >> 3u) == 0b100;
                if (isPredecrement) {
                    opcode = OpcodeType::MoveM_Rs_PD;
                } else {
                    opcode = legalIf(OpcodeType::MoveM_Rs_EA, kValidControlAlterableAddrModes[ea]);
                }
            } else if (bit::extract<7, 11>(instr) == 0b11001) {
                const bool isPostincrement = (ea >> 3u) == 0b011;
                if (isPostincrement) {
                    opcode = OpcodeType::MoveM_PI_Rs;
                } else {
                    opcode = legalIf(OpcodeType::MoveM_EA_Rs, kValidControlAddrModes[ea]);
                }
            } else if (bit::extract<8, 11>(instr) == 0b0010) {
                const uint16 sz = bit::extract<6, 7>(instr);
                opcode = legalIf(OpcodeType::Clr, sz != 0b11 && kValidDataAlterableAddrModes[ea]);
            } else if (bit::extract<6, 8>(instr) == 0b111) {
                opcode = legalIf(OpcodeType::LEA, kValidControlAddrModes[ea]);
            }
            break;
        }
        case 0x5: {
            const uint16 ea = bit::extract<0, 5>(instr);
            if (bit::extract<3, 7>(instr) == 0b11001) {
                opcode = OpcodeType::DBcc;
            } else if (bit::extract<6, 7>(instr) == 0b11) {
                // TODO: Scc
            } else {
                const uint16 M = bit::extract<3, 5>(instr);
                const uint16 sz = bit::extract<6, 7>(instr);
                const bool isAn = M == 0b001;
                const bool isByte = sz == 0b00;
                if (bit::extract<8>(instr) == 1) {
                    // TODO: SUBQ
                } else {
                    if (isAn) {
                        opcode = legalIf(OpcodeType::AddQ_An, !isByte);
                    } else {
                        opcode = legalIf(OpcodeType::AddQ_EA, kValidAlterableAddrModes[ea]);
                    }
                }
            }
            break;
        }
        case 0x6:
            switch (bit::extract<8, 11>(instr)) {
            case 0b0000: opcode = OpcodeType::BRA; break;
            case 0b0001: opcode = OpcodeType::BSR; break;
            default: opcode = OpcodeType::Bcc; break;
            }
            break;
        case 0x7: opcode = legalIf(OpcodeType::MoveQ, bit::extract<8>(instr) == 0); break;
        case 0x8: {
            const uint16 ea = bit::extract<0, 5>(instr);
            if (bit::extract<6, 7>(instr) == 0b11) {
                // TODO: DIVU, DIVS
            } else if (bit::extract<4, 8>(instr) == 0b10000) {
                // TODO: SBCD
            } else {
                const uint16 dir = bit::extract<8>(instr);
                opcode = legalIf(dir ? OpcodeType::Or_Dn_EA : OpcodeType::Or_EA_Dn, kValidDataAddrModes[ea]);
            }
            break;
        }
        case 0x9: break;
        case 0xA: break;
        case 0xB: {
            const uint16 ea = bit::extract<0, 5>(instr);
            if (bit::extract<6, 7>(instr) == 0b11) {
                opcode = legalIf(OpcodeType::CmpA, kValidAddrModes[ea]);
            } else if (bit::extract<8>(instr) == 0) {
                opcode = legalIf(OpcodeType::Cmp, kValidAddrModes[ea]);
            } else if (bit::extract<3, 5>(instr) == 0b001) {
                // TODO: CMPM
            } else {
                // TODO: EOR
            }
            break;
        }
        case 0xC: break;
        case 0xD: {
            const uint16 ea = bit::extract<0, 5>(instr);
            if (bit::extract<6, 7>(instr) == 0b11) {
                opcode = legalIf(OpcodeType::AddA, kValidAddrModes[bit::extract<0, 5>(instr)]);
            } else if (bit::extract<4, 5>(instr) == 0b00 && bit::extract<8>(instr) == 1) {
                // TODO: ADDX
            } else {
                const bool dir = bit::extract<8>(instr);
                if (dir) {
                    opcode = legalIf(OpcodeType::Add_Dn_EA, kValidMemoryAlterableAddrModes[ea]);
                } else {
                    opcode = legalIf(OpcodeType::Add_EA_Dn, kValidAddrModes[ea]);
                }
            }
            break;
        }
        case 0xE:
            if (bit::extract<6, 7>(instr) == 0b11) {
                const uint16 ea = bit::extract<0, 5>(instr);
                const bool dir = bit::extract<8>(instr);
                opcode = legalIf(dir ? OpcodeType::LSL_M : OpcodeType::LSR_M, kValidMemoryAlterableAddrModes[ea]);
            } else {
                const bool reg = bit::extract<5>(instr);
                const bool dir = bit::extract<8>(instr);
                switch (bit::extract<3, 4>(instr)) {
                case 0b00: /* TODO: ASL/ASR */ break;
                case 0b01:
                    opcode = reg ? (dir ? OpcodeType::LSL_R : OpcodeType::LSR_R)
                                 : (dir ? OpcodeType::LSL_I : OpcodeType::LSR_I);
                    break;
                case 0b10: /* TODO: ROXL/ROXR */ break;
                case 0b11: /* TODO: ROL/ROR */ break;
                }
            }
            break;
        case 0xF: break;
        }
    }

    return table;
}

DecodeTable g_decodeTable = BuildDecodeTable();

} // namespace satemu::m68k
