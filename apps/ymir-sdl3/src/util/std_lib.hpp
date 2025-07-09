#pragma once

#include <chrono>
#include <ctime>

namespace util {

tm to_local_time(std::chrono::system_clock::time_point tp);

} // namespace util
