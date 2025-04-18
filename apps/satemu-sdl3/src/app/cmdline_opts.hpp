#pragma once

#include <filesystem>

namespace app {

struct CommandLineOptions {
    std::filesystem::path iplPath;
    std::filesystem::path gameDiscPath;
};

} // namespace app
