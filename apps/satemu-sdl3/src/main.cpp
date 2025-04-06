#include "app/app.hpp"

#include <cxxopts.hpp>
#include <fmt/format.h>

#include <memory>

int main(int argc, char **argv) {
    bool showHelp = false;

    app::CommandLineOptions progOpts{};
    cxxopts::Options options("satemu", "satemu - Unnamed Sega Saturn emulator");
    options.add_options()("f,file", "Path to Saturn disc image (.ccd, .cue, .iso, .mds)",
                          cxxopts::value(progOpts.gameDiscPath));
    options.add_options()("b,bios", "Path to Saturn BIOS ROM", cxxopts::value(progOpts.biosPath));
    options.add_options()("h,help", "Display help text", cxxopts::value(showHelp)->default_value("false"));
    options.parse_positional({"file"});
    auto result = options.parse(argc, argv);

    try {
        if (showHelp) {
            fmt::println("{}", options.help());
            return 0;
        }

        auto app = std::make_unique<app::App>();
        return app->Run(progOpts);
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

    return EXIT_SUCCESS;
}
