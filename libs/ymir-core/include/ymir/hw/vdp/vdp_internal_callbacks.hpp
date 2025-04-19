#pragma once

#include <ymir/util/callback.hpp>

namespace ymir::vdp {

// Invoked when various interrupt signals are raised.
using CBTriggerInterrupt = util::RequiredCallback<void()>;

// Invoked when specific events occur while processing.
using CBTriggerEvent = util::RequiredCallback<void()>;

} // namespace ymir::vdp
