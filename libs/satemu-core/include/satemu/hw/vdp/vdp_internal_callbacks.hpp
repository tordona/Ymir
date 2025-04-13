#pragma once

#include <satemu/util/callback.hpp>

namespace satemu::vdp {

// Invoked when various interrupt signals are raised.
using CBTriggerInterrupt = util::RequiredCallback<void()>;

// Invoked when specific events occur while processing.
using CBTriggerEvent = util::RequiredCallback<void()>;

} // namespace satemu::vdp
