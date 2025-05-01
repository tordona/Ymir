#pragma once

/**
@file
@brief Internal callback definitions used by the CD block.
*/

#include <ymir/core/types.hpp>

#include <ymir/util/callback.hpp>

namespace ymir::cdblock {

/// @brief Invoked when the CD Block raises an interrupt.
using CBTriggerExternalInterrupt0 = util::RequiredCallback<void()>;

/// @brief Invoked when the CD Block reads a CDDA sector.
///
/// The callback should return how many thirds of the audio buffer are full.
using CBCDDASector = util::RequiredCallback<uint32(std::span<uint8, 2048> data)>;

} // namespace ymir::cdblock
