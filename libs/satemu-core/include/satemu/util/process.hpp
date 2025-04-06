#pragma once

#include <filesystem>

namespace util {

std::filesystem::path GetCurrentProcessExecutablePath();
void BoostCurrentProcessPriority(bool boost);
void BoostCurrentThreadPriority(bool boost);

} // namespace util
