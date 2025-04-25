#include <ymir/core/types.hpp>

#include <ymir/hw/sh2/sh2_disasm.hpp>

#include <cxxopts.hpp>
#include <fmt/format.h>

#include <algorithm>
#include <concepts>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#define ANSI_ESCAPE "\x1b"

#define ANSI_RESET ANSI_ESCAPE "[0m"

#define ANSI_FGCOLOR_BLACK ANSI_ESCAPE "[30m"
#define ANSI_FGCOLOR_RED ANSI_ESCAPE "[31m"
#define ANSI_FGCOLOR_GREEN ANSI_ESCAPE "[32m"
#define ANSI_FGCOLOR_YELLOW ANSI_ESCAPE "[33m"
#define ANSI_FGCOLOR_BLUE ANSI_ESCAPE "[34m"
#define ANSI_FGCOLOR_MAGENTA ANSI_ESCAPE "[35m"
#define ANSI_FGCOLOR_CYAN ANSI_ESCAPE "[36m"
#define ANSI_FGCOLOR_WHITE ANSI_ESCAPE "[37m"

#define ANSI_FGCOLOR_BRIGHT_BLACK ANSI_ESCAPE "[1;30m"
#define ANSI_FGCOLOR_BRIGHT_RED ANSI_ESCAPE "[1;31m"
#define ANSI_FGCOLOR_BRIGHT_GREEN ANSI_ESCAPE "[1;32m"
#define ANSI_FGCOLOR_BRIGHT_YELLOW ANSI_ESCAPE "[1;33m"
#define ANSI_FGCOLOR_BRIGHT_BLUE ANSI_ESCAPE "[1;34m"
#define ANSI_FGCOLOR_BRIGHT_MAGENTA ANSI_ESCAPE "[1;35m"
#define ANSI_FGCOLOR_BRIGHT_CYAN ANSI_ESCAPE "[1;36m"
#define ANSI_FGCOLOR_BRIGHT_WHITE ANSI_ESCAPE "[1;37m"

#define ANSI_FGCOLOR_24B(r, g, b) ANSI_ESCAPE "[38;2;" #r ";" #g ";" #b "m"

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

    const char *separator; // Operand (,) and size suffix (.) separators

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

    .separator = "",

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

    .separator = ANSI_FGCOLOR_BRIGHT_BLACK,

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
    .sizeSuffix = ANSI_FGCOLOR_24B(128, 145, 194),
    .cond = ANSI_FGCOLOR_24B(138, 128, 194),

    .immediate = ANSI_FGCOLOR_24B(221, 247, 173),
    .opRead = ANSI_FGCOLOR_24B(173, 247, 206),
    .opWrite = ANSI_FGCOLOR_24B(215, 173, 247),
    .opReadWrite = ANSI_FGCOLOR_24B(247, 206, 173),

    .separator = ANSI_FGCOLOR_24B(186, 191, 194),

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

        // TODO: read program according to specified ISA
        // - SH2: 16-bit fixed-length opcodes
        // - M68K: 16-bit variable-length opcodes
        // - SCUDSP: 32-bit VLIW opcodes
        // - SCSPDSP: 64-bit VLIW opcodes
        // If reading from command line, SH2 can also override the delay slot flag with opcode prefixes ! and _
        //
        // Design ideas for opcode parsing:
        // - v1: each ISA (re)implements opcode reading from a std::istream or from the args vector

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
                    fmt::print("{}{:08X}  ", colors.address, address);
                }
            };

            auto printOpcode = [&](uint16 opcode) {
                if (!hideOpcodes) {
                    fmt::print("{}{:04X}  ", colors.bytes, opcode);
                }
            };

            auto printDelaySlotPrefix = [&] { fmt::print("{}> ", colors.delaySlot); };

            auto printMnemonic = [&](const char *mnemonic) { fmt::print("{}{}", colors.mnemonic, mnemonic); };
            auto printIllegalMnemonic = [&] { fmt::print("{}{}", colors.illegalMnemonic, "(illegal)"); };
            auto printUnknownMnemonic = [&] { fmt::print("{}{}", colors.illegalMnemonic, "(?)"); };
            auto printCond = [&](const char *cond) { fmt::print("{}{}", colors.cond, cond); };
            auto printSeparator = [&](const char *separator) { fmt::print("{}{}", colors.separator, separator); };
            auto printSize = [&](const char *size) {
                printSeparator(".");
                fmt::print("{}{}", colors.sizeSuffix, size);
            };

            auto printInstruction = [&](const ymir::sh2::OpcodeDisasm &disasm, bool delaySlot) {
                if (delaySlot) {
                    if (!disasm.validInDelaySlot) {
                        printIllegalMnemonic();
                        return;
                    }
                    printDelaySlotPrefix();
                }

                using namespace ymir;

                switch (disasm.mnemonic) {
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
                    printSeparator("/");
                    printCond("eq");
                    break;
                case sh2::Mnemonic::CMP_GE:
                    printMnemonic("cmp");
                    printSeparator("/");
                    printCond("ge");
                    break;
                case sh2::Mnemonic::CMP_GT:
                    printMnemonic("cmp");
                    printSeparator("/");
                    printCond("gt");
                    break;
                case sh2::Mnemonic::CMP_HI:
                    printMnemonic("cmp");
                    printSeparator("/");
                    printCond("hi");
                    break;
                case sh2::Mnemonic::CMP_HS:
                    printMnemonic("cmp");
                    printSeparator("/");
                    printCond("hs");
                    break;
                case sh2::Mnemonic::CMP_PL:
                    printMnemonic("cmp");
                    printSeparator("/");
                    printCond("pl");
                    break;
                case sh2::Mnemonic::CMP_PZ:
                    printMnemonic("cmp");
                    printSeparator("/");
                    printCond("pz");
                    break;
                case sh2::Mnemonic::CMP_STR:
                    printMnemonic("cmp");
                    printSeparator("/");
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
                    printSeparator("/");
                    printMnemonic("s");
                    break;
                case sh2::Mnemonic::BT:
                    printMnemonic("b");
                    printCond("t");
                    break;
                case sh2::Mnemonic::BTS:
                    printMnemonic("b");
                    printCond("t");
                    printSeparator("/");
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

                switch (disasm.opSize) {
                case sh2::OperandSize::Byte: printSize("b"); break;
                case sh2::OperandSize::Word: printSize("w"); break;
                case sh2::OperandSize::Long: printSize("l"); break;
                default: break;
                }
            };

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

                    const ymir::sh2::OpcodeDisasm &disasm = ymir::sh2::Disassemble(opcode);

                    bool delaySlotState = (isDelaySlot || forceDelaySlot > 0) && forceDelaySlot >= 0;

                    printAddress(address);
                    printOpcode(opcode);
                    printInstruction(disasm, delaySlotState);

                    if (forceDelaySlot > 0) {
                        fmt::print("{}; delay slot override", colors.comment);
                    } else if (forceDelaySlot < 0) {
                        fmt::print("{}; non-delay slot override", colors.comment);
                    }

                    fmt::println("{}", colors.reset);

                    isDelaySlot = disasm.hasDelaySlot;
                    address += sizeof(uint16);
                }
            } else {
                // TODO: disassemble from inputFile
                // - parse optional args: [<offset> [<length>]]
            }
        } else if (lcisa == "m68k" || lcisa == "m68000") {
            // TODO: disassemble M68000
        } else if (lcisa == "scudsp") {
            // TODO: disassemble SCU DSP
        } else if (lcisa == "scspdsp") {
            // TODO: disassemble SCSP DSP
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
