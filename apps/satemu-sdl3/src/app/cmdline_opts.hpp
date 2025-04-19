#pragma once

#include <filesystem>

namespace app {

struct CommandLineOptions {
    std::filesystem::path iplPath;
    std::filesystem::path gameDiscPath;
    std::filesystem::path profilePath;
};

} // namespace app
