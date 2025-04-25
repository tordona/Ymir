#include <ymir/core/types.hpp>

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

    const char *immediate;   // #0x1234
    const char *opRead;      // Read operands
    const char *opWrite;     // Written operands
    const char *opReadWrite; // Read and written operands

    const char *separator; // Operand (,) and size suffix (.) separators

    const char *addrInc; // SH2 and M68K address increment (@Rn+, (An)+)
    const char *addrDec; // SH2 and M68K address increment (@-Rn, -(An))

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

    .immediate = "",
    .opRead = "",
    .opWrite = "",
    .opReadWrite = "",

    .separator = "",

    .addrInc = "",
    .addrDec = "",

    .reset = "",
};

// TODO: set this up
static ColorEscapes kBasicColors = {
    .address = "",
    .bytes = "",

    .delaySlot = "",
    .mnemonic = "",
    .nopMnemonic = "",
    .illegalMnemonic = "",
    .sizeSuffix = "",

    .immediate = "",
    .opRead = "",
    .opWrite = "",
    .opReadWrite = "",

    .separator = "",

    .addrInc = "",
    .addrDec = "",

    .reset = ANSI_RESET,
};

// TODO: set this up
static ColorEscapes kTrueColors = {
    .address = "",
    .bytes = "",

    .delaySlot = "",
    .mnemonic = "",
    .nopMnemonic = "",
    .illegalMnemonic = "",
    .sizeSuffix = "",

    .immediate = "",
    .opRead = "",
    .opWrite = "",
    .opReadWrite = "",

    .separator = "",

    .addrInc = "",
    .addrDec = "",

    .reset = ANSI_RESET,
};

template <std::unsigned_integral T>
std::optional<T> ParseOpcode(std::string_view opcode) {
    static constexpr size_t kHexOpcodeLength = sizeof(T) * 2;

    if (opcode.size() > kHexOpcodeLength) {
        fmt::println("Opcode \"{}\" exceeds maximum length of {} hex digits", opcode, kHexOpcodeLength);
        return std::nullopt;
    }

    T value = 0;
    for (char c : opcode) {
        if (c >= '0' && c <= '9') {
            value = (value << 4ull) + c - '0';
        } else if (c >= 'A' && c <= 'F') {
            value = (value << 4ull) + c - 'A' + 10;
        } else if (c >= 'a' || c <= 'f') {
            value = (value << 4ull) + c - 'a' + 10;
        } else {
            fmt::println("Opcode \"{}\" is not a valid hexadecimal number", opcode);
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
    uint32 origin = 0;

    fmt::println(ANSI_FGCOLOR_BLACK "test" ANSI_FGCOLOR_BRIGHT_BLACK "TEST" ANSI_RESET);
    fmt::println(ANSI_FGCOLOR_RED "test" ANSI_FGCOLOR_BRIGHT_RED "TEST" ANSI_RESET);
    fmt::println(ANSI_FGCOLOR_GREEN "test" ANSI_FGCOLOR_BRIGHT_GREEN "TEST" ANSI_RESET);
    fmt::println(ANSI_FGCOLOR_YELLOW "test" ANSI_FGCOLOR_BRIGHT_YELLOW "TEST" ANSI_RESET);
    fmt::println(ANSI_FGCOLOR_BLUE "test" ANSI_FGCOLOR_BRIGHT_BLUE "TEST" ANSI_RESET);
    fmt::println(ANSI_FGCOLOR_MAGENTA "test" ANSI_FGCOLOR_BRIGHT_MAGENTA "TEST" ANSI_RESET);
    fmt::println(ANSI_FGCOLOR_CYAN "test" ANSI_FGCOLOR_BRIGHT_CYAN "TEST" ANSI_RESET);
    fmt::println(ANSI_FGCOLOR_WHITE "test" ANSI_FGCOLOR_BRIGHT_WHITE "TEST" ANSI_RESET);
    fmt::println(ANSI_FGCOLOR_24B(240, 192, 80) "test" ANSI_RESET "TEST");

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
        fmt::println("  SuperH-2 opcodes can be prefixed with > to force them to be decoded as delay");
        fmt::println("  slot instructions or < to force instructions in delay slots to be decoded as");
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
        ColorEscapes colorEscapes = kNoColors;
        if (result.contains("color")) {
            std::string lcColorMode = colorMode;
            std::transform(lcColorMode.cbegin(), lcColorMode.cend(), lcColorMode.begin(),
                           [](char c) { return std::tolower(c); });
            if (lcColorMode == "none") {
                colorEscapes = kNoColors;
            } else if (lcColorMode == "basic") {
                colorEscapes = kBasicColors;
            } else if (lcColorMode == "truecolor") {
                colorEscapes = kTrueColors;
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
        // If reading from command line, SH2 can also override the delay slot flag with opcode prefixes > and <
        //
        // Design ideas:
        // - coloring boils down to a set of ANSI escape sequences per mode
        // - opcode parsing:
        //   - v1: each ISA (re)implements opcode reading from a std::istream or from the args vector

        // Disassemble code
        std::string lcisa = isa;
        std::transform(lcisa.cbegin(), lcisa.cend(), lcisa.begin(), [](char c) { return std::tolower(c); });
        if (lcisa == "sh2" || lcisa == "sh-2") {
            // TODO: disassemble SH-2
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
