#pragma once

#include <ymir/core/types.hpp>

#include <ymir/util/callback.hpp>

namespace ymir::scsp {

// Sample output callback, invoked every sample
using CBOutputSample = util::OptionalCallback<void(sint16 left, sint16 right)>;

} // namespace ymir::scsp
