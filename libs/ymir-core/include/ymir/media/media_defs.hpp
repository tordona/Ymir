#pragma once

#include <ymir/core/types.hpp>

namespace ymir::media {

enum class AreaCode : uint16 {
    None = 0,

    Japan = 1u << 0x1,
    AsiaNTSC = 1u << 0x2,
    NorthAmerica = 1u << 0x4,
    CentralSouthAmericaNTSC = 1u << 0x5,
    Korea = 1u << 0x6,
    AsiaPAL = 1u << 0xA,
    EuropePAL = 1u << 0xC,
    CentralSouthAmericaPAL = 1u << 0xD,
};

enum class PeripheralCode : uint16 {
    None = 0,

    ControlPad = 1u << 0u,
    AnalogPad = 1u << 1u,
    Mouse = 1u << 2u,
    Keyboard = 1u << 3u,
    SteeringWheel = 1u << 4u,
    Multitap = 1u << 5u,
    VirtuaGun = 1u << 6u,
};

} // namespace ymir::media
