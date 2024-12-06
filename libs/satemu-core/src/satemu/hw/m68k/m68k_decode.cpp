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

DecodeTable BuildDecodeTable() {
    DecodeTable table{};
    table.opcodeTypes.fill(OpcodeType::Undecoded);

    for (uint32 instr = 0; instr < 0x10000; instr++) {
        auto &opcodeEntry = table.opcodeTypes[instr];

        switch (instr >> 12u) {
        case 0x0: break;
        case 0x1: break;
        case 0x2:
            if (bit::extract<12, 13>(instr) == 0b00) {
                // TODO: various bitwise operations
            } else if (bit::extract<6, 8>(instr) == 0b001) {
                // TODO: MOVEA
            } else {
                const uint16 srcEA = bit::extract<0, 5>(instr);
                const uint16 dstEA = (bit::extract<6, 8>(instr) << 3) | bit::extract<9, 11>(instr);
                if (kValidDataAddrModes[dstEA] && kValidAlterableAddrModes[dstEA] && kValidAddrModes[srcEA]) {
                    opcodeEntry = OpcodeType::Move_EA_EA;
                } else {
                    opcodeEntry = OpcodeType::Illegal;
                }
            }
            break;
        case 0x3: break;
        case 0x4:
            if (bit::extract<6, 11>(instr) == 0b0110'11) {
                if (kValidDataAddrModes[bit::extract<0, 5>(instr)]) {
                    opcodeEntry = OpcodeType::Move_EA_SR;
                } else {
                    opcodeEntry = OpcodeType::Illegal;
                }
            }
            break;
        case 0x5: break;
        case 0x6: break;
        case 0x7:
            if (bit::extract<8>(instr) == 0) {
                opcodeEntry = OpcodeType::MoveQ;
            } else {
                opcodeEntry = OpcodeType::Illegal;
            }
            break;
        case 0x8: break;
        case 0x9: break;
        case 0xA: break;
        case 0xB: break;
        case 0xC: break;
        case 0xD: break;
        case 0xE: break;
        case 0xF: break;
        }
    }

    return table;
}

DecodeTable g_decodeTable = BuildDecodeTable();

} // namespace satemu::m68k
