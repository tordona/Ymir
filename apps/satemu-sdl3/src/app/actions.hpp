#pragma once

#include <app/input/input_action.hpp>

namespace app::actions {

namespace general {

    inline constexpr input::ActionID OpenSettings{0x001000};
    inline constexpr input::ActionID ToggleWindowedVideoOutput{0x001001};

} // namespace general

namespace cd_drive {

    inline constexpr input::ActionID LoadDisc{0x002000};
    inline constexpr input::ActionID EjectDisc{0x002001};
    inline constexpr input::ActionID OpenCloseTray{0x002002};

} // namespace cd_drive

namespace sys {

    inline constexpr input::ActionID HardReset{0xE01000};
    inline constexpr input::ActionID SoftReset{0xE01001};
    inline constexpr input::ActionID ResetButton{0xE01002};

} // namespace sys

namespace emu {

    inline constexpr input::ActionID PauseResume{0xE02000};
    inline constexpr input::ActionID FrameStep{0xE02001};
    inline constexpr input::ActionID FastForward{0xE02002};
    // TODO: inline constexpr input::ActionID SlowForward{0xE02003};
    // TODO: inline constexpr input::ActionID Reverse{0xE02004};
    // TODO: inline constexpr input::ActionID ReverseFrameStep{0xE02005};

} // namespace emu

namespace dbg {

    inline constexpr input::ActionID ToggleDebugTrace{0xE03000};
    inline constexpr input::ActionID DumpMemory{0xE03001};

} // namespace dbg

namespace std_saturn_pad {

    inline constexpr input::ActionID A{0xC01000};
    inline constexpr input::ActionID B{0xC01001};
    inline constexpr input::ActionID C{0xC01002};
    inline constexpr input::ActionID X{0xC01003};
    inline constexpr input::ActionID Y{0xC01004};
    inline constexpr input::ActionID Z{0xC01005};
    inline constexpr input::ActionID Up{0xC01006};
    inline constexpr input::ActionID Down{0xC01007};
    inline constexpr input::ActionID Left{0xC01008};
    inline constexpr input::ActionID Right{0xC01009};
    inline constexpr input::ActionID Start{0xC0100A};
    inline constexpr input::ActionID L{0xC0100B};
    inline constexpr input::ActionID R{0xC0100C};

} // namespace std_saturn_pad

} // namespace app::actions
