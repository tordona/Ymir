#pragma once

#include <satemu/core/types.hpp>

#include <satemu/util/callback.hpp>

namespace satemu::scsp {

// Sample output callback, invoked every sample
using CBOutputSample = util::OptionalCallback<void(sint16 left, sint16 right)>;

// SCU sound request interrupt trigger callback
using CBTriggerSoundRequestInterrupt = util::RequiredCallback<void(bool level)>;

} // namespace satemu::scsp
