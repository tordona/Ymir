#pragma once

#include <ymir/core/types.hpp>

#include <ymir/hw/vdp/vdp_defs.hpp>

#include "emu_event.hpp"

namespace app::events::emu::debug {

EmuEvent ExecuteSH2Division(bool master, bool div64);

EmuEvent WriteMainMemory(uint32 address, uint8 value, bool enableSideEffects);
EmuEvent WriteSH2Memory(uint32 address, uint8 value, bool enableSideEffects, bool master, bool bypassCache);

EmuEvent SetLayerEnabled(ymir::vdp::Layer layer, bool enabled);

} // namespace app::events::emu::debug
