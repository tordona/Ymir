#pragma once

#include <app/input/input_defs.hpp>

namespace app::actions {

namespace emu {
    // -----------------------------------------------------------------------------------------------------------------
    // General emulator actions

    inline constexpr input::ActionID HardReset = 0x1000;
    inline constexpr input::ActionID SoftReset = 0x1001;

    // -----------------------------------------------------------------------------------------------------------------
    // System buttons and toggles

    inline constexpr input::ActionID ResetButton = 0x2000;

    // -----------------------------------------------------------------------------------------------------------------
    // Standard Saturn Pad buttons

    inline constexpr input::ActionID StandardPadA = 0x2100;
    inline constexpr input::ActionID StandardPadB = 0x2101;
    inline constexpr input::ActionID StandardPadC = 0x2102;
    inline constexpr input::ActionID StandardPadX = 0x2103;
    inline constexpr input::ActionID StandardPadY = 0x2104;
    inline constexpr input::ActionID StandardPadZ = 0x2105;
    inline constexpr input::ActionID StandardPadUp = 0x2106;
    inline constexpr input::ActionID StandardPadDown = 0x2107;
    inline constexpr input::ActionID StandardPadLeft = 0x2108;
    inline constexpr input::ActionID StandardPadRight = 0x2109;
    inline constexpr input::ActionID StandardPadStart = 0x210A;
    inline constexpr input::ActionID StandardPadL = 0x210B;
    inline constexpr input::ActionID StandardPadR = 0x210C;

} // namespace emu

} // namespace app::actions
