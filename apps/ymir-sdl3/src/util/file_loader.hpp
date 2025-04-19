#pragma once

#include <ymir/core/types.hpp>

#include <filesystem>
#include <vector>

namespace util {

std::vector<uint8> LoadFile(std::filesystem::path romPath);

} // namespace util
