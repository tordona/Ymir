#include <satemu/hw/sh2/sh2_decode.hpp>

namespace satemu::sh2 {

DecodeTable BuildDecodeTable() {
    DecodeTable table{};

    table.opcodes[0].fill(OpcodeType::Illegal);
    table.opcodes[1].fill(OpcodeType::Illegal);

    for (uint32 instr = 0; instr < 0x10000; instr++) {
        auto &regularOpcode = table.opcodes[0][instr];
        auto &delayOpcode = table.opcodes[1][instr];

        auto setOpcode = [&](OpcodeType type) { regularOpcode = delayOpcode = type; };
        auto setNonDelayOpcode = [&](OpcodeType type) {
            regularOpcode = type;
            delayOpcode = OpcodeType::IllegalSlot;
        };

        switch (instr >> 12u) {
        case 0x0:
            switch (instr) {
            case 0x0008: setOpcode(OpcodeType::CLRT); break;
            case 0x0009: setOpcode(OpcodeType::NOP); break;
            case 0x000B: setNonDelayOpcode(OpcodeType::RTS); break;
            case 0x0018: setOpcode(OpcodeType::SETT); break;
            case 0x0019: setOpcode(OpcodeType::DIV0U); break;
            case 0x001B: setOpcode(OpcodeType::SLEEP); break;
            case 0x0028: setOpcode(OpcodeType::CLRMAC); break;
            case 0x002B: setNonDelayOpcode(OpcodeType::RTE); break;
            default:
                switch (instr & 0xFF) {
                case 0x02: setOpcode(OpcodeType::STC_SR_R); break;
                case 0x03: setNonDelayOpcode(OpcodeType::BSRF); break;
                case 0x0A: setOpcode(OpcodeType::STS_MACH_R); break;
                case 0x12: setOpcode(OpcodeType::STC_GBR_R); break;
                case 0x1A: setOpcode(OpcodeType::STS_MACL_R); break;
                case 0x22: setOpcode(OpcodeType::STC_VBR_R); break;
                case 0x23: setNonDelayOpcode(OpcodeType::BRAF); break;
                case 0x29: setOpcode(OpcodeType::MOVT); break;
                case 0x2A: setOpcode(OpcodeType::STS_PR_R); break;
                default:
                    switch (instr & 0xF) {
                    case 0x4: setOpcode(OpcodeType::MOVB_S0); break;
                    case 0x5: setOpcode(OpcodeType::MOVW_S0); break;
                    case 0x6: setOpcode(OpcodeType::MOVL_S0); break;
                    case 0x7: setOpcode(OpcodeType::MUL); break;
                    case 0xC: setOpcode(OpcodeType::MOVB_L0); break;
                    case 0xD: setOpcode(OpcodeType::MOVW_L0); break;
                    case 0xE: setOpcode(OpcodeType::MOVL_L0); break;
                    case 0xF: setOpcode(OpcodeType::MACL); break;
                    }
                    break;
                }
                break;
            }
            break;
        case 0x1: setOpcode(OpcodeType::MOVL_S4); break;
        case 0x2: {
            switch (instr & 0xF) {
            case 0x0: setOpcode(OpcodeType::MOVB_S); break;
            case 0x1: setOpcode(OpcodeType::MOVW_S); break;
            case 0x2: setOpcode(OpcodeType::MOVL_S); break;

            case 0x4: setOpcode(OpcodeType::MOVB_M); break;
            case 0x5: setOpcode(OpcodeType::MOVW_M); break;
            case 0x6: setOpcode(OpcodeType::MOVL_M); break;
            case 0x7: setOpcode(OpcodeType::DIV0S); break;
            case 0x8: setOpcode(OpcodeType::TST_R); break;
            case 0x9: setOpcode(OpcodeType::AND_R); break;
            case 0xA: setOpcode(OpcodeType::XOR_R); break;
            case 0xB: setOpcode(OpcodeType::OR_R); break;
            case 0xC: setOpcode(OpcodeType::CMP_STR); break;
            case 0xD: setOpcode(OpcodeType::XTRCT); break;
            case 0xE: setOpcode(OpcodeType::MULU); break;
            case 0xF: setOpcode(OpcodeType::MULS); break;
            }
            break;
        }
        case 0x3:
            switch (instr & 0xF) {
            case 0x0: setOpcode(OpcodeType::CMP_EQ_R); break;
            case 0x2: setOpcode(OpcodeType::CMP_HS); break;
            case 0x3: setOpcode(OpcodeType::CMP_GE); break;
            case 0x4: setOpcode(OpcodeType::DIV1); break;
            case 0x5: setOpcode(OpcodeType::DMULU); break;
            case 0x6: setOpcode(OpcodeType::CMP_HI); break;
            case 0x7: setOpcode(OpcodeType::CMP_GT); break;
            case 0x8: setOpcode(OpcodeType::SUB); break;

            case 0xA: setOpcode(OpcodeType::SUBC); break;
            case 0xB: setOpcode(OpcodeType::SUBV); break;

            case 0xC: setOpcode(OpcodeType::ADD); break;
            case 0xD: setOpcode(OpcodeType::DMULS); break;
            case 0xE: setOpcode(OpcodeType::ADDC); break;
            case 0xF: setOpcode(OpcodeType::ADDV); break;
            }
            break;
        case 0x4:
            if ((instr & 0xF) == 0xF) {
                setOpcode(OpcodeType::MACW);
            } else {
                switch (instr & 0xFF) {
                case 0x00: setOpcode(OpcodeType::SHLL); break;
                case 0x01: setOpcode(OpcodeType::SHLR); break;
                case 0x02: setOpcode(OpcodeType::STS_MACH_M); break;
                case 0x03: setOpcode(OpcodeType::STC_SR_M); break;
                case 0x04: setOpcode(OpcodeType::ROTL); break;
                case 0x05: setOpcode(OpcodeType::ROTR); break;
                case 0x06: setOpcode(OpcodeType::LDS_MACH_M); break;
                case 0x07: setOpcode(OpcodeType::LDC_SR_M); break;
                case 0x08: setOpcode(OpcodeType::SHLL2); break;
                case 0x09: setOpcode(OpcodeType::SHLR2); break;
                case 0x0A: setOpcode(OpcodeType::LDS_MACH_R); break;
                case 0x0B: setNonDelayOpcode(OpcodeType::JSR); break;

                case 0x0E: setOpcode(OpcodeType::LDC_SR_R); break;

                case 0x10: setOpcode(OpcodeType::DT); break;
                case 0x11: setOpcode(OpcodeType::CMP_PZ); break;
                case 0x12: setOpcode(OpcodeType::STS_MACL_M); break;
                case 0x13: setOpcode(OpcodeType::STC_GBR_M); break;

                case 0x15: setOpcode(OpcodeType::CMP_PL); break;
                case 0x16: setOpcode(OpcodeType::LDS_MACL_M); break;
                case 0x17: setOpcode(OpcodeType::LDC_GBR_M); break;
                case 0x18: setOpcode(OpcodeType::SHLL8); break;
                case 0x19: setOpcode(OpcodeType::SHLR8); break;
                case 0x1A: setOpcode(OpcodeType::LDS_MACL_R); break;
                case 0x1B: setOpcode(OpcodeType::TAS); break;

                case 0x1E: setOpcode(OpcodeType::LDC_GBR_R); break;

                case 0x20: setOpcode(OpcodeType::SHAL); break;
                case 0x21: setOpcode(OpcodeType::SHAR); break;
                case 0x22: setOpcode(OpcodeType::STS_PR_M); break;
                case 0x23: setOpcode(OpcodeType::STC_VBR_M); break;
                case 0x24: setOpcode(OpcodeType::ROTCL); break;
                case 0x25: setOpcode(OpcodeType::ROTCR); break;
                case 0x26: setOpcode(OpcodeType::LDS_PR_M); break;
                case 0x27: setOpcode(OpcodeType::LDC_VBR_M); break;
                case 0x28: setOpcode(OpcodeType::SHLL16); break;
                case 0x29: setOpcode(OpcodeType::SHLR16); break;
                case 0x2A: setOpcode(OpcodeType::LDS_PR_R); break;
                case 0x2B: setNonDelayOpcode(OpcodeType::JMP); break;

                case 0x2E: setOpcode(OpcodeType::LDC_VBR_R); break;
                }
            }
            break;
        case 0x5: setOpcode(OpcodeType::MOVL_L4); break;
        case 0x6:
            switch (instr & 0xF) {
            case 0x0: setOpcode(OpcodeType::MOVB_L); break;
            case 0x1: setOpcode(OpcodeType::MOVW_L); break;
            case 0x2: setOpcode(OpcodeType::MOVL_L); break;
            case 0x3: setOpcode(OpcodeType::MOV_R); break;
            case 0x4: setOpcode(OpcodeType::MOVB_P); break;
            case 0x5: setOpcode(OpcodeType::MOVW_P); break;
            case 0x6: setOpcode(OpcodeType::MOVL_P); break;
            case 0x7: setOpcode(OpcodeType::NOT); break;
            case 0x8: setOpcode(OpcodeType::SWAPB); break;
            case 0x9: setOpcode(OpcodeType::SWAPW); break;
            case 0xA: setOpcode(OpcodeType::NEGC); break;
            case 0xB: setOpcode(OpcodeType::NEG); break;
            case 0xC: setOpcode(OpcodeType::EXTUB); break;
            case 0xD: setOpcode(OpcodeType::EXTUW); break;
            case 0xE: setOpcode(OpcodeType::EXTSB); break;
            case 0xF: setOpcode(OpcodeType::EXTSW); break;
            }
            break;
        case 0x7: setOpcode(OpcodeType::ADD_I); break;
        case 0x8:
            switch ((instr >> 8u) & 0xF) {
            case 0x0: setOpcode(OpcodeType::MOVB_S4); break;
            case 0x1: setOpcode(OpcodeType::MOVW_S4); break;

            case 0x4: setOpcode(OpcodeType::MOVB_L4); break;
            case 0x5: setOpcode(OpcodeType::MOVW_L4); break;

            case 0x8: setOpcode(OpcodeType::CMP_EQ_I); break;
            case 0x9: setNonDelayOpcode(OpcodeType::BT); break;

            case 0xB: setNonDelayOpcode(OpcodeType::BF); break;

            case 0xD: setNonDelayOpcode(OpcodeType::BTS); break;

            case 0xF: setNonDelayOpcode(OpcodeType::BFS); break;
            }
            break;
        case 0x9: setOpcode(OpcodeType::MOVW_I); break;
        case 0xA: setNonDelayOpcode(OpcodeType::BRA); break;
        case 0xB: setNonDelayOpcode(OpcodeType::BSR); break;
        case 0xC:
            switch ((instr >> 8u) & 0xF) {
            case 0x0: setOpcode(OpcodeType::MOVB_SG); break;
            case 0x1: setOpcode(OpcodeType::MOVW_SG); break;
            case 0x2: setOpcode(OpcodeType::MOVL_SG); break;
            case 0x3: setNonDelayOpcode(OpcodeType::TRAPA); break;
            case 0x4: setOpcode(OpcodeType::MOVB_LG); break;
            case 0x5: setOpcode(OpcodeType::MOVW_LG); break;
            case 0x6: setOpcode(OpcodeType::MOVL_LG); break;
            case 0x7: setOpcode(OpcodeType::MOVA); break;
            case 0x8: setOpcode(OpcodeType::TST_I); break;
            case 0x9: setOpcode(OpcodeType::AND_I); break;
            case 0xA: setOpcode(OpcodeType::XOR_I); break;
            case 0xB: setOpcode(OpcodeType::OR_I); break;
            case 0xC: setOpcode(OpcodeType::TST_M); break;
            case 0xD: setOpcode(OpcodeType::AND_M); break;
            case 0xE: setOpcode(OpcodeType::XOR_M); break;
            case 0xF: setOpcode(OpcodeType::OR_M); break;
            }
            break;
        case 0xD: setOpcode(OpcodeType::MOVL_I); break;
        case 0xE: setOpcode(OpcodeType::MOV_I); break;
        }
    }

    return table;
}

DecodeTable g_decodeTable = BuildDecodeTable();

} // namespace satemu::sh2
