#pragma once

#include "clocks.hpp"

#include <ymir/util/callback.hpp>

namespace ymir::sys {

// Invoked when the system clock speed changes.
using CBClockSpeedChange = util::RequiredCallback<void(const ClockRatios &clockRatios)>;

} // namespace ymir::sys
