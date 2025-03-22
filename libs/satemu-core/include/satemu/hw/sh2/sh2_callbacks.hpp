#pragma once

#include <satemu/util/callback.hpp>

namespace satemu::sh2 {

// Invoked when the SH2 acknowledges an external interrupt signal.
using CBAcknowledgeExternalInterrupt = util::RequiredCallback<void()>;

} // namespace satemu::sh2
