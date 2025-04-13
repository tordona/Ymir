#pragma once

#include <satemu/core/types.hpp>

#include <satemu/util/callback.hpp>

namespace satemu::scu {

// Invoked when the SCU raises an interrupt.
using CBExternalInterrupt = util::RequiredCallback<void(uint8 level, uint8 vector)>;

} // namespace satemu::scu
