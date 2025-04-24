#include "processors.hpp"

#include <fmt/format.h>

#include <memory>
#include <optional>
#include <string_view>

int main(int argc, char *argv[]) {
    fmt::println("ymdasm " Ymir_VERSION);

    std::unique_ptr<CommandProcessor> processor = std::make_unique<InitialCommandProcessor>();

    auto readArg = [&, i = 1]() mutable -> std::optional<std::string_view> {
        if (i < argc) {
            return argv[i++];
        } else {
            return std::nullopt;
        }
    };

    auto printHelp = [&] {
        for (const OptionInfo *info : processor->GetOptions()) {
            if (info->argument) {
                fmt::println("-{}, --{} <{}>  {}", info->shortName, info->longName, info->argument, info->description);
            } else {
                fmt::println("-{}, --{}  {}", info->shortName, info->longName, info->description);
            }
        }
    };

    while (auto arg = readArg()) {
        if (arg->starts_with("--")) {
            // Long options:
            // --name=value
            // --name value
            // --name
            // --name-

            const auto equalsPos = arg->find_first_of('=');
            if (equalsPos != std::string::npos) {
                // --name=value
                std::string_view name = arg->substr(2, equalsPos);

                const OptionInfo *info = processor->LongOptionInfo(name);
                if (info == nullptr) {
                    fmt::println("Invalid option: --{}", name);
                    printHelp();
                    return 1;
                } else if (info->argument == nullptr) {
                    fmt::println("Option --{} does not take an argument", name);
                    printHelp();
                    return 1;
                } else {
                    std::string_view value = arg->substr(equalsPos + 1);
                    processor->LongOption(name, value);
                }
            } else {
                std::string_view name = arg->substr(2);
                bool flagValue = true;
                if (name.ends_with('-')) {
                    // "flag-" -> disable "flag"
                    name = name.substr(2, name.size() - 2);
                    flagValue = false;
                }

                const OptionInfo *info = processor->LongOptionInfo(name);
                if (info == nullptr) {
                    fmt::println("Invalid option: --{}", name);
                    printHelp();
                    return 1;
                } else if (info->argument != nullptr) {
                    if (!flagValue) {
                        fmt::println("Option --{} is not a flag", info->argument, name);
                        printHelp();
                        return 1;
                    } else if (auto value = readArg()) {
                        // --name value
                        processor->LongOption(name, *value);
                    } else {
                        fmt::println("Missing argument <{}> for option --{}", info->argument, name);
                        printHelp();
                        return 1;
                    }
                } else {
                    // --flag or --flag-
                    processor->LongFlag(name, flagValue);
                }
            }
        } else if (arg->starts_with("-")) {
            // Short options sequence
            // -a
            // -b 2
            // -c-
            // -d 4
            // -ab 2
            // -bd 2 4
            // -dcba 4 2
            // etc.

            auto readShortOpt = [&, i = 1]() mutable -> std::optional<std::string_view> {
                if (i < arg->length()) {
                    char opt = (*arg)[i];
                    if (i + 1 < arg->length()) {
                        char possibleFlagDisable = (*arg)[i + 1];
                        if (possibleFlagDisable == '-') {
                            auto ret = arg->substr(i, i + 2);
                            i += 2;
                            return ret;
                        }
                    }
                    auto ret = arg->substr(i, i + 1);
                    ++i;
                    return ret;
                } else {
                    return std::nullopt;
                }
            };

            while (auto shortOpt = readShortOpt()) {
                char name = (*shortOpt)[0];
                const OptionInfo *info = processor->ShortOptionInfo(name);
                if (info == nullptr) {
                    fmt::println("Invalid option: -{}", name);
                    printHelp();
                    return 1;
                } else if (shortOpt->ends_with('-')) {
                    // -x-
                    processor->ShortFlag(name, false);
                } else if (info->argument != nullptr) {
                    if (auto shortArg = readArg()) {
                        // -x value
                        processor->ShortOption(name, *shortArg);
                    } else {
                        fmt::println("Missing argument <{}> for option -{}", info->argument, name);
                        printHelp();
                        return 1;
                    }
                } else {
                    // -x
                    processor->ShortFlag(name, true);
                }
            }
        } else {
            processor->Argument(*arg);
        }
    }

    // TODO: architecture
    // - command processor (doubles as disassembly engine)
    //   - interface
    //   - abstract base class to build processors with compile-time-defined options
    //     - similar to the structured registers idea
    //   - abstract base class for ISA processors
    //     - concrete classes for each ISA
    //   - concrete class for initial processor (default)
    // - command line parser
    //   - reads argc and argv
    //   - parses the command line syntax described in README
    //   - calls command processor methods with structured data
    //   - also processes the ISA switch commands
    // - global options shared with all command processors
}
