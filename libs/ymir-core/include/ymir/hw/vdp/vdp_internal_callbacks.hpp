#pragma once

/**
@file
@brief Internal callback definitions used by the VDP.
*/

#include <ymir/util/callback.hpp>

namespace ymir::vdp {

/// @brief Invoked when the HBlank and VBlank signals change.
using CBHVBlankStateChange = util::RequiredCallback<void(bool level)>;

/// @brief Invoked when specific events occur while processing.
using CBTriggerEvent = util::RequiredCallback<void()>;

} // namespace ymir::vdp
