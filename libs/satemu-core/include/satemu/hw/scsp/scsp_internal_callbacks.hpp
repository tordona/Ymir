#pragma once

#include <satemu/util/callback.hpp>

namespace satemu::scsp {

// SCU sound request interrupt trigger callback
using CBTriggerSoundRequestInterrupt = util::RequiredCallback<void(bool level)>;

} // namespace satemu::scsp
