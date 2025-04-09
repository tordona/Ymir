#pragma once

#include <app/input/input_action.hpp>

namespace app::actions {

namespace general {

    // -----------------------------------------------------------------------------------------------------------------
    // General actions

    inline constexpr input::SingleShotAction OpenLoadDiscDialog{0x10000};
    inline constexpr input::SingleShotAction EjectDisc{0x10001};
    inline constexpr input::SingleShotAction OpenCloseTray{0x10002};

    inline constexpr input::SingleShotAction ToggleWindowedVideoOutput{0x11000};
    inline constexpr input::SingleShotAction OpenSettings{0x11001};

} // namespace general

namespace emu {
    // -----------------------------------------------------------------------------------------------------------------
    // General emulator actions

    inline constexpr input::SingleShotAction HardReset{0xE0000};
    inline constexpr input::SingleShotAction SoftReset{0xE0001};

    inline constexpr input::SingleShotAction FrameStep{0xE0002};
    inline constexpr input::SingleShotAction PauseResume{0xE0003};
    inline constexpr input::BinaryAction FastForward{0xE0004};
    // TODO: inline constexpr input::BinaryAction SlowForward{0xE0005};
    // TODO: inline constexpr input::BinaryAction Reverse{0xE0006};
    // TODO: inline constexpr input::SingleShotAction ReverseFrameStep{0xE0007};

    // -----------------------------------------------------------------------------------------------------------------
    // System buttons and toggles

    inline constexpr input::BinaryAction ResetButton{0xE1000};

    // -----------------------------------------------------------------------------------------------------------------
    // Standard Saturn Pad buttons

    inline constexpr input::BinaryAction StandardPadA{0xE1100};
    inline constexpr input::BinaryAction StandardPadB{0xE1101};
    inline constexpr input::BinaryAction StandardPadC{0xE1102};
    inline constexpr input::BinaryAction StandardPadX{0xE1103};
    inline constexpr input::BinaryAction StandardPadY{0xE1104};
    inline constexpr input::BinaryAction StandardPadZ{0xE1105};
    inline constexpr input::BinaryAction StandardPadUp{0xE1106};
    inline constexpr input::BinaryAction StandardPadDown{0xE1107};
    inline constexpr input::BinaryAction StandardPadLeft{0xE1108};
    inline constexpr input::BinaryAction StandardPadRight{0xE1109};
    inline constexpr input::BinaryAction StandardPadStart{0xE110A};
    inline constexpr input::BinaryAction StandardPadL{0xE110B};
    inline constexpr input::BinaryAction StandardPadR{0xE110C};

    // -----------------------------------------------------------------------------------------------------------------
    // Debugger actions

    inline constexpr input::SingleShotAction ToggleDebugTrace{0xE2000};
    inline constexpr input::SingleShotAction DumpMemory{0xE2001};

} // namespace emu

} // namespace app::actions
