#include <ymir/core/types.hpp>

#include <ymir/hw/m68k/m68k_disasm.hpp>
#include <ymir/hw/scsp/scsp_dsp_instr.hpp>
#include <ymir/hw/scu/scu_dsp_disasm.hpp>
#include <ymir/hw/sh2/sh2_disasm.hpp>

#include <ymir/util/bit_ops.hpp>

#include "ansi.hpp"

#include <cxxopts.hpp>
#include <fmt/format.h>

#include <algorithm>
#include <concepts>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

using namespace ymir;

// TODO: reuse code where possible

struct ColorEscapes {
    const char *address;
    const char *bytes;

    const char *delaySlot;       // SH2 delay slot prefix
    const char *mnemonic;        // All except NOP
    const char *nopMnemonic;     // NOP
    const char *illegalMnemonic; // (illegal)
    const char *sizeSuffix;      // SH2/M68K size suffixes: b w l
    const char *cond;            // Conditions: eq, ne, z, nz, T0, ...

    const char *immediate;   // #0x1234
    const char *opRead;      // Read operands
    const char *opWrite;     // Written operands
    const char *opReadWrite; // Read and written operands

    const char *oper; // Operand (,); size suffix (.); operators (+, -, =, etc.)

    const char *addrInc; // SH2 and M68K address increment (@Rn+, (An)+)
    const char *addrDec; // SH2 and M68K address increment (@-Rn, -(An))

    const char *comment; // Comments

    const char *reset; // Color reset sequence
};

static ColorEscapes kNoColors = {
    .address = "",
    .bytes = "",

    .delaySlot = "",
    .mnemonic = "",
    .nopMnemonic = "",
    .illegalMnemonic = "",
    .sizeSuffix = "",
    .cond = "",

    .immediate = "",
    .opRead = "",
    .opWrite = "",
    .opReadWrite = "",

    .oper = "",

    .addrInc = "",
    .addrDec = "",

    .comment = "",

    .reset = "",
};

static ColorEscapes kBasicColors = {
    .address = ANSI_FGCOLOR_WHITE,
    .bytes = ANSI_FGCOLOR_WHITE,

    .delaySlot = ANSI_FGCOLOR_BLUE,
    .mnemonic = ANSI_FGCOLOR_BRIGHT_CYAN,
    .nopMnemonic = ANSI_FGCOLOR_CYAN,
    .illegalMnemonic = ANSI_FGCOLOR_BRIGHT_RED,
    .sizeSuffix = ANSI_FGCOLOR_BLUE,
    .cond = ANSI_FGCOLOR_MAGENTA,

    .immediate = ANSI_FGCOLOR_BRIGHT_YELLOW,
    .opRead = ANSI_FGCOLOR_BRIGHT_GREEN,
    .opWrite = ANSI_FGCOLOR_BRIGHT_MAGENTA,
    .opReadWrite = ANSI_FGCOLOR_BRIGHT_YELLOW,

    .oper = ANSI_FGCOLOR_BRIGHT_BLACK,

    .addrInc = ANSI_FGCOLOR_GREEN,
    .addrDec = ANSI_FGCOLOR_RED,

    .comment = ANSI_FGCOLOR_BRIGHT_BLACK,

    .reset = ANSI_RESET,
};

static ColorEscapes kTrueColors = {
    .address = ANSI_FGCOLOR_24B(217, 216, 237),
    .bytes = ANSI_FGCOLOR_24B(237, 236, 216),

    .delaySlot = ANSI_FGCOLOR_24B(96, 112, 156),
    .mnemonic = ANSI_FGCOLOR_24B(173, 216, 247),
    .nopMnemonic = ANSI_FGCOLOR_24B(66, 81, 92),
    .illegalMnemonic = ANSI_FGCOLOR_24B(247, 191, 173),
    .sizeSuffix = ANSI_FGCOLOR_24B(143, 159, 207),
    .cond = ANSI_FGCOLOR_24B(155, 146, 212),

    .immediate = ANSI_FGCOLOR_24B(221, 247, 173),
    .opRead = ANSI_FGCOLOR_24B(173, 247, 206),
    .opWrite = ANSI_FGCOLOR_24B(215, 173, 247),
    .opReadWrite = ANSI_FGCOLOR_24B(247, 206, 173),

    .oper = ANSI_FGCOLOR_24B(186, 191, 194),

    .addrInc = ANSI_FGCOLOR_24B(147, 194, 155),
    .addrDec = ANSI_FGCOLOR_24B(194, 159, 147),

    .comment = ANSI_FGCOLOR_24B(151, 154, 156),

    .reset = ANSI_RESET,
};

template <std::unsigned_integral T>
std::optional<T> ParseHex(std::string_view opcode) {
    static constexpr size_t kHexOpcodeLength = sizeof(T) * 2;

    if (opcode.size() > kHexOpcodeLength) {
        fmt::println("Value \"{}\" exceeds maximum length of {} hex digits", opcode, kHexOpcodeLength);
        return std::nullopt;
    }

    T value = 0;
    for (char c : opcode) {
        if (c >= '0' && c <= '9') {
            value = (value << 4ull) + c - '0';
        } else if (c >= 'A' && c <= 'F') {
            value = (value << 4ull) + c - 'A' + 10;
        } else if (c >= 'a' && c <= 'f') {
            value = (value << 4ull) + c - 'a' + 10;
        } else {
            fmt::println("Value \"{}\" is not a valid hexadecimal number", opcode);
            return std::nullopt;
        }
    }
    return value;
}

int main(int argc, char *argv[]) {
    bool showHelp = false;
    bool hideAddresses = false;
    bool hideOpcodes = false;
    std::string colorMode = "none";
    std::string inputFile{};
    std::string origin{};

    std::string isa{};
    std::vector<std::string> args{};

    cxxopts::Options options("ymdasm", "Ymir disassembly tool\nVersion " Ymir_VERSION);
    options.add_options()("h,help", "Display this help text.", cxxopts::value(showHelp)->default_value("false"));
    options.add_options()("C,color", "Color text output (stdout only): none, basic, truecolor",
                          cxxopts::value(colorMode)->default_value("none"), "color_mode");
    options.add_options()("i,input-file",
                          "Disassemble code from the specified file. Omit to disassemble command line arguments.",
                          cxxopts::value(inputFile), "path");
    options.add_options()("o,origin", "Origin (base) address of the disassembled code.",
                          cxxopts::value(origin)->default_value("0"), "address");
    options.add_options()("a,hide-addresses", "Hide addresses from disassembly listing.",
                          cxxopts::value(hideAddresses)->default_value("false"));
    options.add_options()("c,hide-opcodes", "Hide opcodes from disassembly listing.",
                          cxxopts::value(hideOpcodes)->default_value("false"));

    options.add_options()("isa", "Instruction set architecture: sh2, sh-2, m68k, m68000, scudsp, scspdsp",
                          cxxopts::value(isa));
    options.add_options()("opcodes", "Sequence of program opcodes", cxxopts::value(args));

    options.parse_positional({"isa", "opcodes"});
    options.positional_help("<isa> {<program opcodes>|[<offset> [<length>]]}");

    auto printHelp = [&] {
        fmt::println("{}", options.help());
        fmt::println("  <isa> specifies an instruction set architecture to disassemble:");
        fmt::println("    sh2, sh-2     Hitachi/Renesas SuperH-2");
        fmt::println("    m68k, m68000  Motorola 68000");
        fmt::println("    scudsp        SCU (Saturn Control Unit) DSP");
        fmt::println("    scspdsp       SCSP (Saturn Custom Sound Processor) DSP");
        fmt::println("    scspdspraw    SCSP (Saturn Custom Sound Processor) DSP (raw dissasembly)");
        fmt::println("  This argument is case-insensitive.");
        fmt::println("");
        fmt::println("  When disassembling command line arguments, <program opcodes> specifies the");
        fmt::println("  hexadecimal opcodes to disassemble.");
        fmt::println("");
        fmt::println("  When disassembling from a file, <offset> specifies the offset from the start");
        fmt::println("  of the file and <length> determines the number of bytes to disassemble.");
        fmt::println("  <length> is rounded down to the nearest multiple of the opcode size.");
        fmt::println("  If <offset> is omitted, ymdasm disassembles from the start of the file.");
        fmt::println("  If <length> is omitted, ymdasm disassembles until the end of the file.");
        fmt::println("");
        fmt::println("  SuperH-2 opcodes can be prefixed with ! to force them to be decoded as delay");
        fmt::println("  slot instructions or _ to force instructions in delay slots to be decoded as");
        fmt::println("  regular instructions.");
    };

    try {
        auto result = options.parse(argc, argv);

        // Show help if requested
        if (showHelp) {
            printHelp();
            return 0;
        }

        // ISA is required
        if (!result.contains("isa")) {
            fmt::println("Missing argument: <isa>");
            fmt::println("");
            printHelp();
            return 1;
        }

        // Must specify at least one opcode when disassembling from command line
        if (inputFile.empty() && args.empty()) {
            fmt::println("Missing argument: <program opcodes>");
            fmt::println("");
            printHelp();
            return 1;
        }

        // Color mode must be one of the valid modes
        ColorEscapes colors = kNoColors;
        if (result.contains("color")) {
            std::string lcColorMode = colorMode;
            std::transform(lcColorMode.cbegin(), lcColorMode.cend(), lcColorMode.begin(),
                           [](char c) { return std::tolower(c); });
            if (lcColorMode == "none") {
                colors = kNoColors;
            } else if (lcColorMode == "basic") {
                colors = kBasicColors;
            } else if (lcColorMode == "truecolor") {
                colors = kTrueColors;
            } else {
                fmt::println("Invalid color mode: {}", colorMode);
                fmt::println("");
                printHelp();
                return 1;
            }
        }

        int x = 0;

        auto printRaw = [&](std::string_view text, bool incPos = true) {
            fmt::print("{}", text);
            if (incPos) {
                x += text.size();
            }
        };

        auto print = [&](const char *color, std::string_view text, bool incPos = true) {
            fmt::print("{}{}", color, text);
            if (incPos) {
                x += text.size();
            }
        };

        auto align = [&](int pos) {
            if (pos > x) {
                std::string pad(pos - x, ' ');
                printRaw(pad);
            }
        };

        auto printMnemonic = [&](std::string_view mnemonic) { print(colors.mnemonic, mnemonic); };
        auto printNOP = [&](const char *mnemonic) { print(colors.nopMnemonic, mnemonic); };
        auto printIllegalMnemonic = [&](const char *mnemonic = "(illegal)") {
            print(colors.illegalMnemonic, mnemonic);
        };
        auto printUnknownMnemonic = [&] { printIllegalMnemonic("(?)"); };
        auto printCond = [&](const char *cond) { print(colors.cond, cond); };
        auto printOperator = [&](const char *oper) { print(colors.oper, oper); };
        auto printComma = [&] { printOperator(", "); };
        auto printSizeSuffix = [&](const char *size) {
            printOperator(".");
            print(colors.sizeSuffix, size);
        };

        auto printOpRead = [&](std::string_view op) { print(colors.opRead, op); };
        auto printOpWrite = [&](std::string_view op) { print(colors.opWrite, op); };
        auto printOpReadWrite = [&](std::string_view op) { print(colors.opReadWrite, op); };

        auto printOp = [&](std::string_view op, bool read, bool write) {
            if (read && write) {
                printOpReadWrite(op);
            } else if (write) {
                printOpWrite(op);
            } else {
                printOpRead(op);
            }
        };

        auto printAddrInc = [&] { print(colors.addrInc, "+"); };
        auto printAddrDec = [&] { print(colors.addrDec, "-"); };

        auto printReset = [&] { fmt::println("{}", colors.reset); };

        // Disassemble code
        std::string lcisa = isa;
        std::transform(lcisa.cbegin(), lcisa.cend(), lcisa.begin(), [](char c) { return std::tolower(c); });
        if (lcisa == "sh2" || lcisa == "sh-2") {
            auto maybeAddress = ParseHex<uint32>(origin);
            if (!maybeAddress) {
                fmt::println("Invalid origin address: {}", origin);
                return 1;
            }
            uint32 address = *maybeAddress;
            bool isDelaySlot = false;

            auto printAddress = [&](uint32 address) {
                if (!hideAddresses) {
                    print(colors.address, fmt::format("{:08X}  ", address), false);
                }
            };

            auto printOpcode = [&](uint16 opcode) {
                if (!hideOpcodes) {
                    print(colors.bytes, fmt::format("{:04X}  ", opcode), false);
                }
            };

            auto printDelaySlotPrefix = [&] { print(colors.delaySlot, "> "); };

            auto printInstruction = [&](const sh2::DisassembledInstruction &instr, bool delaySlot) {
                if (delaySlot) {
                    if (!instr.validInDelaySlot) {
                        printIllegalMnemonic();
                        return;
                    }
                    printDelaySlotPrefix();
                }

                switch (instr.mnemonic) {
                case sh2::Mnemonic::NOP: printMnemonic("nop"); break;
                case sh2::Mnemonic::SLEEP: printMnemonic("sleep"); break;
                case sh2::Mnemonic::MOV: printMnemonic("mov"); break;
                case sh2::Mnemonic::MOVA: printMnemonic("mova"); break;
                case sh2::Mnemonic::MOVT: printMnemonic("movt"); break;
                case sh2::Mnemonic::CLRT: printMnemonic("clrt"); break;
                case sh2::Mnemonic::SETT: printMnemonic("sett"); break;
                case sh2::Mnemonic::EXTU: printMnemonic("extu"); break;
                case sh2::Mnemonic::EXTS: printMnemonic("exts"); break;
                case sh2::Mnemonic::SWAP: printMnemonic("swap"); break;
                case sh2::Mnemonic::XTRCT: printMnemonic("xtrct"); break;
                case sh2::Mnemonic::LDC: printMnemonic("ldc"); break;
                case sh2::Mnemonic::LDS: printMnemonic("lds"); break;
                case sh2::Mnemonic::STC: printMnemonic("stc"); break;
                case sh2::Mnemonic::STS: printMnemonic("sts"); break;
                case sh2::Mnemonic::ADD: printMnemonic("add"); break;
                case sh2::Mnemonic::ADDC: printMnemonic("addc"); break;
                case sh2::Mnemonic::ADDV: printMnemonic("addv"); break;
                case sh2::Mnemonic::AND: printMnemonic("and"); break;
                case sh2::Mnemonic::NEG: printMnemonic("neg"); break;
                case sh2::Mnemonic::NEGC: printMnemonic("negc"); break;
                case sh2::Mnemonic::NOT: printMnemonic("not"); break;
                case sh2::Mnemonic::OR: printMnemonic("or"); break;
                case sh2::Mnemonic::ROTCL: printMnemonic("rotcl"); break;
                case sh2::Mnemonic::ROTCR: printMnemonic("rotcr"); break;
                case sh2::Mnemonic::ROTL: printMnemonic("rotl"); break;
                case sh2::Mnemonic::ROTR: printMnemonic("rotr"); break;
                case sh2::Mnemonic::SHAL: printMnemonic("shal"); break;
                case sh2::Mnemonic::SHAR: printMnemonic("shar"); break;
                case sh2::Mnemonic::SHLL: printMnemonic("shll"); break;
                case sh2::Mnemonic::SHLL2: printMnemonic("shll2"); break;
                case sh2::Mnemonic::SHLL8: printMnemonic("shll8"); break;
                case sh2::Mnemonic::SHLL16: printMnemonic("shll16"); break;
                case sh2::Mnemonic::SHLR: printMnemonic("shlr"); break;
                case sh2::Mnemonic::SHLR2: printMnemonic("shlr2"); break;
                case sh2::Mnemonic::SHLR8: printMnemonic("shlr8"); break;
                case sh2::Mnemonic::SHLR16: printMnemonic("shlr16"); break;
                case sh2::Mnemonic::SUB: printMnemonic("sub"); break;
                case sh2::Mnemonic::SUBC: printMnemonic("subc"); break;
                case sh2::Mnemonic::SUBV: printMnemonic("subv"); break;
                case sh2::Mnemonic::XOR: printMnemonic("xor"); break;
                case sh2::Mnemonic::DT: printMnemonic("dt"); break;
                case sh2::Mnemonic::CLRMAC: printMnemonic("clrmac"); break;
                case sh2::Mnemonic::MAC: printMnemonic("mac"); break;
                case sh2::Mnemonic::MUL: printMnemonic("mul"); break;
                case sh2::Mnemonic::MULS: printMnemonic("muls"); break;
                case sh2::Mnemonic::MULU: printMnemonic("mulu"); break;
                case sh2::Mnemonic::DMULS: printMnemonic("dmuls"); break;
                case sh2::Mnemonic::DMULU: printMnemonic("dmulu"); break;
                case sh2::Mnemonic::DIV0S: printMnemonic("div0s"); break;
                case sh2::Mnemonic::DIV0U: printMnemonic("div0u"); break;
                case sh2::Mnemonic::DIV1: printMnemonic("div1"); break;
                case sh2::Mnemonic::CMP_EQ:
                    printMnemonic("cmp");
                    printOperator("/");
                    printCond("eq");
                    break;
                case sh2::Mnemonic::CMP_GE:
                    printMnemonic("cmp");
                    printOperator("/");
                    printCond("ge");
                    break;
                case sh2::Mnemonic::CMP_GT:
                    printMnemonic("cmp");
                    printOperator("/");
                    printCond("gt");
                    break;
                case sh2::Mnemonic::CMP_HI:
                    printMnemonic("cmp");
                    printOperator("/");
                    printCond("hi");
                    break;
                case sh2::Mnemonic::CMP_HS:
                    printMnemonic("cmp");
                    printOperator("/");
                    printCond("hs");
                    break;
                case sh2::Mnemonic::CMP_PL:
                    printMnemonic("cmp");
                    printOperator("/");
                    printCond("pl");
                    break;
                case sh2::Mnemonic::CMP_PZ:
                    printMnemonic("cmp");
                    printOperator("/");
                    printCond("pz");
                    break;
                case sh2::Mnemonic::CMP_STR:
                    printMnemonic("cmp");
                    printOperator("/");
                    printCond("str");
                    break;
                case sh2::Mnemonic::TAS: printMnemonic("tas"); break;
                case sh2::Mnemonic::TST: printMnemonic("tst"); break;
                case sh2::Mnemonic::BF:
                    printMnemonic("b");
                    printCond("f");
                    break;
                case sh2::Mnemonic::BFS:
                    printMnemonic("b");
                    printCond("f");
                    printOperator("/");
                    printMnemonic("s");
                    break;
                case sh2::Mnemonic::BT:
                    printMnemonic("b");
                    printCond("t");
                    break;
                case sh2::Mnemonic::BTS:
                    printMnemonic("b");
                    printCond("t");
                    printOperator("/");
                    printMnemonic("s");
                    break;
                case sh2::Mnemonic::BRA: printMnemonic("bra"); break;
                case sh2::Mnemonic::BRAF: printMnemonic("braf"); break;
                case sh2::Mnemonic::BSR: printMnemonic("bsr"); break;
                case sh2::Mnemonic::BSRF: printMnemonic("bsrf"); break;
                case sh2::Mnemonic::JMP: printMnemonic("jmp"); break;
                case sh2::Mnemonic::JSR: printMnemonic("jsr"); break;
                case sh2::Mnemonic::TRAPA: printMnemonic("trapa"); break;
                case sh2::Mnemonic::RTE: printMnemonic("rte"); break;
                case sh2::Mnemonic::RTS: printMnemonic("rts"); break;
                case sh2::Mnemonic::Illegal: printIllegalMnemonic(); break;
                default: printUnknownMnemonic(); break;
                }

                switch (instr.opSize) {
                case sh2::OperandSize::Byte: printSizeSuffix("b"); break;
                case sh2::OperandSize::Word: printSizeSuffix("w"); break;
                case sh2::OperandSize::Long: printSizeSuffix("l"); break;
                default: break;
                }
            };

            auto printRnRead = [&](uint8 rn) { printOpRead(fmt::format("r{}", rn)); };
            auto printRnWrite = [&](uint8 rn) { printOpWrite(fmt::format("r{}", rn)); };
            auto printRnReadWrite = [&](uint8 rn) { printOpReadWrite(fmt::format("r{}", rn)); };

            auto printRn = [&](uint8 rn, bool read, bool write) {
                if (read && write) {
                    printRnReadWrite(rn);
                } else if (write) {
                    printRnWrite(rn);
                } else {
                    printRnRead(rn);
                }
            };

            auto printRWSymbol = [&](const char *symbol, bool write) {
                const auto color = write ? colors.opWrite : colors.opRead;
                print(color, symbol);
            };

            auto printImm = [&](sint32 imm) {
                print(colors.immediate, fmt::format("#0x{:X}", static_cast<uint32>(imm)));
            };

            auto printSH2Op = [&](const sh2::Operand &op) {
                if (op.type == sh2::Operand::Type::None) {
                    return;
                }

                switch (op.type) {
                case sh2::Operand::Type::Imm: printImm(op.immDisp); break;
                case sh2::Operand::Type::Rn: printRn(op.reg, op.read, op.write); break;
                case sh2::Operand::Type::AtRn:
                    printRWSymbol("@", op.write);
                    printRnRead(op.reg);
                    break;
                case sh2::Operand::Type::AtRnPlus:
                    printRWSymbol("@", op.write);
                    printRnReadWrite(op.reg);
                    printAddrInc();
                    break;
                case sh2::Operand::Type::AtMinusRn:
                    printRWSymbol("@", op.write);
                    printAddrDec();
                    printRnReadWrite(op.reg);
                    break;
                case sh2::Operand::Type::AtDispRn:
                    printRWSymbol("@(", op.write);
                    printImm(op.immDisp);
                    printComma();
                    printRnRead(op.reg);
                    printRWSymbol(")", op.write);
                    break;
                case sh2::Operand::Type::AtR0Rn:
                    printRWSymbol("@(", op.write);
                    printRnRead(0);
                    printComma();
                    printRnRead(op.reg);
                    printRWSymbol(")", op.write);
                    break;
                case sh2::Operand::Type::AtDispGBR:
                    printRWSymbol("@(", op.write);
                    printImm(op.immDisp);
                    printComma();
                    printOpRead("gbr");
                    printRWSymbol(")", op.write);
                    break;
                case sh2::Operand::Type::AtR0GBR:
                    printRWSymbol("@(", op.write);
                    printRnRead(0);
                    printComma();
                    printOpRead("gbr");
                    printRWSymbol(")", op.write);
                    break;
                case sh2::Operand::Type::AtDispPC:
                    printRWSymbol("@(", false);
                    printImm(address + op.immDisp);
                    printRWSymbol(")", false);
                    break;
                case sh2::Operand::Type::AtDispPCWordAlign:
                    printRWSymbol("@(", false);
                    printImm((address & ~3) + op.immDisp);
                    printRWSymbol(")", false);
                    break;
                case sh2::Operand::Type::DispPC: printImm(address + op.immDisp); break;
                case sh2::Operand::Type::RnPC: printRnRead(op.reg); break;
                case sh2::Operand::Type::SR: printOp("sr", op.read, op.write); break;
                case sh2::Operand::Type::GBR: printOp("gbr", op.read, op.write); break;
                case sh2::Operand::Type::VBR: printOp("vbr", op.read, op.write); break;
                case sh2::Operand::Type::MACH: printOp("mach", op.read, op.write); break;
                case sh2::Operand::Type::MACL: printOp("macl", op.read, op.write); break;
                case sh2::Operand::Type::PR: printOp("pr", op.read, op.write); break;
                default: break;
                }
            };

            auto printOp1 = [&](const sh2::DisassembledInstruction &instr) { printSH2Op(instr.op1); };
            auto printOp2 = [&](const sh2::DisassembledInstruction &instr) { printSH2Op(instr.op2); };

            auto printComment = [&](std::string_view comment) { print(colors.comment, comment); };

            if (inputFile.empty()) {
                for (auto &opcodeStr : args) {
                    std::string_view strippedOpcode = opcodeStr;

                    // 0 = no change; +1 = force delay slot, -1 = force non-delay slot
                    int forceDelaySlot = 0;
                    if (opcodeStr.starts_with('_')) {
                        forceDelaySlot = +1;
                        strippedOpcode = strippedOpcode.substr(1);
                    } else if (opcodeStr.starts_with('!')) {
                        forceDelaySlot = -1;
                        strippedOpcode = strippedOpcode.substr(1);
                    }

                    auto maybeOpcode = ParseHex<uint16>(strippedOpcode);
                    if (!maybeOpcode) {
                        fmt::println("Invalid opcode: {}", opcodeStr);
                        return 1;
                    }
                    const uint16 opcode = *maybeOpcode;

                    const sh2::DisassembledInstruction &instr = sh2::Disassemble(opcode);

                    bool delaySlotState = (isDelaySlot || forceDelaySlot > 0) && forceDelaySlot >= 0;

                    printAddress(address);
                    printOpcode(opcode);
                    printInstruction(instr, delaySlotState);
                    align(12);
                    printOp1(instr);
                    if (instr.op1.type != sh2::Operand::Type::None && instr.op2.type != sh2::Operand::Type::None) {
                        printComma();
                    }
                    printOp2(instr);

                    align(34);
                    if (forceDelaySlot > 0) {
                        printComment("; delay slot override");
                    } else if (forceDelaySlot < 0) {
                        printComment("; non-delay slot override");
                    }

                    printReset();
                    x = 0;

                    isDelaySlot = instr.hasDelaySlot;
                    address += sizeof(opcode);
                }
            } else {
                std::ifstream in{inputFile, std::ios::binary};
                if (!in) {
                    std::error_code error{errno, std::generic_category()};
                    fmt::println("Could not open file: {}", error.message());
                    return 1;
                }

                in.seekg(0, std::ios::end);
                const size_t fileSize = in.tellg();

                size_t offset = 0;
                size_t length = fileSize;

                if (args.size() >= 1) {
                    auto maybeOffset = ParseHex<size_t>(args[0]);
                    if (!maybeOffset) {
                        fmt::println("Invalid offset: {}", args[0]);
                        return 1;
                    }
                    offset = *maybeOffset;
                }
                if (args.size() >= 2) {
                    auto maybeLength = ParseHex<size_t>(args[1]);
                    if (!maybeLength) {
                        fmt::println("Invalid length: {}", args[1]);
                        return 1;
                    }
                    length = *maybeLength;
                }

                length = std::min(length, fileSize - offset);

                in.seekg(offset, std::ios::beg);

                uint16 opcode{};

                auto readOpcode = [&] {
                    uint8 b[2] = {0};
                    in.read((char *)b, 2);
                    opcode = (static_cast<uint16>(b[0]) << 8u) | static_cast<uint16>(b[1]);
                    return in.good();
                };

                while (readOpcode()) {
                    const sh2::DisassembledInstruction &instr = sh2::Disassemble(opcode);

                    printAddress(address);
                    printOpcode(opcode);
                    printInstruction(instr, isDelaySlot);
                    align(12);
                    printOp1(instr);
                    if (instr.op1.type != sh2::Operand::Type::None && instr.op2.type != sh2::Operand::Type::None) {
                        printComma();
                    }
                    printOp2(instr);

                    printReset();
                    x = 0;

                    isDelaySlot = instr.hasDelaySlot;
                    address += sizeof(opcode);
                }
            }
        } else if (lcisa == "m68k" || lcisa == "m68000") {
            auto maybeAddress = ParseHex<uint32>(origin);
            if (!maybeAddress) {
                fmt::println("Invalid origin address: {}", origin);
                return 1;
            }
            uint32 address = *maybeAddress;
            const uint32 finalAddress = address + args.size() * sizeof(uint16);

            auto printAddress = [&](uint32 address) {
                if (!hideAddresses) {
                    print(colors.address, fmt::format("{:08X}  ", address), false);
                }
            };

            auto printOpcodes = [&](std::span<const uint16> opcodes) {
                if (!hideOpcodes) {
                    for (auto opcode : opcodes) {
                        print(colors.bytes, fmt::format("{:04X} ", opcode), false);
                    }
                    for (int i = opcodes.size(); i < 5; i++) {
                        printRaw("     ", false);
                    }
                    printRaw(" ", false);
                }
            };

            auto printM68KCond = [&](m68k::Condition cond) {
                switch (cond) {
                case m68k::Condition::T: printCond("t"); break;
                case m68k::Condition::F: printCond("f"); break;
                case m68k::Condition::HI: printCond("hi"); break;
                case m68k::Condition::LS: printCond("ls"); break;
                case m68k::Condition::CC: printCond("cc"); break;
                case m68k::Condition::CS: printCond("cs"); break;
                case m68k::Condition::NE: printCond("ne"); break;
                case m68k::Condition::EQ: printCond("eq"); break;
                case m68k::Condition::VC: printCond("vc"); break;
                case m68k::Condition::VS: printCond("vs"); break;
                case m68k::Condition::PL: printCond("pl"); break;
                case m68k::Condition::MI: printCond("mi"); break;
                case m68k::Condition::GE: printCond("ge"); break;
                case m68k::Condition::LT: printCond("lt"); break;
                case m68k::Condition::GT: printCond("gt"); break;
                case m68k::Condition::LE: printCond("le"); break;
                }
            };

            auto printInstruction = [&](const m68k::DisassembledInstruction &instr) {
                switch (instr.info.mnemonic) {
                case m68k::Mnemonic::Move: printMnemonic("move"); break;
                case m68k::Mnemonic::MoveA: printMnemonic("movea"); break;
                case m68k::Mnemonic::MoveM: printMnemonic("movem"); break;
                case m68k::Mnemonic::MoveP: printMnemonic("movep"); break;
                case m68k::Mnemonic::MoveQ: printMnemonic("moveq"); break;
                case m68k::Mnemonic::Clr: printMnemonic("clr"); break;
                case m68k::Mnemonic::Exg: printMnemonic("exg"); break;
                case m68k::Mnemonic::Ext: printMnemonic("ext"); break;
                case m68k::Mnemonic::Swap: printMnemonic("swap"); break;
                case m68k::Mnemonic::ABCD: printMnemonic("abcd"); break;
                case m68k::Mnemonic::NBCD: printMnemonic("nbcd"); break;
                case m68k::Mnemonic::SBCD: printMnemonic("sbcd"); break;
                case m68k::Mnemonic::Add: printMnemonic("add"); break;
                case m68k::Mnemonic::AddA: printMnemonic("adda"); break;
                case m68k::Mnemonic::AddI: printMnemonic("addi"); break;
                case m68k::Mnemonic::AddQ: printMnemonic("addq"); break;
                case m68k::Mnemonic::AddX: printMnemonic("addx"); break;
                case m68k::Mnemonic::And: printMnemonic("and"); break;
                case m68k::Mnemonic::AndI: printMnemonic("andi"); break;
                case m68k::Mnemonic::Eor: printMnemonic("eor"); break;
                case m68k::Mnemonic::EorI: printMnemonic("eori"); break;
                case m68k::Mnemonic::Neg: printMnemonic("neg"); break;
                case m68k::Mnemonic::NegX: printMnemonic("negx"); break;
                case m68k::Mnemonic::Not: printMnemonic("not"); break;
                case m68k::Mnemonic::Or: printMnemonic("or"); break;
                case m68k::Mnemonic::OrI: printMnemonic("ori"); break;
                case m68k::Mnemonic::Sub: printMnemonic("sub"); break;
                case m68k::Mnemonic::SubA: printMnemonic("suba"); break;
                case m68k::Mnemonic::SubI: printMnemonic("subi"); break;
                case m68k::Mnemonic::SubQ: printMnemonic("subq"); break;
                case m68k::Mnemonic::SubX: printMnemonic("subx"); break;
                case m68k::Mnemonic::DivS: printMnemonic("divs"); break;
                case m68k::Mnemonic::DivU: printMnemonic("divu"); break;
                case m68k::Mnemonic::MulS: printMnemonic("muls"); break;
                case m68k::Mnemonic::MulU: printMnemonic("mulu"); break;
                case m68k::Mnemonic::BChg: printMnemonic("bchg"); break;
                case m68k::Mnemonic::BClr: printMnemonic("bclr"); break;
                case m68k::Mnemonic::BSet: printMnemonic("bset"); break;
                case m68k::Mnemonic::BTst: printMnemonic("btst"); break;
                case m68k::Mnemonic::ASL: printMnemonic("asl"); break;
                case m68k::Mnemonic::ASR: printMnemonic("asr"); break;
                case m68k::Mnemonic::LSL: printMnemonic("lsl"); break;
                case m68k::Mnemonic::LSR: printMnemonic("lsr"); break;
                case m68k::Mnemonic::ROL: printMnemonic("rol"); break;
                case m68k::Mnemonic::ROR: printMnemonic("ror"); break;
                case m68k::Mnemonic::ROXL: printMnemonic("roxl"); break;
                case m68k::Mnemonic::ROXR: printMnemonic("roxr"); break;
                case m68k::Mnemonic::Cmp: printMnemonic("cmp"); break;
                case m68k::Mnemonic::CmpA: printMnemonic("cmpa"); break;
                case m68k::Mnemonic::CmpI: printMnemonic("cmpi"); break;
                case m68k::Mnemonic::CmpM: printMnemonic("cmpm"); break;
                case m68k::Mnemonic::Scc:
                    printMnemonic("s");
                    printM68KCond(instr.info.cond);
                    break;
                case m68k::Mnemonic::TAS: printMnemonic("tas"); break;
                case m68k::Mnemonic::Tst: printMnemonic("tst"); break;
                case m68k::Mnemonic::LEA: printMnemonic("lea"); break;
                case m68k::Mnemonic::PEA: printMnemonic("pea"); break;
                case m68k::Mnemonic::Link: printMnemonic("link"); break;
                case m68k::Mnemonic::Unlink: printMnemonic("unlk"); break;
                case m68k::Mnemonic::BRA: printMnemonic("bra"); break;
                case m68k::Mnemonic::BSR: printMnemonic("bsr"); break;
                case m68k::Mnemonic::Bcc:
                    printMnemonic("b");
                    printM68KCond(instr.info.cond);
                    break;
                case m68k::Mnemonic::DBcc:
                    printMnemonic("db");
                    printM68KCond(instr.info.cond);
                    break;
                case m68k::Mnemonic::JSR: printMnemonic("jsr"); break;
                case m68k::Mnemonic::Jmp: printMnemonic("jmp"); break;
                case m68k::Mnemonic::RTE: printMnemonic("rte"); break;
                case m68k::Mnemonic::RTR: printMnemonic("rtr"); break;
                case m68k::Mnemonic::RTS: printMnemonic("rts"); break;
                case m68k::Mnemonic::Chk: printMnemonic("chk"); break;
                case m68k::Mnemonic::Reset: printMnemonic("reset"); break;
                case m68k::Mnemonic::Stop: printMnemonic("stop"); break;
                case m68k::Mnemonic::Trap: printMnemonic("trap"); break;
                case m68k::Mnemonic::TrapV: printMnemonic("trapv"); break;
                case m68k::Mnemonic::Noop: printMnemonic("nop"); break;

                case m68k::Mnemonic::Illegal1010: printIllegalMnemonic("(illegal 1010)"); break;
                case m68k::Mnemonic::Illegal1111: printIllegalMnemonic("(illegal 1111)"); break;
                case m68k::Mnemonic::Illegal: printIllegalMnemonic(); break;
                }

                switch (instr.info.opSize) {
                case m68k::OperandSize::Byte: printSizeSuffix("b"); break;
                case m68k::OperandSize::Word: printSizeSuffix("w"); break;
                case m68k::OperandSize::Long: printSizeSuffix("l"); break;
                default: break;
                }
            };

            auto printUImm8 = [&](uint8 imm, bool hashPrefix) {
                print(colors.immediate, fmt::format("{}${:X}", (hashPrefix ? "#" : ""), imm));
            };
            auto printUImm16 = [&](uint16 imm, bool hashPrefix) {
                print(colors.immediate, fmt::format("{}${:X}", (hashPrefix ? "#" : ""), imm));
            };
            auto printUImm32 = [&](uint32 imm, bool hashPrefix) {
                print(colors.immediate, fmt::format("{}${:X}", (hashPrefix ? "#" : ""), imm));
            };
            auto printSImm16 = [&](sint16 imm, bool hashPrefix) {
                print(colors.immediate,
                      fmt::format("{}{}${:X}", (imm < 0 ? "-" : ""), (hashPrefix ? "#" : ""), (uint16)abs(imm)));
            };
            auto printSImm32 = [&](sint32 imm, bool hashPrefix) {
                print(colors.immediate,
                      fmt::format("{}{}${:X}", (imm < 0 ? "-" : ""), (hashPrefix ? "#" : ""), (uint32)abs(imm)));
            };

            auto printRegSublist = [&](uint16 regList, bool read, bool write, char regPrefix, bool &printedOnce) {
                uint16 bits = bit::extract<0, 7>(regList);
                uint16 pos = 0;
                uint16 numOnes = std::countr_one(bits);
                while (bits != 0) {
                    if (numOnes >= 1) {
                        if (printedOnce) {
                            printOperator("/");
                        }
                        if (numOnes == 1) {
                            printOp(fmt::format("{}{}", regPrefix, pos), read, write);
                        } else {
                            printOp(fmt::format("{0}{1}-{0}{2}", regPrefix, pos, pos + numOnes - 1), read, write);
                        }
                        printedOnce = true;
                    }

                    bits >>= numOnes;
                    const uint16 numZeros = std::countr_zero(bits);
                    bits >>= numZeros;
                    pos += numOnes + numZeros;
                    numOnes = std::countr_one(bits);
                }
            };

            auto printRegList = [&](uint16 regList, bool read, bool write) {
                bool printedOnce = false;
                printRegSublist(bit::extract<0, 7>(regList), read, write, 'd', printedOnce);
                printRegSublist(bit::extract<8, 15>(regList), read, write, 'a', printedOnce);
            };

            auto printM68KOp = [&](const m68k::Operand &op, const m68k::OperandDetails &det) {
                switch (op.type) {
                case m68k::Operand::Type::None: break;
                case m68k::Operand::Type::Dn: printOp(fmt::format("d{}", op.rn), op.read, op.write); break;
                case m68k::Operand::Type::An: printOp(fmt::format("a{}", op.rn), op.read, op.write); break;
                case m68k::Operand::Type::AtAn:
                    printOp("(", op.read, op.write);
                    printOpRead(fmt::format("a{}", op.rn));
                    printOp(")", op.read, op.write);
                    break;
                case m68k::Operand::Type::AtAnPlus:
                    printOp("(", op.read, op.write);
                    printOpReadWrite(fmt::format("a{}", op.rn));
                    printOp(")", op.read, op.write);
                    printAddrInc();
                    break;
                case m68k::Operand::Type::MinusAtAn:
                    printAddrDec();
                    printOp("(", op.read, op.write);
                    printOpReadWrite(fmt::format("a{}", op.rn));
                    printOp(")", op.read, op.write);
                    break;
                case m68k::Operand::Type::AtDispAn:
                    printSImm16(det.immDisp, false);
                    printOp("(", op.read, op.write);
                    printOpRead(fmt::format("a{}", op.rn));
                    printOp(")", op.read, op.write);
                    break;
                case m68k::Operand::Type::AtDispAnIx:
                    printSImm16(det.immDisp, false);
                    printOp("(", op.read, op.write);
                    printOpRead(fmt::format("a{}", op.rn));
                    printComma();
                    printOpRead(fmt::format("{}{}", (det.ix >= 8 ? 'a' : 'd'), det.ix & 7));
                    printSizeSuffix(det.ixLong ? "l" : "w");
                    printOp(")", op.read, op.write);
                    break;
                case m68k::Operand::Type::AtDispPC:
                    printUImm32(det.immDisp + address, false);
                    printOp("(", op.read, op.write);
                    printOpRead("pc");
                    printOp(")", op.read, op.write);
                    break;
                case m68k::Operand::Type::AtDispPCIx:
                    printUImm32(det.immDisp + address, false);
                    printOp("(", op.read, op.write);
                    printOpRead("pc");
                    printComma();
                    printOpRead(fmt::format("{}{}", (det.ix >= 8 ? 'a' : 'd'), det.ix & 7));
                    printSizeSuffix(det.ixLong ? "l" : "w");
                    printOp(")", op.read, op.write);
                    break;
                case m68k::Operand::Type::AtImmWord:
                    printUImm16(det.immDisp, false);
                    printSizeSuffix("w");
                    break;
                case m68k::Operand::Type::AtImmLong:
                    printUImm32(det.immDisp, false);
                    printSizeSuffix("l");
                    break;
                case m68k::Operand::Type::UImmEmbedded: printUImm32(op.simm, true); break;
                case m68k::Operand::Type::UImm8Fetched: printUImm8(det.immDisp, true); break;
                case m68k::Operand::Type::UImm16Fetched: printUImm16(det.immDisp, true); break;
                case m68k::Operand::Type::UImm32Fetched: printUImm32(det.immDisp, true); break;

                case m68k::Operand::Type::WordDispPCEmbedded: printUImm32(op.simm + address, false); break;
                case m68k::Operand::Type::WordDispPCFetched: printUImm32(det.immDisp + address, false); break;

                case m68k::Operand::Type::CCR: printOp("ccr", op.read, op.write); break;
                case m68k::Operand::Type::SR: printOp("sr", op.read, op.write); break;
                case m68k::Operand::Type::USP: printOp("usp", op.read, op.write); break;

                case m68k::Operand::Type::RegList: printRegList(det.regList, op.read, op.write); break;
                case m68k::Operand::Type::RevRegList: printRegList(det.regList, op.read, op.write); break;
                }
            };

            auto printOp1 = [&](const m68k::DisassembledInstruction &instr) { printM68KOp(instr.info.op1, instr.op1); };
            auto printOp2 = [&](const m68k::DisassembledInstruction &instr) { printM68KOp(instr.info.op2, instr.op2); };

            size_t opcodeOffset = 0;

            if (inputFile.empty()) {
                bool valid = true;

                while (true) {
                    uint32 baseAddress = address;

                    const m68k::DisassembledInstruction instr = m68k::Disassemble([&]() -> uint16 {
                        if (!valid) {
                            return 0;
                        }
                        if (opcodeOffset >= args.size()) {
                            valid = false;
                            return 0;
                        }

                        std::string opcodeStr = args[opcodeOffset];
                        auto maybeOpcode = ParseHex<uint32>(opcodeStr);
                        if (!maybeOpcode) {
                            fmt::println("Invalid opcode: {}", opcodeStr);
                            valid = false;
                            return 0;
                        }

                        address += sizeof(uint16);
                        ++opcodeOffset;
                        return *maybeOpcode;
                    });

                    if (!valid) {
                        break;
                    }

                    printAddress(baseAddress);
                    printOpcodes(instr.opcodes);
                    printInstruction(instr);
                    align(9);
                    printOp1(instr);
                    if (instr.info.op1.type != m68k::Operand::Type::None &&
                        instr.info.op2.type != m68k::Operand::Type::None) {
                        printComma();
                    }
                    printOp2(instr);

                    printReset();
                    x = 0;
                }
            } else {
            }
        } else if (lcisa == "scudsp") {
            auto maybeAddress = ParseHex<uint8>(origin);
            if (!maybeAddress) {
                fmt::println("Invalid origin address: {}", origin);
                return 1;
            }
            uint8 address = *maybeAddress;

            auto printAddress = [&](uint8 address) {
                if (!hideAddresses) {
                    print(colors.address, fmt::format("{:02X}  ", address), false);
                }
            };

            auto printOpcode = [&](uint32 opcode) {
                if (!hideOpcodes) {
                    print(colors.bytes, fmt::format("{:08X}  ", opcode), false);
                }
            };

            auto printU8 = [&](uint8 imm) { print(colors.immediate, fmt::format("#0x{:X}", imm)); };
            auto printS8 = [&](sint8 imm) {
                print(colors.immediate, fmt::format("#{}0x{:X}", (imm < 0 ? "-" : ""), abs(imm)));
            };
            auto printS32 = [&](sint32 imm) {
                print(colors.immediate, fmt::format("#{}0x{:X}", (imm < 0 ? "-" : ""), abs(imm)));
            };

            auto printCond = [&](scu::SCUDSPInstruction::Cond cond) { print(colors.cond, scu::ToString(cond)); };

            auto printInstruction = [&](const scu::SCUDSPInstruction &instr) {
                switch (instr.type) {
                case scu::SCUDSPInstruction::Type::Operation: //
                {
                    if (instr.operation.aluOp == scu::SCUDSPInstruction::ALUOp::NOP) {
                        printNOP("NOP");
                    } else {
                        printMnemonic(scu::ToString(instr.operation.aluOp));
                    }
                    align(5);

                    switch (instr.operation.xbusPOp) {
                    case scu::SCUDSPInstruction::XBusPOp::NOP: printNOP("NOP"); break;
                    case scu::SCUDSPInstruction::XBusPOp::MOV_MUL_P:
                        printMnemonic("MOV ");
                        printOpRead("MUL");
                        printComma();
                        printOpWrite("P");
                        break;
                    case scu::SCUDSPInstruction::XBusPOp::MOV_S_P:
                        printMnemonic("MOV ");
                        printOpRead(scu::ToString(instr.operation.xbusSrc));
                        printComma();
                        printOpWrite("P");
                        break;
                    }
                    align(17);

                    if (instr.operation.xbusXOp) {
                        printMnemonic("MOV ");
                        printOpRead(scu::ToString(instr.operation.xbusSrc));
                        printComma();
                        printOpWrite("X");
                    } else {
                        printNOP("NOP");
                    }
                    align(29);

                    switch (instr.operation.ybusAOp) {
                    case scu::SCUDSPInstruction::YBusAOp::NOP: printNOP("NOP"); break;
                    case scu::SCUDSPInstruction::YBusAOp::CLR_A:
                        printMnemonic("CLR ");
                        printOpWrite("A");
                        break;
                    case scu::SCUDSPInstruction::YBusAOp::MOV_ALU_A:
                        printMnemonic("MOV ");
                        printOpRead("ALU");
                        printComma();
                        printOpWrite("A");
                        break;
                    case scu::SCUDSPInstruction::YBusAOp::MOV_S_A:
                        printMnemonic("MOV ");
                        printOpRead(scu::ToString(instr.operation.xbusSrc));
                        printComma();
                        printOpWrite("A");
                        break;
                    }
                    align(41);

                    if (instr.operation.ybusYOp) {
                        printMnemonic("MOV ");
                        printOpRead(scu::ToString(instr.operation.ybusSrc));
                        printComma();
                        printOpWrite("Y");
                    } else {
                        printNOP("NOP");
                    }
                    align(53);

                    switch (instr.operation.d1BusOp) {
                    case scu::SCUDSPInstruction::D1BusOp::NOP: printNOP("NOP"); break;
                    case scu::SCUDSPInstruction::D1BusOp::MOV_SIMM_D:
                        printMnemonic("MOV ");
                        printS8(instr.operation.d1BusSrc.imm);
                        printComma();
                        printOpWrite(scu::ToString(instr.operation.d1BusDst));
                        break;
                    case scu::SCUDSPInstruction::D1BusOp::MOV_S_D:
                        printMnemonic("MOV ");
                        printOpRead(scu::ToString(instr.operation.d1BusSrc.reg));
                        printComma();
                        printOpWrite(scu::ToString(instr.operation.d1BusDst));
                        break;
                    }

                    break;
                }
                case scu::SCUDSPInstruction::Type::MVI:
                    printMnemonic("MVI ");
                    printS32(instr.mvi.imm);
                    printComma();
                    printOpWrite(scu::ToString(instr.mvi.dst));
                    if (instr.mvi.cond != scu::SCUDSPInstruction::Cond::None) {
                        printComma();
                        printCond(instr.mvi.cond);
                    }
                    break;
                case scu::SCUDSPInstruction::Type::DMA:
                    printMnemonic(instr.dma.hold ? "DMAH " : "DMA ");
                    if (instr.dma.toD0) {
                        printOpRead(scu::ToString(instr.dma.ramOp));
                    } else {
                        printOpRead("D0");
                    }
                    printComma();
                    if (instr.dma.toD0) {
                        printOpWrite("D0");
                    } else {
                        printOpWrite(scu::ToString(instr.dma.ramOp));
                    }
                    printComma();
                    if (instr.dma.countType) {
                        auto reg = fmt::format("M{}{}", (instr.dma.count.ct < 4 ? "" : "C"), instr.dma.count.ct & 3);
                        printOpRead(reg);
                    } else {
                        printU8(instr.dma.count.imm);
                    }
                    break;
                case scu::SCUDSPInstruction::Type::JMP:
                    printMnemonic("JMP ");
                    if (instr.jmp.cond != scu::SCUDSPInstruction::Cond::None) {
                        printCond(instr.jmp.cond);
                        printComma();
                    }
                    printU8(instr.jmp.target);
                    break;
                case scu::SCUDSPInstruction::Type::LPS: printMnemonic("LPS"); break;
                case scu::SCUDSPInstruction::Type::BTM: printMnemonic("BTM"); break;
                case scu::SCUDSPInstruction::Type::END: printMnemonic("END"); break;
                case scu::SCUDSPInstruction::Type::ENDI: printMnemonic("ENDI"); break;
                default: printIllegalMnemonic(); break;
                }
            };

            if (inputFile.empty()) {
                for (auto &opcodeStr : args) {
                    auto maybeOpcode = ParseHex<uint32>(opcodeStr);
                    if (!maybeOpcode) {
                        fmt::println("Invalid opcode: {}", opcodeStr);
                        return 1;
                    }
                    const uint32 opcode = *maybeOpcode;

                    const scu::SCUDSPInstruction &instr = scu::Disassemble(opcode);

                    printAddress(address);
                    printOpcode(opcode);
                    printInstruction(instr);

                    printReset();
                    x = 0;

                    ++address;
                }
            } else {
                std::ifstream in{inputFile, std::ios::binary};
                if (!in) {
                    std::error_code error{errno, std::generic_category()};
                    fmt::println("Could not open file: {}", error.message());
                    return 1;
                }

                in.seekg(0, std::ios::end);
                const size_t fileSize = in.tellg();

                size_t offset = 0;
                size_t length = fileSize;

                if (args.size() >= 1) {
                    auto maybeOffset = ParseHex<size_t>(args[0]);
                    if (!maybeOffset) {
                        fmt::println("Invalid offset: {}", args[0]);
                        return 1;
                    }
                    offset = *maybeOffset;
                }
                if (args.size() >= 2) {
                    auto maybeLength = ParseHex<size_t>(args[1]);
                    if (!maybeLength) {
                        fmt::println("Invalid length: {}", args[1]);
                        return 1;
                    }
                    length = *maybeLength;
                }

                length = std::min(length, fileSize - offset);

                in.seekg(offset, std::ios::beg);

                uint32 opcode{};

                auto readOpcode = [&] {
                    uint8 b[4] = {0};
                    in.read((char *)b, 4);
                    opcode = (static_cast<uint32>(b[0]) << 24u) | (static_cast<uint32>(b[1]) << 16u) |
                             (static_cast<uint32>(b[2]) << 8u) | static_cast<uint32>(b[3]);
                    return in.good();
                };

                while (readOpcode()) {
                    const scu::SCUDSPInstruction &instr = scu::Disassemble(opcode);

                    printAddress(address);
                    printOpcode(opcode);
                    printInstruction(instr);

                    printReset();
                    x = 0;

                    ++address;
                }
            }
        } else if (lcisa == "scspdspraw" || lcisa == "scspdsp") {
            const bool rawDisasm = lcisa == "scspdspraw";
            auto maybeAddress = ParseHex<uint8>(origin);
            if (!maybeAddress) {
                fmt::println("Invalid origin address: {}", origin);
                return 1;
            }
            uint8 address = *maybeAddress;
            if (address >= 128) {
                fmt::println("Invalid origin address: {}", origin);
                return 1;
            }

            auto printAddress = [&](uint8 address) {
                if (!hideAddresses) {
                    print(colors.address, fmt::format("{:02X}  ", address), false);
                }
            };

            auto printOpcode = [&](uint64 opcode) {
                if (!hideOpcodes) {
                    print(colors.bytes, fmt::format("{:016X}  ", opcode), false);
                }
            };

            auto printImm = [&](uint64 imm) { print(colors.immediate, fmt::format("0x{:X}", imm)); };
            auto printImmDec = [&](uint64 imm) { print(colors.immediate, fmt::format("{:d}", imm)); };

            auto printBitRange = [&](uint32 high, uint32 low) {
                printOperator("[");
                printImmDec(high);
                printOperator(":");
                printImmDec(low);
                printOperator("]");
            };

            auto printInstruction = [&](scsp::DSPInstr instr) {
                if (instr.u64 == 0) {
                    printNOP("NOP");
                    return;
                }

                if (rawDisasm) {
                    printMnemonic("IRA");
                    printOperator("=");
                    if (instr.IRA <= 0x1F) {
                        printOpRead("MEMS");
                        printOperator("[");
                        printImm(instr.IRA);
                        printOperator("]");
                    } else if (instr.IRA <= 0x2F) {
                        printOpRead("MIXS");
                        printOperator("[");
                        printImm(instr.IRA & 0xF);
                        printOperator("]");
                    } else if (instr.IRA <= 0x31) {
                        printOpRead("EXTS");
                        printOperator("[");
                        printImm(instr.IRA & 0x1);
                        printOperator("]");
                    } else {
                        printIllegalMnemonic();
                    }

                    if (instr.IWT) {
                        align(15);
                        printMnemonic("IWA");
                        printOperator("=");
                        printImm(instr.IWA);
                    }

                    align(24);
                    printMnemonic("TRA");
                    printOperator("=");
                    printImm(instr.TRA);

                    if (instr.TWT) {
                        align(33);
                        printMnemonic("TWA");
                        printOperator("=");
                        printImm(instr.TWA);
                    }

                    align(42);
                    printMnemonic("XSEL");
                    printOperator("=");
                    switch (instr.XSEL) {
                    case 0: printOpRead("TEMP"); break;
                    case 1: printOpRead("INPUTS"); break;
                    }

                    align(54);
                    printMnemonic("YSEL");
                    printOperator("=");
                    switch (instr.YSEL) {
                    case 0: printOpRead("FRC"); break;
                    case 1:
                        printOpRead("COEF");
                        printOperator("[");
                        printImm(instr.CRA);
                        printOperator("]");
                        break;
                    case 2:
                        printOpRead("Y");
                        printBitRange(23, 11);
                        break;
                    case 3:
                        printOpRead("Y");
                        printBitRange(15, 4);
                        break;
                    }

                    if (instr.YRL) {
                        align(70);
                        printMnemonic("YRL");
                    }

                    if (instr.FRCL) {
                        align(74);
                        printMnemonic("FRCL");
                    }

                    align(79);
                    if (instr.ZERO) {
                        printMnemonic("ZERO");
                    } else {
                        if (instr.NEGB) {
                            printMnemonic("NEGB");
                            if (instr.BSEL) {
                                printOperator(" ");
                            }
                        }
                        if (instr.BSEL) {
                            printMnemonic("BSEL");
                        }
                    }

                    if (instr.SHFT0) {
                        align(89);
                        printMnemonic("SHFT0");
                    }
                    if (instr.SHFT1) {
                        align(95);
                        printMnemonic("SHFT1");
                    }

                    if (instr.EWT) {
                        align(101);
                        printMnemonic("EWA");
                        printOperator("=");
                        printImm(instr.EWA);
                    }

                    if (instr.MRD || instr.MWT) {
                        align(109);
                        printMnemonic("MASA");
                        printOperator("=");
                        printImm(instr.MASA);
                        if (instr.MRD) {
                            align(119);
                            printMnemonic("MRD");
                        }
                        if (instr.MWT) {
                            align(123);
                            printMnemonic("MWT");
                        }
                        if (instr.NXADR) {
                            align(127);
                            printMnemonic("NXADR");
                        }
                        if (instr.ADREB) {
                            align(133);
                            printMnemonic("ADREB");
                        }
                        if (instr.NOFL) {
                            align(139);
                            printMnemonic("NOFL");
                        }
                        if (instr.TABLE) {
                            align(144);
                            printMnemonic("TABLE");
                        }
                    }

                    if (instr.ADRL) {
                        align(150);
                        printMnemonic("ADRL");
                    }
                } else {
                    printOpWrite("INPUTS");
                    printOperator("<-");
                    if (instr.IRA <= 0x1F) {
                        printOpRead("MEMS");
                        printOperator("[");
                        printImm(instr.IRA);
                        printOperator("]");
                    } else if (instr.IRA <= 0x2F) {
                        printOpRead("MIXS");
                        printOperator("[");
                        printImm(instr.IRA & 0xF);
                        printOperator("]");
                    } else if (instr.IRA <= 0x31) {
                        printOpRead("EXTS");
                        printOperator("[");
                        printImm(instr.IRA & 0x1);
                        printOperator("]");
                    } else {
                        printIllegalMnemonic();
                    }

                    align(20);
                    printOpWrite("TMP");
                    printOperator("<-");
                    printOpRead("TEMP");
                    printOperator("[");
                    printImm(instr.TRA);
                    printOperator("+");
                    printOpRead("MDEC_CT");
                    printOperator("]");

                    align(45);
                    printOpWrite("SFT");
                    printOperator("<-");
                    switch (instr.XSEL) {
                    case 0: printOpRead("TMP"); break;
                    case 1: printOpRead("INPUTS"); break;
                    }
                    // align(56);
                    printOperator("*");
                    switch (instr.YSEL) {
                    case 0: printOpRead("FRC"); break;
                    case 1:
                        printOpRead("COEF");
                        printOperator("[");
                        printImm(instr.CRA);
                        printOperator("]");
                        break;
                    case 2:
                        printOpRead("Y");
                        printBitRange(23, 11);
                        break;
                    case 3:
                        printOpRead("Y");
                        printBitRange(15, 4);
                        break;
                    }
                    if (!instr.ZERO) {
                        // align(67);
                        if (instr.NEGB) {
                            printOperator("-");
                        } else {
                            printOperator("+");
                        }
                        if (instr.BSEL) {
                            printOpRead("SFT");
                        } else {
                            printOpRead("TMP");
                        }
                    }
                    if (instr.SHFT0 ^ instr.SHFT1) {
                        // align(71);
                        printOperator("<<");
                        printImmDec(1);
                    }

                    if (instr.YRL) {
                        align(76);
                        printOpWrite("Y");
                        printOperator("<-");
                        printOpRead("INPUTS");
                    }

                    if (instr.FRCL) {
                        align(87);
                        printOpWrite("FRC");
                        printOperator("<-");
                        printOpRead("SFT");
                        if (instr.SHFT0 & instr.SHFT1) {
                            printBitRange(11, 0);
                        } else {
                            printBitRange(23, 11);
                        }
                    }

                    align(104);
                    if (instr.EWT) {
                        printOpWrite("EFREG");
                        printOperator("[");
                        printImm(instr.EWA);
                        printOperator("]");
                        printOperator("<-");
                        printOpRead("SFT");
                        printBitRange(23, 8);
                    }

                    align(127);
                    if (instr.TWT) {
                        printOpWrite("TEMP");
                        printOperator("[");
                        printImm(instr.TWA);
                        printOperator("+");
                        printOpRead("MDEC_CT");
                        printOperator("]");
                        printOperator("<-");
                        printOpRead("SFT");
                    }

                    align(152);
                    if (instr.IWT) {
                        printOpWrite("MEMS");
                        printOperator("[");
                        printImm(instr.IWA);
                        printOperator("]");
                        printOperator("<-");
                        printOpRead("MEM");
                    }

                    align(169);
                    if (instr.MRD || instr.MWT) {
                        printOpWrite("MADR");
                        printOperator("<-");
                        printOperator("(");
                        if (!instr.TABLE && (instr.ADREB || instr.NXADR)) {
                            printOperator("(");
                        }
                        printOpRead("MADRS");
                        printOperator("[");
                        printImm(instr.MASA);
                        printOperator("]");
                        if (instr.ADREB) {
                            printOperator("+");
                            printOpRead("ADRS_REG");
                        }
                        if (instr.NXADR) {
                            printOperator("+");
                            printImmDec(1);
                        }
                        if (!instr.TABLE) {
                            if (instr.ADREB || instr.NXADR) {
                                printOperator(")");
                            }
                            printOperator("&");
                            printOpRead("RBL");
                        }
                        printOperator(")");
                        printOperator("+");
                        printOpRead("RBP");
                    }

                    if (instr.MRD) {
                        align(211);
                        printOpWrite("MEM");
                        printOperator("<-");
                        printOpRead("WRAM");
                        printOperator("[");
                        printOpRead("MADR");
                        printOperator("]");
                    }

                    if (instr.MWT) {
                        align(228);
                        printOpWrite("WRAM");
                        printOperator("[");
                        printOpRead("MADR");
                        printOperator("]");
                        printOperator("<-");
                        printOpRead("MEM");
                    }

                    if ((instr.MRD || instr.MWT) && instr.NOFL) {
                        align(245);
                        printMnemonic("NOFL");
                    }

                    if (instr.ADRL) {
                        align(251);
                        printOpWrite("ADRS_REG");
                        printOperator("<-");
                        if (instr.SHFT0 & instr.SHFT1) {
                            printOpRead("INPUTS");
                            printBitRange(27, 16);
                        } else {
                            printOpRead("SFT");
                            printBitRange(23, 12);
                        }
                    }
                }
            };

            if (inputFile.empty()) {
                for (auto &opcodeStr : args) {
                    auto maybeOpcode = ParseHex<uint64>(opcodeStr);
                    if (!maybeOpcode) {
                        fmt::println("Invalid opcode: {}", opcodeStr);
                        return 1;
                    }
                    const uint64 opcode = *maybeOpcode;

                    const scsp::DSPInstr instr{.u64 = opcode};

                    printAddress(address);
                    printOpcode(opcode);
                    printInstruction(instr);

                    printReset();
                    x = 0;

                    ++address;
                }
            } else {
                std::ifstream in{inputFile, std::ios::binary};
                if (!in) {
                    std::error_code error{errno, std::generic_category()};
                    fmt::println("Could not open file: {}", error.message());
                    return 1;
                }

                in.seekg(0, std::ios::end);
                const size_t fileSize = in.tellg();

                size_t offset = 0;
                size_t length = fileSize;

                if (args.size() >= 1) {
                    auto maybeOffset = ParseHex<size_t>(args[0]);
                    if (!maybeOffset) {
                        fmt::println("Invalid offset: {}", args[0]);
                        return 1;
                    }
                    offset = *maybeOffset;
                }
                if (args.size() >= 2) {
                    auto maybeLength = ParseHex<size_t>(args[1]);
                    if (!maybeLength) {
                        fmt::println("Invalid length: {}", args[1]);
                        return 1;
                    }
                    length = *maybeLength;
                }

                length = std::min(length, fileSize - offset);

                in.seekg(offset, std::ios::beg);

                uint64 opcode{};

                auto readOpcode = [&] {
                    uint8 b[8] = {0};
                    in.read((char *)b, 8);
                    opcode = (static_cast<uint64>(b[0]) << 56ull) | (static_cast<uint64>(b[1]) << 48ull) |
                             (static_cast<uint64>(b[2]) << 40ull) | (static_cast<uint64>(b[3]) << 32ull) |
                             (static_cast<uint64>(b[4]) << 24ull) | (static_cast<uint64>(b[5]) << 16ull) |
                             (static_cast<uint64>(b[6]) << 8ull) | static_cast<uint64>(b[7]);
                    return in.good();
                };

                while (readOpcode()) {
                    const scsp::DSPInstr instr{.u64 = opcode};

                    printAddress(address);
                    printOpcode(opcode);
                    printInstruction(instr);

                    printReset();
                    x = 0;

                    ++address;
                }
            }
        } else {
            fmt::println("Invalid ISA: {}", isa);
            fmt::println("");
            printHelp();
            return 1;
        }
    } catch (const cxxopts::exceptions::exception &e) {
        fmt::println("Failed to parse arguments: {}", e.what());
        return -1;
    } catch (const std::system_error &e) {
        fmt::println("System error: {}", e.what());
        return e.code().value();
    } catch (const std::exception &e) {
        fmt::println("Unhandled exception: {}", e.what());
        return -1;
    }
}
