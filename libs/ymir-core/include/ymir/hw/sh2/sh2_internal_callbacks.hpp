#pragma once

#include <ymir/util/callback.hpp>

namespace ymir::sh2 {

// Invoked when the SH2 acknowledges an external interrupt signal.
using CBAcknowledgeExternalInterrupt = util::RequiredCallback<void()>;

} // namespace ymir::sh2
