#pragma once

#include <app/input/input_action.hpp>

namespace app::actions {

namespace general {

    inline constexpr input::ActionID OpenSettings{0x001000};
    inline constexpr input::ActionID ToggleWindowedVideoOutput{0x001001};

} // namespace general

namespace audio {

    inline constexpr input::ActionID ToggleMute{0x001100};
    inline constexpr input::ActionID IncreaseVolume{0x001101};
    inline constexpr input::ActionID DecreaseVolume{0x001102};

} // namespace audio

namespace cd_drive {

    inline constexpr input::ActionID LoadDisc{0x002000};
    inline constexpr input::ActionID EjectDisc{0x002001};
    inline constexpr input::ActionID OpenCloseTray{0x002002};

} // namespace cd_drive

namespace save_states {

    inline constexpr input::ActionID QuickLoadState{0x003000};
    inline constexpr input::ActionID QuickSaveState{0x003001};

    inline constexpr input::ActionID SelectState1{0x003011};
    inline constexpr input::ActionID SelectState2{0x003012};
    inline constexpr input::ActionID SelectState3{0x003013};
    inline constexpr input::ActionID SelectState4{0x003014};
    inline constexpr input::ActionID SelectState5{0x003015};
    inline constexpr input::ActionID SelectState6{0x003016};
    inline constexpr input::ActionID SelectState7{0x003017};
    inline constexpr input::ActionID SelectState8{0x003018};
    inline constexpr input::ActionID SelectState9{0x003019};
    inline constexpr input::ActionID SelectState10{0x00301A};

    inline constexpr input::ActionID LoadState1{0x003021};
    inline constexpr input::ActionID LoadState2{0x003022};
    inline constexpr input::ActionID LoadState3{0x003023};
    inline constexpr input::ActionID LoadState4{0x003024};
    inline constexpr input::ActionID LoadState5{0x003025};
    inline constexpr input::ActionID LoadState6{0x003026};
    inline constexpr input::ActionID LoadState7{0x003027};
    inline constexpr input::ActionID LoadState8{0x003028};
    inline constexpr input::ActionID LoadState9{0x003029};
    inline constexpr input::ActionID LoadState10{0x00302A};

    inline constexpr input::ActionID SaveState1{0x003031};
    inline constexpr input::ActionID SaveState2{0x003032};
    inline constexpr input::ActionID SaveState3{0x003033};
    inline constexpr input::ActionID SaveState4{0x003034};
    inline constexpr input::ActionID SaveState5{0x003035};
    inline constexpr input::ActionID SaveState6{0x003036};
    inline constexpr input::ActionID SaveState7{0x003037};
    inline constexpr input::ActionID SaveState8{0x003038};
    inline constexpr input::ActionID SaveState9{0x003039};
    inline constexpr input::ActionID SaveState10{0x00303A};

} // namespace save_states

namespace sys {

    inline constexpr input::ActionID HardReset{0xE01000};
    inline constexpr input::ActionID SoftReset{0xE01001};
    inline constexpr input::ActionID ResetButton{0xE01002};

} // namespace sys

namespace emu {

    inline constexpr input::ActionID TurboSpeed{0xE02000};
    // TODO: inline constexpr input::ActionID SlowMotion{0xE02001};
    inline constexpr input::ActionID PauseResume{0xE02002};
    inline constexpr input::ActionID ForwardFrameStep{0xE02003};
    inline constexpr input::ActionID ReverseFrameStep{0xE02004};
    inline constexpr input::ActionID Rewind{0xE02005};

    inline constexpr input::ActionID ToggleRewindBuffer{0xE02100};

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
