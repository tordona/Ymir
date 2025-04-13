#pragma once

#include <satemu/core/types.hpp>

#include <satemu/util/callback.hpp>

namespace satemu::scsp {

// Sample output callback, invoked every sample
using CBOutputSample = util::OptionalCallback<void(sint16 left, sint16 right)>;

} // namespace satemu::scsp
