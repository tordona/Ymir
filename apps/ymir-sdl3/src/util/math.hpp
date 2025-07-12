#pragma once

#include <cmath>

namespace util {

double RoundToMultiple(double value, double multiple) {
    return std::round(value / multiple) * multiple;
}

} // namespace util
