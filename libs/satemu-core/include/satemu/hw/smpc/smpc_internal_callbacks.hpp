#pragma once

#include <satemu/util/callback.hpp>

namespace satemu::smpc {

// Invoked when INTBACK finishes processing to raise the SCU System Manager interrupt signal.
using CBSystemManagerInterruptCallback = util::RequiredCallback<void()>;

} // namespace satemu::smpc
