#pragma once

#include <app/input/input_defs.hpp>

namespace app::actions {

namespace general {

    // -----------------------------------------------------------------------------------------------------------------
    // General actions

    inline constexpr input::ActionID OpenLoadDiscDialog = 0x10000;
    inline constexpr input::ActionID EjectDisc = 0x10001;
    inline constexpr input::ActionID OpenCloseTray = 0x10002;
    
    inline constexpr input::ActionID ToggleWindowedVideoOutput = 0x11000;

} // namespace general

namespace emu {
    // -----------------------------------------------------------------------------------------------------------------
    // General emulator actions

    inline constexpr input::ActionID HardReset = 0xE0000;
    inline constexpr input::ActionID SoftReset = 0xE0001;

    inline constexpr input::ActionID FrameStep = 0xE0002;
    inline constexpr input::ActionID PauseResume = 0xE0003;
    inline constexpr input::ActionID FastForward = 0xE0004;
    // TODO: inline constexpr input::ActionID SlowForward = 0xE0005;
    // TODO: inline constexpr input::ActionID Reverse = 0xE0006;
    // TODO: inline constexpr input::ActionID ReverseFrameStep = 0xE0007;

    // -----------------------------------------------------------------------------------------------------------------
    // System buttons and toggles

    inline constexpr input::ActionID ResetButton = 0xE1000;

    // -----------------------------------------------------------------------------------------------------------------
    // Standard Saturn Pad buttons

    inline constexpr input::ActionID StandardPadA = 0xE1100;
    inline constexpr input::ActionID StandardPadB = 0xE1101;
    inline constexpr input::ActionID StandardPadC = 0xE1102;
    inline constexpr input::ActionID StandardPadX = 0xE1103;
    inline constexpr input::ActionID StandardPadY = 0xE1104;
    inline constexpr input::ActionID StandardPadZ = 0xE1105;
    inline constexpr input::ActionID StandardPadUp = 0xE1106;
    inline constexpr input::ActionID StandardPadDown = 0xE1107;
    inline constexpr input::ActionID StandardPadLeft = 0xE1108;
    inline constexpr input::ActionID StandardPadRight = 0xE1109;
    inline constexpr input::ActionID StandardPadStart = 0xE110A;
    inline constexpr input::ActionID StandardPadL = 0xE110B;
    inline constexpr input::ActionID StandardPadR = 0xE110C;

    // -----------------------------------------------------------------------------------------------------------------
    // Debugger actions

    inline constexpr input::ActionID ToggleDebugTrace = 0xE2000;
    inline constexpr input::ActionID DumpMemory = 0xE2001;

} // namespace emu

} // namespace app::actions
