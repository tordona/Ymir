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
    table.opcodeTypes.fill(OpcodeType::Illegal);

    for (uint32 instr = 0; instr < 0x10000; instr++) {
        auto &opcode = table.opcodeTypes[instr];

        auto legalIf = [](OpcodeType type, bool cond) { return cond ? type : OpcodeType::Illegal; };

        switch (instr >> 12u) {
        case 0x0: {
            const uint16 ea = bit::extract<0, 5>(instr);
            const uint16 sz = bit::extract<6, 7>(instr);
            if (instr == 0x003C) {
                opcode = OpcodeType::OrI_CCR;
            } else if (instr == 0x007C) {
                opcode = OpcodeType::OrI_SR;
            } else if (instr == 0x023C) {
                opcode = OpcodeType::AndI_CCR;
            } else if (instr == 0x027C) {
                opcode = OpcodeType::AndI_SR;
            } else if (instr == 0x0A3C) {
                opcode = OpcodeType::EorI_CCR;
            } else if (instr == 0x0A7C) {
                opcode = OpcodeType::EorI_SR;
            } else if (bit::extract<3, 5>(instr) == 0b001 && bit::extract<8>(instr) == 1) {
                const bool sz = bit::extract<6>(instr);
                if (bit::extract<7>(instr)) {
                    opcode = sz ? OpcodeType::MoveP_Dx_Ay_L : OpcodeType::MoveP_Dx_Ay_W;
                } else {
                    opcode = sz ? OpcodeType::MoveP_Ay_Dx_L : OpcodeType::MoveP_Ay_Dx_W;
                }
            } else if (bit::extract<6, 8>(instr) == 0b100) {
                if ((ea >> 3u) == 0b000) {
                    opcode = OpcodeType::BTst_R_Dn;
                } else {
                    opcode = legalIf(OpcodeType::BTst_R_EA, kValidDataAddrModes[ea]);
                }
            } else if (bit::extract<6, 8>(instr) == 0b101) {
                if ((ea >> 3u) == 0b000) {
                    opcode = OpcodeType::BChg_R_Dn;
                } else {
                    opcode = legalIf(OpcodeType::BChg_R_EA, kValidDataAddrModes[ea]);
                }
            } else if (bit::extract<6, 8>(instr) == 0b110) {
                if ((ea >> 3u) == 0b000) {
                    opcode = OpcodeType::BClr_R_Dn;
                } else {
                    opcode = legalIf(OpcodeType::BClr_R_EA, kValidDataAddrModes[ea]);
                }
            } else if (bit::extract<6, 8>(instr) == 0b111) {
                if ((ea >> 3u) == 0b000) {
                    opcode = OpcodeType::BSet_R_Dn;
                } else {
                    opcode = legalIf(OpcodeType::BSet_R_EA, kValidDataAddrModes[ea]);
                }
            } else if (bit::extract<6, 11>(instr) == 0b100000) {
                if ((ea >> 3u) == 0b000) {
                    opcode = OpcodeType::BTst_I_Dn;
                } else {
                    opcode = OpcodeType::BTst_I_EA;
                }
            } else if (bit::extract<6, 11>(instr) == 0b100001) {
                if ((ea >> 3u) == 0b000) {
                    opcode = OpcodeType::BChg_I_Dn;
                } else {
                    opcode = OpcodeType::BChg_I_EA;
                }
            } else if (bit::extract<6, 11>(instr) == 0b100010) {
                if ((ea >> 3u) == 0b000) {
                    opcode = OpcodeType::BClr_I_Dn;
                } else {
                    opcode = OpcodeType::BClr_I_EA;
                }
            } else if (bit::extract<6, 11>(instr) == 0b100011) {
                if ((ea >> 3u) == 0b000) {
                    opcode = OpcodeType::BSet_I_Dn;
                } else {
                    opcode = OpcodeType::BSet_I_EA;
                }
            } else if (bit::extract<8, 11>(instr) == 0b0000) {
                opcode = legalIf(OpcodeType::OrI_EA, sz != 0b11 && kValidDataAlterableAddrModes[ea]);
            } else if (bit::extract<8, 11>(instr) == 0b0010) {
                opcode = legalIf(OpcodeType::AndI_EA, sz != 0b11 && kValidDataAlterableAddrModes[ea]);
            } else if (bit::extract<8, 11>(instr) == 0b1010) {
                opcode = legalIf(OpcodeType::EorI_EA, sz != 0b11 && kValidDataAlterableAddrModes[ea]);
            } else if (bit::extract<8, 11>(instr) == 0b0100) {
                opcode = legalIf(OpcodeType::SubI, sz != 0b11 && kValidDataAlterableAddrModes[ea]);
            } else if (bit::extract<8, 11>(instr) == 0b0110) {
                opcode = legalIf(OpcodeType::AddI, sz != 0b11 && kValidDataAlterableAddrModes[ea]);
            } else if (bit::extract<8, 11>(instr) == 0b1100) {
                opcode = legalIf(OpcodeType::CmpI, sz != 0b11 && kValidDataAddrModes[ea]);
            }
            break;
        }
        case 0x1: // fallthrough
        case 0x2: // fallthrough
        case 0x3:
            if (bit::extract<6, 8>(instr) == 0b001) {
                const uint16 size = bit::extract<12, 13>(instr);
                switch (size) {
                case 0b11: opcode = OpcodeType::MoveA_W; break;
                case 0b10: opcode = OpcodeType::MoveA_L; break;
                }
            } else {
                const uint16 size = bit::extract<12, 13>(instr);
                const uint16 srcEA = bit::extract<0, 5>(instr);
                const uint16 dstEA = (bit::extract<6, 8>(instr) << 3) | bit::extract<9, 11>(instr);

                // Note the swapped bit order between word and longword moves
                switch (size) {
                case 0b01: opcode = OpcodeType::Move_EA_EA_B; break;
                case 0b11: opcode = OpcodeType::Move_EA_EA_W; break;
                case 0b10: opcode = OpcodeType::Move_EA_EA_L; break;
                }
                opcode = legalIf(opcode, kValidDataAlterableAddrModes[dstEA] && kValidAddrModes[srcEA]);
            }
            break;
        case 0x4: {
            const uint16 ea = bit::extract<0, 5>(instr);
            if (instr == 0x4E70) {
                opcode = OpcodeType::Reset;
            } else if (instr == 0x4E71) {
                opcode = OpcodeType::Noop;
            } else if (instr == 0x4E72) {
                opcode = OpcodeType::Stop;
            } else if (instr == 0x4E73) {
                opcode = OpcodeType::RTE;
            } else if (instr == 0x4E75) {
                opcode = OpcodeType::RTS;
            } else if (instr == 0x4E76) {
                opcode = OpcodeType::TrapV;
            } else if (instr == 0x4E77) {
                opcode = OpcodeType::RTR;
            } else if (instr == 0x4AFC) {
                opcode = OpcodeType::Illegal;
            } else if (bit::extract<3, 11>(instr) == 0b100001000) {
                opcode = OpcodeType::Swap;
            } else if (bit::extract<3, 11>(instr) == 0b100010000) {
                opcode = OpcodeType::Ext_W;
            } else if (bit::extract<3, 11>(instr) == 0b100011000) {
                opcode = OpcodeType::Ext_L;
            } else if (bit::extract<3, 11>(instr) == 0b111001010) {
                opcode = OpcodeType::Link;
            } else if (bit::extract<3, 11>(instr) == 0b111001011) {
                opcode = OpcodeType::Unlink;
            } else if (bit::extract<3, 11>(instr) == 0b111001100) {
                opcode = OpcodeType::Move_An_USP;
            } else if (bit::extract<3, 11>(instr) == 0b111001101) {
                opcode = OpcodeType::Move_USP_An;
            } else if (bit::extract<4, 11>(instr) == 0b11100100) {
                opcode = OpcodeType::Trap;
            } else if (bit::extract<6, 11>(instr) == 0b000011) {
                opcode = legalIf(OpcodeType::Move_SR_EA, kValidDataAlterableAddrModes[ea]);
            } else if (bit::extract<6, 11>(instr) == 0b001011) {
                opcode = legalIf(OpcodeType::Move_CCR_EA, kValidDataAlterableAddrModes[ea]);
            } else if (bit::extract<6, 11>(instr) == 0b010011) {
                opcode = legalIf(OpcodeType::Move_EA_CCR, kValidDataAddrModes[ea]);
            } else if (bit::extract<6, 11>(instr) == 0b011011) {
                opcode = legalIf(OpcodeType::Move_EA_SR, kValidDataAddrModes[ea]);
            } else if (bit::extract<6, 11>(instr) == 0b100000) {
                opcode = legalIf(OpcodeType::NBCD, kValidDataAlterableAddrModes[ea]);
            } else if (bit::extract<6, 11>(instr) == 0b101011) {
                opcode = legalIf(OpcodeType::TAS, kValidDataAlterableAddrModes[ea]);
            } else if (bit::extract<6, 11>(instr) == 0b100001) {
                opcode = legalIf(OpcodeType::PEA, kValidControlAddrModes[ea]);
            } else if (bit::extract<6, 11>(instr) == 0b111010) {
                opcode = legalIf(OpcodeType::JSR, kValidControlAddrModes[ea]);
            } else if (bit::extract<6, 11>(instr) == 0b111011) {
                opcode = legalIf(OpcodeType::Jmp, kValidControlAddrModes[ea]);
            } else if (bit::extract<7, 11>(instr) == 0b10001) {
                const bool isPredecrement = (ea >> 3u) == 0b100;
                const bool sz = bit::extract<6>(instr);
                if (isPredecrement) {
                    opcode = sz ? OpcodeType::MoveM_Rs_PD_L : OpcodeType::MoveM_Rs_PD_W;
                } else {
                    opcode = legalIf(sz ? OpcodeType::MoveM_Rs_EA_L : OpcodeType::MoveM_Rs_EA_W,
                                     kValidControlAlterableAddrModes[ea]);
                }
            } else if (bit::extract<7, 11>(instr) == 0b11001) {
                const bool isPostincrement = (ea >> 3u) == 0b011;
                const bool sz = bit::extract<6>(instr);
                if (isPostincrement) {
                    opcode = sz ? OpcodeType::MoveM_PI_Rs_L : OpcodeType::MoveM_PI_Rs_W;
                } else {
                    const uint16 Xn = bit::extract<0, 2>(instr);
                    const uint16 M = bit::extract<3, 5>(instr);
                    const bool isProgramAccess = M == 7 && (Xn == 2 || Xn == 3);
                    if (isProgramAccess) {
                        opcode = legalIf(sz ? OpcodeType::MoveM_EA_Rs_C_L : OpcodeType::MoveM_EA_Rs_C_W,
                                         kValidControlAddrModes[ea]);
                    } else {
                        opcode = legalIf(sz ? OpcodeType::MoveM_EA_Rs_D_L : OpcodeType::MoveM_EA_Rs_D_W,
                                         kValidControlAddrModes[ea]);
                    }
                }
            } else if (bit::extract<8, 11>(instr) == 0b0000) {
                const uint16 sz = bit::extract<6, 7>(instr);
                opcode = legalIf(OpcodeType::NegX, sz != 0b11 && kValidDataAlterableAddrModes[ea]);
            } else if (bit::extract<8, 11>(instr) == 0b0010) {
                const uint16 sz = bit::extract<6, 7>(instr);
                switch (sz) {
                case 0b00: opcode = legalIf(OpcodeType::Clr_B, kValidDataAlterableAddrModes[ea]); break;
                case 0b01: opcode = legalIf(OpcodeType::Clr_W, kValidDataAlterableAddrModes[ea]); break;
                case 0b10: opcode = legalIf(OpcodeType::Clr_L, kValidDataAlterableAddrModes[ea]); break;
                }
            } else if (bit::extract<8, 11>(instr) == 0b0100) {
                const uint16 sz = bit::extract<6, 7>(instr);
                opcode = legalIf(OpcodeType::Neg, sz != 0b11 && kValidDataAlterableAddrModes[ea]);
            } else if (bit::extract<8, 11>(instr) == 0b0110) {
                const uint16 sz = bit::extract<6, 7>(instr);
                opcode = legalIf(OpcodeType::Not, sz != 0b11 && kValidDataAlterableAddrModes[ea]);
            } else if (bit::extract<8, 11>(instr) == 0b1010) {
                opcode = legalIf(OpcodeType::Tst, kValidDataAlterableAddrModes[ea]);
            } else if (bit::extract<6, 8>(instr) == 0b110) {
                opcode = legalIf(OpcodeType::Chk, kValidDataAddrModes[ea]);
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
                opcode = legalIf(OpcodeType::Scc, kValidDataAlterableAddrModes[ea]);
            } else {
                const uint16 M = bit::extract<3, 5>(instr);
                const uint16 sz = bit::extract<6, 7>(instr);
                const bool isAn = M == 0b001;
                const bool isByte = sz == 0b00;
                if (bit::extract<8>(instr) == 1) {
                    if (isAn) {
                        opcode = legalIf(OpcodeType::SubQ_An, !isByte);
                    } else {
                        opcode = legalIf(OpcodeType::SubQ_EA, kValidAlterableAddrModes[ea]);
                    }
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
            if (bit::extract<6, 8>(instr) == 0b011) {
                opcode = legalIf(OpcodeType::DivU, kValidDataAddrModes[ea]);
            } else if (bit::extract<6, 8>(instr) == 0b111) {
                opcode = legalIf(OpcodeType::DivS, kValidDataAddrModes[ea]);
            } else if (bit::extract<3, 8>(instr) == 0b100000) {
                opcode = OpcodeType::SBCD_R;
            } else if (bit::extract<3, 8>(instr) == 0b100001) {
                opcode = OpcodeType::SBCD_M;
            } else {
                const uint16 dir = bit::extract<8>(instr);
                opcode = legalIf(dir ? OpcodeType::Or_Dn_EA : OpcodeType::Or_EA_Dn, kValidDataAddrModes[ea]);
            }
            break;
        }
        case 0x9: {
            const uint16 ea = bit::extract<0, 5>(instr);
            if (bit::extract<6, 7>(instr) == 0b11) {
                opcode = legalIf(OpcodeType::SubA, kValidAddrModes[bit::extract<0, 5>(instr)]);
            } else if (bit::extract<4, 5>(instr) == 0b00 && bit::extract<8>(instr) == 1) {
                const bool rm = bit::extract<3>(instr);
                if (rm) {
                    opcode = OpcodeType::SubX_M;
                } else {
                    opcode = OpcodeType::SubX_R;
                }
            } else {
                const bool dir = bit::extract<8>(instr);
                if (dir) {
                    opcode = legalIf(OpcodeType::Sub_Dn_EA, kValidMemoryAlterableAddrModes[ea]);
                } else {
                    opcode = legalIf(OpcodeType::Sub_EA_Dn, kValidAddrModes[ea]);
                }
            }
            break;
        }
        case 0xA: opcode = OpcodeType::Illegal1010; break;
        case 0xB: {
            const uint16 ea = bit::extract<0, 5>(instr);
            if (bit::extract<6, 7>(instr) == 0b11) {
                opcode = legalIf(OpcodeType::CmpA, kValidAddrModes[ea]);
            } else if (bit::extract<8>(instr) == 0) {
                opcode = legalIf(OpcodeType::Cmp, kValidAddrModes[ea]);
            } else if (bit::extract<3, 5>(instr) == 0b001) {
                opcode = OpcodeType::CmpM;
            } else {
                opcode = legalIf(OpcodeType::Eor_Dn_EA, kValidDataAlterableAddrModes[ea]);
            }
            break;
        }
        case 0xC: {
            const uint16 ea = bit::extract<0, 5>(instr);
            if (bit::extract<3, 8>(instr) == 0b100000) {
                opcode = OpcodeType::ABCD_R;
            } else if (bit::extract<3, 8>(instr) == 0b100001) {
                opcode = OpcodeType::ABCD_M;
            } else if (bit::extract<3, 8>(instr) == 0b101000) {
                opcode = OpcodeType::Exg_Dn_Dn;
            } else if (bit::extract<3, 8>(instr) == 0b101001) {
                opcode = OpcodeType::Exg_An_An;
            } else if (bit::extract<3, 8>(instr) == 0b110001) {
                opcode = OpcodeType::Exg_Dn_An;
            } else if (bit::extract<6, 8>(instr) == 0b011) {
                opcode = legalIf(OpcodeType::MulU, kValidDataAddrModes[ea]);
            } else if (bit::extract<6, 8>(instr) == 0b111) {
                opcode = legalIf(OpcodeType::MulS, kValidDataAddrModes[ea]);
            } else {
                const uint16 dir = bit::extract<8>(instr);
                opcode = legalIf(dir ? OpcodeType::And_Dn_EA : OpcodeType::And_EA_Dn, kValidDataAddrModes[ea]);
            }
            break;
        }
        case 0xD: {
            const uint16 ea = bit::extract<0, 5>(instr);
            const uint16 sz = bit::extract<6, 7>(instr);
            if (sz == 0b11) {
                opcode = legalIf(OpcodeType::AddA, kValidAddrModes[bit::extract<0, 5>(instr)]);
            } else if (bit::extract<4, 5>(instr) == 0b00 && bit::extract<8>(instr) == 1) {
                const bool rm = bit::extract<3>(instr);
                if (rm) {
                    opcode = OpcodeType::AddX_M;
                } else {
                    opcode = OpcodeType::AddX_R;
                }
            } else {
                const bool dir = bit::extract<8>(instr);
                if (dir) {
                    switch (sz) {
                    case 0b00: opcode = legalIf(OpcodeType::Add_Dn_EA_B, kValidMemoryAlterableAddrModes[ea]);
                    case 0b01: opcode = legalIf(OpcodeType::Add_Dn_EA_W, kValidMemoryAlterableAddrModes[ea]);
                    case 0b10: opcode = legalIf(OpcodeType::Add_Dn_EA_L, kValidMemoryAlterableAddrModes[ea]);
                    }
                } else {
                    opcode = legalIf(OpcodeType::Add_EA_Dn, kValidAddrModes[ea]);
                }
            }
            break;
        }
        case 0xE:
            if (bit::extract<6, 7>(instr) == 0b11 && bit::extract<11>(instr) == 0) {
                const uint16 ea = bit::extract<0, 5>(instr);
                const bool dir = bit::extract<8>(instr);
                switch (bit::extract<9, 10>(instr)) {
                case 0b00: opcode = dir ? OpcodeType::ASL_M : OpcodeType::ASR_M; break;
                case 0b01: opcode = dir ? OpcodeType::LSL_M : OpcodeType::LSR_M; break;
                case 0b10: opcode = dir ? OpcodeType::ROXL_M : OpcodeType::ROXR_M; break;
                case 0b11: opcode = dir ? OpcodeType::ROL_M : OpcodeType::ROR_M; break;
                }
                opcode = legalIf(opcode, kValidMemoryAlterableAddrModes[ea]);
            } else {
                const bool reg = bit::extract<5>(instr);
                const bool dir = bit::extract<8>(instr);
                switch (bit::extract<3, 4>(instr)) {
                case 0b00:
                    opcode = reg ? (dir ? OpcodeType::ASL_R : OpcodeType::ASR_R)
                                 : (dir ? OpcodeType::ASL_I : OpcodeType::ASR_I);
                    break;
                case 0b01:
                    opcode = reg ? (dir ? OpcodeType::LSL_R : OpcodeType::LSR_R)
                                 : (dir ? OpcodeType::LSL_I : OpcodeType::LSR_I);
                    break;
                case 0b10:
                    opcode = reg ? (dir ? OpcodeType::ROXL_R : OpcodeType::ROXR_R)
                                 : (dir ? OpcodeType::ROXL_I : OpcodeType::ROXR_I);
                    break;
                case 0b11:
                    opcode = reg ? (dir ? OpcodeType::ROL_R : OpcodeType::ROR_R)
                                 : (dir ? OpcodeType::ROL_I : OpcodeType::ROR_I);
                    break;
                }
            }
            break;
        case 0xF: opcode = OpcodeType::Illegal1111; break;
        }
    }

    return table;
}

DecodeTable g_decodeTable = BuildDecodeTable();

} // namespace satemu::m68k
