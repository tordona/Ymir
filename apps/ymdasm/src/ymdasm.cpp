#include "cmd_opts.hpp"

#include <cxxopts.hpp>
#include <fmt/format.h>

int main(int argc, char *argv[]) {
    bool showHelp = false;

    YmDasmOptions opts{};
    cxxopts::Options options("ymdasm", "Ymir disassembly tool");
    options.add_options()                                                                               //
        ("i,isa", "Instruction set architecture: sh2, m68k, scudsp, scspdsp", cxxopts::value(opts.isa)) //
        ("h,help", "Display help text", cxxopts::value(showHelp)->default_value("false"));
    options.parse_positional({"isa"});

    try {
        options.parse(argc, argv);
        fmt::println("ymdasm " Ymir_VERSION);
        fmt::println("{}", options.help());
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
