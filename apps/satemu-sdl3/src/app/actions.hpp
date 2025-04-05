#pragma once

#include <app/input/input_defs.hpp>

namespace app::actions {

namespace emu {
    // Performs a hard reset on the system
    inline constexpr input::ActionID HardReset = 0x10000;
    // Performs a soft reset on the system
    inline constexpr input::ActionID SoftReset = 0x10001;

    // -----------------------------------------------------------------------------------------------------------------
    // Buttons

    // Reset button state
    inline constexpr input::ActionID ResetButton = 0x11000;

} // namespace emu

} // namespace app::actions
