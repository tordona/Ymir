#pragma once

#include <app/input/input_action.hpp>

namespace app::actions {

namespace general {

    inline constexpr auto OpenSettings = input::Action::Button(0x001000);
    inline constexpr auto ToggleWindowedVideoOutput = input::Action::Button(0x001001);

} // namespace general

namespace audio {

    inline constexpr auto ToggleMute = input::Action::Button(0x001100);
    inline constexpr auto IncreaseVolume = input::Action::Button(0x001101);
    inline constexpr auto DecreaseVolume = input::Action::Button(0x001102);

} // namespace audio

namespace cd_drive {

    inline constexpr auto LoadDisc = input::Action::Button(0x002000);
    inline constexpr auto EjectDisc = input::Action::Button(0x002001);
    inline constexpr auto OpenCloseTray = input::Action::Button(0x002002);

} // namespace cd_drive

namespace save_states {

    inline constexpr auto QuickLoadState = input::Action::Button(0x003000);
    inline constexpr auto QuickSaveState = input::Action::Button(0x003001);

    inline constexpr auto SelectState1 = input::Action::Button(0x003011);
    inline constexpr auto SelectState2 = input::Action::Button(0x003012);
    inline constexpr auto SelectState3 = input::Action::Button(0x003013);
    inline constexpr auto SelectState4 = input::Action::Button(0x003014);
    inline constexpr auto SelectState5 = input::Action::Button(0x003015);
    inline constexpr auto SelectState6 = input::Action::Button(0x003016);
    inline constexpr auto SelectState7 = input::Action::Button(0x003017);
    inline constexpr auto SelectState8 = input::Action::Button(0x003018);
    inline constexpr auto SelectState9 = input::Action::Button(0x003019);
    inline constexpr auto SelectState10 = input::Action::Button(0x00301A);

    inline constexpr auto LoadState1 = input::Action::Button(0x003021);
    inline constexpr auto LoadState2 = input::Action::Button(0x003022);
    inline constexpr auto LoadState3 = input::Action::Button(0x003023);
    inline constexpr auto LoadState4 = input::Action::Button(0x003024);
    inline constexpr auto LoadState5 = input::Action::Button(0x003025);
    inline constexpr auto LoadState6 = input::Action::Button(0x003026);
    inline constexpr auto LoadState7 = input::Action::Button(0x003027);
    inline constexpr auto LoadState8 = input::Action::Button(0x003028);
    inline constexpr auto LoadState9 = input::Action::Button(0x003029);
    inline constexpr auto LoadState10 = input::Action::Button(0x00302A);

    inline constexpr auto SaveState1 = input::Action::Button(0x003031);
    inline constexpr auto SaveState2 = input::Action::Button(0x003032);
    inline constexpr auto SaveState3 = input::Action::Button(0x003033);
    inline constexpr auto SaveState4 = input::Action::Button(0x003034);
    inline constexpr auto SaveState5 = input::Action::Button(0x003035);
    inline constexpr auto SaveState6 = input::Action::Button(0x003036);
    inline constexpr auto SaveState7 = input::Action::Button(0x003037);
    inline constexpr auto SaveState8 = input::Action::Button(0x003038);
    inline constexpr auto SaveState9 = input::Action::Button(0x003039);
    inline constexpr auto SaveState10 = input::Action::Button(0x00303A);

} // namespace save_states

namespace sys {

    inline constexpr auto HardReset = input::Action::Button(0xE01000);
    inline constexpr auto SoftReset = input::Action::Button(0xE01001);
    inline constexpr auto ResetButton = input::Action::Button(0xE01002);

} // namespace sys

namespace emu {

    inline constexpr auto TurboSpeed = input::Action::Button(0xE02000);
    // TODO: inline constexpr auto SlowMotion = input::Action::Button(0xE02001);
    inline constexpr auto PauseResume = input::Action::Button(0xE02002);
    inline constexpr auto ForwardFrameStep = input::Action::Button(0xE02003);
    inline constexpr auto ReverseFrameStep = input::Action::Button(0xE02004);
    inline constexpr auto Rewind = input::Action::Button(0xE02005);

    inline constexpr auto ToggleRewindBuffer = input::Action::Button(0xE02100);

} // namespace emu

namespace dbg {

    inline constexpr auto ToggleDebugTrace = input::Action::Button(0xE03000);
    inline constexpr auto DumpMemory = input::Action::Button(0xE03001);

} // namespace dbg

namespace control_pad {

    inline constexpr auto A = input::Action::Button(0xC01000);
    inline constexpr auto B = input::Action::Button(0xC01001);
    inline constexpr auto C = input::Action::Button(0xC01002);
    inline constexpr auto X = input::Action::Button(0xC01003);
    inline constexpr auto Y = input::Action::Button(0xC01004);
    inline constexpr auto Z = input::Action::Button(0xC01005);
    inline constexpr auto Up = input::Action::Button(0xC01006);
    inline constexpr auto Down = input::Action::Button(0xC01007);
    inline constexpr auto Left = input::Action::Button(0xC01008);
    inline constexpr auto Right = input::Action::Button(0xC01009);
    inline constexpr auto Start = input::Action::Button(0xC0100A);
    inline constexpr auto L = input::Action::Button(0xC0100B);
    inline constexpr auto R = input::Action::Button(0xC0100C);
    inline constexpr auto DPad = input::Action::Axis1D(0xC0100D);

} // namespace control_pad

} // namespace app::actions
