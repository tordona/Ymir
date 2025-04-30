#pragma once

/**
@file
@brief Internal VDP callbacks.
*/

#include <ymir/util/callback.hpp>

namespace ymir::vdp {

// Invoked when the HBlank and VBlank signals change.
using CBHVBlankStateChange = util::RequiredCallback<void(bool level)>;

// Invoked when specific events occur while processing.
using CBTriggerEvent = util::RequiredCallback<void()>;

} // namespace ymir::vdp
