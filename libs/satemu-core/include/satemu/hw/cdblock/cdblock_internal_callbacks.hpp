#pragma once

#include <satemu/core/types.hpp>

#include <satemu/util/callback.hpp>

namespace satemu::cdblock {

// Invoked when the CD Block raises an interrupt.
using CBTriggerExternalInterrupt0 = util::RequiredCallback<void()>;

// Invoked when the CD Block reads a CDDA sector.
// The callback should return how many thirds of the audio buffer are full.
using CBCDDASector = util::RequiredCallback<uint32(std::span<uint8, 2048> data)>;

} // namespace satemu::cdblock
