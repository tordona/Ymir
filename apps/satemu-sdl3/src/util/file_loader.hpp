#pragma once

#include <satemu/core/types.hpp>

#include <filesystem>
#include <vector>

namespace util {

std::vector<uint8> LoadFile(std::filesystem::path romPath);

} // namespace util
