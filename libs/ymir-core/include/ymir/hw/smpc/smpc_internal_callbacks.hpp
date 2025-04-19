#pragma once

#include <ymir/util/callback.hpp>

namespace ymir::smpc {

// Invoked when INTBACK finishes processing to raise the SCU System Manager interrupt signal.
using CBSystemManagerInterruptCallback = util::RequiredCallback<void()>;

} // namespace ymir::smpc
