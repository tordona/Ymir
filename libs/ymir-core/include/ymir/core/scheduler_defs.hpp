#pragma once

#include <ymir/core/types.hpp>

namespace ymir::core {

using IDtype = uint8;
using EventID = IDtype;
using UserEventID = IDtype;

// User IDs for all events in the emulator
namespace events {

    // VDP phase update
    inline constexpr UserEventID VDPPhase = 0x10;

    // SCSP sample
    inline constexpr UserEventID SCSPSample = 0x20;

    // CD block drive state update
    inline constexpr UserEventID CDBlockDriveState = 0x30;

    // CD block command processing
    inline constexpr UserEventID CDBlockCommand = 0x31;

    // SCU timer 1 interrupt
    inline constexpr UserEventID SCUTimer1 = 0x40;

    // SMPC command processing
    inline constexpr UserEventID SMPCCommand = 0x50;

} // namespace events

inline constexpr size_t kNumScheduledEvents = 6; // must match the number of events defined in the events namespace

} // namespace ymir::core
