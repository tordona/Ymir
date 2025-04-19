#pragma once

#include <ymir/util/callback.hpp>

namespace ymir::scsp {

// SCU sound request interrupt trigger callback
using CBTriggerSoundRequestInterrupt = util::RequiredCallback<void(bool level)>;

} // namespace ymir::scsp
