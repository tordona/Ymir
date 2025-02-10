#pragma once

#include <filesystem>

namespace app {

struct CommandLineOptions {
    std::filesystem::path biosPath;
    std::filesystem::path gameDiscPath;
};

} // namespace app
