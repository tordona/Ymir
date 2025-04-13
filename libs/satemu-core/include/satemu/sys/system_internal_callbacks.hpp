#pragma once

#include "clocks.hpp"

#include <satemu/util/callback.hpp>

namespace satemu::sys {

// Invoked when the system clock speed changes.
using CBClockSpeedChange = util::RequiredCallback<void(const ClockRatios &clockRatios)>;

} // namespace satemu::sys
