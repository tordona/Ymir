#include <ymir/core/types.hpp>

#include <ymir/hw/scsp/scsp_dsp_instr.hpp>

#include <ymir/util/bit_ops.hpp>

#include "colors.hpp"
#include "disassembler_m68k.hpp"
#include "disassembler_scspdsp.hpp"
#include "disassembler_scudsp.hpp"
#include "disassembler_sh2.hpp"
#include "fetcher.hpp"
#include "utils.hpp"

#include <cxxopts.hpp>
#include <fmt/format.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

using namespace ymir;

// TODO: reuse code where possible

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
        fmt::println("  Both parameters are specified in hexadecimal.");
        fmt::println("  <length> is truncated down to the nearest multiple of the opcode size.");
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

        Disassembler disasm{};
        disasm.hideAddresses = hideAddresses;
        disasm.hideOpcodes = hideOpcodes;

        // Color mode must be one of the valid modes
        if (result.contains("color")) {
            std::string lcColorMode = colorMode;
            std::transform(lcColorMode.cbegin(), lcColorMode.cend(), lcColorMode.begin(),
                           [](char c) { return std::tolower(c); });
            if (lcColorMode == "none") {
                disasm.colors = kNoColors;
            } else if (lcColorMode == "basic") {
                disasm.colors = kBasicColors;
            } else if (lcColorMode == "truecolor") {
                disasm.colors = kTrueColors;
            } else {
                fmt::println("Invalid color mode: {}", colorMode);
                fmt::println("");
                printHelp();
                return 1;
            }
        }

        // Disassemble code
        std::string lcisa = isa;
        std::transform(lcisa.cbegin(), lcisa.cend(), lcisa.begin(), [](char c) { return std::tolower(c); });
        if (lcisa == "sh2" || lcisa == "sh-2") {
            auto maybeAddress = ParseHex<uint32>(origin);
            if (!maybeAddress) {
                fmt::println("Invalid origin address: {}", origin);
                return 1;
            }

            SH2Disassembler sh2Disasm{disasm};
            sh2Disasm.address = *maybeAddress;

            struct SH2Opcode {
                uint16 opcode;
                int forceDelaySlot = 0;
            };

            struct SH2CommandLineOpcodeParser {
                static OpcodeFetchResult<SH2Opcode> Parse(std::string_view arg) {
                    std::string_view strippedOpcode = arg;

                    // 0 = no change; +1 = force delay slot, -1 = force non-delay slot
                    int forceDelaySlot = 0;
                    if (arg.starts_with('_')) {
                        forceDelaySlot = +1;
                        strippedOpcode = strippedOpcode.substr(1);
                    } else if (arg.starts_with('!')) {
                        forceDelaySlot = -1;
                        strippedOpcode = strippedOpcode.substr(1);
                    }

                    auto maybeOpcode = ParseHex<uint16>(strippedOpcode);
                    if (!maybeOpcode) {
                        return OpcodeFetchError{fmt::format("Invalid opcode: {}", arg)};
                    }

                    const uint16 opcode = *maybeOpcode;
                    return SH2Opcode{opcode, forceDelaySlot};
                }
            };

            struct SH2StreamOpcodeParser {
                static SH2Opcode Parse(std::istream &input) {
                    uint16 opcode{};
                    input.read((char *)&opcode, sizeof(uint16));
                    opcode = bit::big_endian_swap(opcode);
                    return SH2Opcode{opcode, 0};
                }
            };

            using SH2OpcodeFetcher = IOpcodeFetcher<SH2Opcode>;
            using CommandLineSH2OpcodeFetcher = CommandLineOpcodeFetcher<SH2Opcode, SH2CommandLineOpcodeParser>;
            using StreamSH2OpcodeFetcher = StreamOpcodeFetcher<SH2Opcode, SH2StreamOpcodeParser>;

            auto fetcher =
                MakeFetcher<SH2OpcodeFetcher, CommandLineSH2OpcodeFetcher, StreamSH2OpcodeFetcher>(args, inputFile);

            bool running = true;
            const auto visitor = overloads{
                [&](SH2Opcode &opcode) { sh2Disasm.Disassemble(opcode.opcode, opcode.forceDelaySlot); },
                [&](OpcodeFetchError &error) {
                    fmt::println("{}", error.message);
                    running = false;
                },
                [&](OpcodeFetchEnd) { running = false; },
            };

            while (running) {
                auto result = fetcher->Fetch();
                std::visit(visitor, result);
            }
        } else if (lcisa == "m68k" || lcisa == "m68000") {
            auto maybeAddress = ParseHex<uint32>(origin);
            if (!maybeAddress) {
                fmt::println("Invalid origin address: {}", origin);
                return 1;
            }

            M68KDisassembler m68kDisasm{disasm};
            m68kDisasm.address = *maybeAddress;

            using M68KOpcode = uint16;
            using M68KOpcodeFetcher = IOpcodeFetcher<M68KOpcode>;
            using CommandLineM68KOpcodeFetcher = CommandLineOpcodeFetcher<M68KOpcode>;
            using StreamM68KOpcodeFetcher = StreamOpcodeFetcher<M68KOpcode>;

            auto fetcher =
                MakeFetcher<M68KOpcodeFetcher, CommandLineM68KOpcodeFetcher, StreamM68KOpcodeFetcher>(args, inputFile);

            while (m68kDisasm.valid) {
                m68kDisasm.Disassemble([&]() -> uint16 {
                    if (!m68kDisasm.valid) {
                        return 0;
                    }

                    auto result = fetcher->Fetch();
                    if (std::holds_alternative<M68KOpcode>(result)) {
                        return std::get<M68KOpcode>(result);
                    } else {
                        if (std::holds_alternative<OpcodeFetchError>(result)) {
                            auto &error = std::get<OpcodeFetchError>(result);
                            fmt::println("{}", error.message);
                        }
                        m68kDisasm.valid = false;
                        return 0;
                    }
                });
            }
        } else if (lcisa == "scudsp") {
            auto maybeAddress = ParseHex<uint8>(origin);
            if (!maybeAddress) {
                fmt::println("Invalid origin address: {}", origin);
                return 1;
            }

            SCUDSPDisassembler scuDspDisasm{disasm};
            scuDspDisasm.address = *maybeAddress;

            using SCUDSPOpcode = uint32;
            using SCUDSPOpcodeFetcher = IOpcodeFetcher<SCUDSPOpcode>;
            using CommandLineSCUDSPOpcodeFetcher = CommandLineOpcodeFetcher<SCUDSPOpcode>;
            using StreamSCUDSPOpcodeFetcher = StreamOpcodeFetcher<SCUDSPOpcode>;

            auto fetcher = MakeFetcher<SCUDSPOpcodeFetcher, CommandLineSCUDSPOpcodeFetcher, StreamSCUDSPOpcodeFetcher>(
                args, inputFile);

            bool running = true;
            const auto visitor = overloads{
                [&](SCUDSPOpcode opcode) { scuDspDisasm.Disassemble(opcode); },
                [&](OpcodeFetchError &error) {
                    fmt::println("{}", error.message);
                    running = false;
                },
                [&](OpcodeFetchEnd) { running = false; },
            };

            while (running) {
                auto result = fetcher->Fetch();
                std::visit(visitor, result);
            }
        } else if (lcisa == "scspdspraw" || lcisa == "scspdsp") {
            const bool rawDisasm = lcisa == "scspdspraw";
            auto maybeAddress = ParseHex<uint8>(origin);
            if (!maybeAddress) {
                fmt::println("Invalid origin address: {}", origin);
                return 1;
            }

            SCSPDSPDisassembler scspDspDisasm{disasm};
            scspDspDisasm.address = *maybeAddress;
            if (scspDspDisasm.address >= 128) {
                fmt::println("Invalid origin address: {}", origin);
                return 1;
            }

            using SCSPDSPOpcode = uint64;
            using SCSPDSPOpcodeFetcher = IOpcodeFetcher<SCSPDSPOpcode>;
            using CommandLineSCSPDSPOpcodeFetcher = CommandLineOpcodeFetcher<SCSPDSPOpcode>;
            using StreamSCSPDSPOpcodeFetcher = StreamOpcodeFetcher<SCSPDSPOpcode>;

            auto fetcher =
                MakeFetcher<SCSPDSPOpcodeFetcher, CommandLineSCSPDSPOpcodeFetcher, StreamSCSPDSPOpcodeFetcher>(
                    args, inputFile);

            bool running = true;
            const auto visitor = overloads{
                [&](SCSPDSPOpcode &opcode) { scspDspDisasm.Disassemble(opcode, rawDisasm); },
                [&](OpcodeFetchError &error) {
                    fmt::println("{}", error.message);
                    running = false;
                },
                [&](OpcodeFetchEnd) { running = false; },
            };

            while (running) {
                auto result = fetcher->Fetch();
                std::visit(visitor, result);
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
