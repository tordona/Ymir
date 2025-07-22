#include "settings.hpp"

#include <app/shared_context.hpp>

#include <ymir/sys/saturn.hpp>

#include <ymir/util/dev_log.hpp>
#include <ymir/util/inline.hpp>

#include <util/math.hpp>

#include <cassert>

using namespace std::literals;
using namespace ymir;

namespace app {

// Increment this version and implement a new LoadV# when making breaking changes to the settings file structure.
// Existing versions should convert old formats on a best-effort basis.
//
// Change history:
// v2:
// - Renamed "Input.Port#.StandardPadBinds" to "Input.Port#.ControlPadBinds"
// v3:
// - Moved "OverrideUIScale" and "UIScale" from "Video" to "GUI"
inline constexpr int kConfigVersion = 3;

namespace grp {

    // -----------------------------------------------------------------------------
    // Dev log groups

    // Hierarchy:
    //
    // settings

    struct settings {
        static constexpr bool enabled = true;
        static constexpr devlog::Level level = devlog::level::debug;
        static constexpr std::string_view name = "Settings";
    };

} // namespace grp

// -------------------------------------------------------------------------------------------------
// Enum parsers

FORCE_INLINE static void Parse(toml::node_view<toml::node> &node, db::SystemVariant &value) {
    value = db::SystemVariant::Saturn;
    if (auto opt = node.value<std::string>()) {
        if (*opt == "Saturn"s) {
            value = db::SystemVariant::Saturn;
        } else if (*opt == "HiSaturn"s) {
            value = db::SystemVariant::HiSaturn;
        } else if (*opt == "VSaturn"s) {
            value = db::SystemVariant::VSaturn;
        }
    }
}

FORCE_INLINE static void Parse(toml::node_view<toml::node> &node, core::config::sys::Region &value) {
    value = core::config::sys::Region::Japan;
    if (auto opt = node.value<std::string>()) {
        if (*opt == "Japan"s) {
            value = core::config::sys::Region::Japan;
        } else if (*opt == "AsiaNTSC"s) {
            value = core::config::sys::Region::AsiaNTSC;
        } else if (*opt == "NorthAmerica"s) {
            value = core::config::sys::Region::NorthAmerica;
        } else if (*opt == "EuropePAL"s) {
            value = core::config::sys::Region::EuropePAL;
        } else if (*opt == "CentralSouthAmericaNTSC"s) {
            value = core::config::sys::Region::NorthAmerica;
        } else if (*opt == "Korea"s) {
            value = core::config::sys::Region::AsiaNTSC;
        } else if (*opt == "AsiaPAL"s) {
            value = core::config::sys::Region::EuropePAL;
        } else if (*opt == "CentralSouthAmericaPAL"s) {
            value = core::config::sys::Region::EuropePAL;
        }
    }
}

FORCE_INLINE static void Parse(toml::node_view<toml::node> &node, core::config::sys::VideoStandard &value) {
    value = core::config::sys::VideoStandard::NTSC;
    if (auto opt = node.value<std::string>()) {
        if (*opt == "NTSC"s) {
            value = core::config::sys::VideoStandard::NTSC;
        } else if (*opt == "PAL"s) {
            value = core::config::sys::VideoStandard::PAL;
        }
    }
}

FORCE_INLINE static void Parse(toml::node_view<toml::node> &node, core::config::rtc::Mode &value) {
    value = core::config::rtc::Mode::Host;
    if (auto opt = node.value<std::string>()) {
        if (*opt == "Host"s) {
            value = core::config::rtc::Mode::Host;
        } else if (*opt == "Virtual"s) {
            value = core::config::rtc::Mode::Virtual;
        }
    }
}

FORCE_INLINE static void Parse(toml::node_view<toml::node> &node, core::config::rtc::HardResetStrategy &value) {
    value = core::config::rtc::HardResetStrategy::Preserve;
    if (auto opt = node.value<std::string>()) {
        if (*opt == "PreserveCurrentTime"s) {
            value = core::config::rtc::HardResetStrategy::Preserve;
        } else if (*opt == "SyncToHost"s) {
            value = core::config::rtc::HardResetStrategy::SyncToHost;
        } else if (*opt == "SyncToFixedStartingTime"s) {
            value = core::config::rtc::HardResetStrategy::ResetToFixedTime;
        }
    }
}

FORCE_INLINE static void Parse(toml::node_view<toml::node> &node, peripheral::PeripheralType &value) {
    value = peripheral::PeripheralType::None;
    if (auto opt = node.value<std::string>()) {
        if (*opt == "None"s) {
            value = peripheral::PeripheralType::None;
        } else if (*opt == "StandardPad"s) { // obsoleted in v2
            value = peripheral::PeripheralType::ControlPad;
        } else if (*opt == "ControlPad"s) {
            value = peripheral::PeripheralType::ControlPad;
        } else if (*opt == "AnalogPad"s) {
            value = peripheral::PeripheralType::AnalogPad;
        } else if (*opt == "ArcadeRacer"s) {
            value = peripheral::PeripheralType::ArcadeRacer;
        }
    }
}

FORCE_INLINE static void Parse(toml::node_view<toml::node> &node, core::config::audio::SampleInterpolationMode &value) {
    value = core::config::audio::SampleInterpolationMode::NearestNeighbor;
    if (auto opt = node.value<std::string>()) {
        if (*opt == "Nearest"s) {
            value = core::config::audio::SampleInterpolationMode::NearestNeighbor;
        } else if (*opt == "Linear"s) {
            value = core::config::audio::SampleInterpolationMode::Linear;
        }
    }
}

FORCE_INLINE static void Parse(toml::node_view<toml::node> &node, Settings::GUI::FrameRateOSDPosition &value) {
    value = Settings::GUI::FrameRateOSDPosition::TopRight;
    if (auto opt = node.value<std::string>()) {
        if (*opt == "TopLeft"s) {
            value = Settings::GUI::FrameRateOSDPosition::TopLeft;
        } else if (*opt == "TopRight"s) {
            value = Settings::GUI::FrameRateOSDPosition::TopRight;
        } else if (*opt == "BottomLeft"s) {
            value = Settings::GUI::FrameRateOSDPosition::BottomLeft;
        } else if (*opt == "BottomRight"s) {
            value = Settings::GUI::FrameRateOSDPosition::BottomRight;
        }
    }
}

FORCE_INLINE static void Parse(toml::node_view<toml::node> &node, Settings::Video::DisplayRotation &value) {
    value = Settings::Video::DisplayRotation::Normal;
    if (auto opt = node.value<std::string>()) {
        if (*opt == "Normal"s) {
            value = Settings::Video::DisplayRotation::Normal;
        } else if (*opt == "90CW"s) {
            value = Settings::Video::DisplayRotation::_90CW;
        } else if (*opt == "180"s) {
            value = Settings::Video::DisplayRotation::_180;
        } else if (*opt == "90CCW"s) {
            value = Settings::Video::DisplayRotation::_90CCW;
        }
    }
}

FORCE_INLINE static void Parse(toml::node_view<toml::node> &node, Settings::Audio::MidiPort::Type &value) {
    value = Settings::Audio::MidiPort::Type::None;
    if (auto opt = node.value<std::string>()) {
        if (*opt == "None"s) {
            value = Settings::Audio::MidiPort::Type::None;
        } else if (*opt == "Normal"s) {
            value = Settings::Audio::MidiPort::Type::Normal;
        } else if (*opt == "Virtual"s) {
            value = Settings::Audio::MidiPort::Type::Virtual;
        }
    }
}

FORCE_INLINE static void Parse(toml::node_view<toml::node> &node, Settings::Cartridge::Type &value) {
    value = Settings::Cartridge::Type::None;
    if (auto opt = node.value<std::string>()) {
        if (*opt == "None"s) {
            value = Settings::Cartridge::Type::None;
        } else if (*opt == "BackupRAM"s) {
            value = Settings::Cartridge::Type::BackupRAM;
        } else if (*opt == "DRAM"s) {
            value = Settings::Cartridge::Type::DRAM;
        } else if (*opt == "ROM"s) {
            value = Settings::Cartridge::Type::ROM;
        }
    }
}

FORCE_INLINE static void Parse(toml::node_view<toml::node> &node, Settings::Cartridge::BackupRAM::Capacity &value) {
    value = Settings::Cartridge::BackupRAM::Capacity::_32Mbit;
    if (auto opt = node.value<std::string>()) {
        if (*opt == "32Mbit"s) {
            value = Settings::Cartridge::BackupRAM::Capacity::_32Mbit;
        } else if (*opt == "16Mbit"s) {
            value = Settings::Cartridge::BackupRAM::Capacity::_16Mbit;
        } else if (*opt == "8Mbit"s) {
            value = Settings::Cartridge::BackupRAM::Capacity::_8Mbit;
        } else if (*opt == "4Mbit"s) {
            value = Settings::Cartridge::BackupRAM::Capacity::_4Mbit;
        }
    }
}

FORCE_INLINE static void Parse(toml::node_view<toml::node> &node, Settings::Cartridge::DRAM::Capacity &value) {
    value = Settings::Cartridge::DRAM::Capacity::_32Mbit;
    if (auto opt = node.value<std::string>()) {
        if (*opt == "32Mbit"s) {
            value = Settings::Cartridge::DRAM::Capacity::_32Mbit;
        } else if (*opt == "8Mbit"s) {
            value = Settings::Cartridge::DRAM::Capacity::_8Mbit;
        }
    }
}

// -------------------------------------------------------------------------------------------------
// Enum-to-string converters

FORCE_INLINE static const char *ToTOML(const db::SystemVariant value) {
    switch (value) {
    default: [[fallthrough]];
    case db::SystemVariant::Saturn: return "Saturn";
    case db::SystemVariant::HiSaturn: return "HiSaturn";
    case db::SystemVariant::VSaturn: return "VSaturn";
    }
}

FORCE_INLINE static const char *ToTOML(const core::config::sys::Region value) {
    switch (value) {
    default: [[fallthrough]];
    case core::config::sys::Region::Japan: return "Japan";
    case core::config::sys::Region::AsiaNTSC: return "AsiaNTSC";
    case core::config::sys::Region::NorthAmerica: return "NorthAmerica";
    case core::config::sys::Region::EuropePAL: return "EuropePAL";

    case core::config::sys::Region::CentralSouthAmericaNTSC: return "NorthAmerica";
    case core::config::sys::Region::Korea: return "AsiaNTSC";
    case core::config::sys::Region::AsiaPAL: return "EuropePAL";
    case core::config::sys::Region::CentralSouthAmericaPAL: return "EuropePAL";
    }
}

FORCE_INLINE static const char *ToTOML(const core::config::sys::VideoStandard value) {
    switch (value) {
    default: [[fallthrough]];
    case core::config::sys::VideoStandard::NTSC: return "NTSC";
    case core::config::sys::VideoStandard::PAL: return "PAL";
    }
}

FORCE_INLINE static const char *ToTOML(const core::config::rtc::Mode value) {
    switch (value) {
    default: [[fallthrough]];
    case core::config::rtc::Mode::Host: return "Host";
    case core::config::rtc::Mode::Virtual: return "Virtual";
    }
}

FORCE_INLINE static const char *ToTOML(const core::config::rtc::HardResetStrategy value) {
    switch (value) {
    default: [[fallthrough]];
    case core::config::rtc::HardResetStrategy::Preserve: return "Preserve";
    case core::config::rtc::HardResetStrategy::SyncToHost: return "SyncToHost";
    case core::config::rtc::HardResetStrategy::ResetToFixedTime: return "ResetToFixedTime";
    }
}

FORCE_INLINE static const char *ToTOML(const peripheral::PeripheralType value) {
    switch (value) {
    default: [[fallthrough]];
    case peripheral::PeripheralType::None: return "None";
    case peripheral::PeripheralType::ControlPad: return "ControlPad";
    case peripheral::PeripheralType::AnalogPad: return "AnalogPad";
    case peripheral::PeripheralType::ArcadeRacer: return "ArcadeRacer";
    }
}

FORCE_INLINE static const char *ToTOML(const core::config::audio::SampleInterpolationMode value) {
    switch (value) {
    default: [[fallthrough]];
    case core::config::audio::SampleInterpolationMode::NearestNeighbor: return "Nearest";
    case core::config::audio::SampleInterpolationMode::Linear: return "Linear";
    }
}

FORCE_INLINE static const char *ToTOML(const Settings::GUI::FrameRateOSDPosition value) {
    switch (value) {
    case Settings::GUI::FrameRateOSDPosition::TopLeft: return "TopLeft";
    default: [[fallthrough]];
    case Settings::GUI::FrameRateOSDPosition::TopRight: return "TopRight";
    case Settings::GUI::FrameRateOSDPosition::BottomLeft: return "BottomLeft";
    case Settings::GUI::FrameRateOSDPosition::BottomRight: return "BottomRight";
    }
}

FORCE_INLINE static const char *ToTOML(const Settings::Video::DisplayRotation value) {
    switch (value) {
    default: [[fallthrough]];
    case Settings::Video::DisplayRotation::Normal: return "Normal";
    case Settings::Video::DisplayRotation::_90CW: return "90CW";
    case Settings::Video::DisplayRotation::_180: return "180";
    case Settings::Video::DisplayRotation::_90CCW: return "90CCW";
    }
}

FORCE_INLINE static const char *ToTOML(const Settings::Audio::MidiPort::Type value) {
    switch (value) {
    default: [[fallthrough]];
    case Settings::Audio::MidiPort::Type::None: return "None";
    case Settings::Audio::MidiPort::Type::Normal: return "Normal";
    case Settings::Audio::MidiPort::Type::Virtual: return "Virtual";
    }
}

FORCE_INLINE static const char *ToTOML(const Settings::Cartridge::Type value) {
    switch (value) {
    default: [[fallthrough]];
    case Settings::Cartridge::Type::None: return "None";
    case Settings::Cartridge::Type::BackupRAM: return "BackupRAM";
    case Settings::Cartridge::Type::DRAM: return "DRAM";
    case Settings::Cartridge::Type::ROM: return "ROM";
    }
}

FORCE_INLINE static const char *ToTOML(const Settings::Cartridge::BackupRAM::Capacity value) {
    switch (value) {
    default: [[fallthrough]];
    case Settings::Cartridge::BackupRAM::Capacity::_32Mbit: return "32Mbit";
    case Settings::Cartridge::BackupRAM::Capacity::_16Mbit: return "16Mbit";
    case Settings::Cartridge::BackupRAM::Capacity::_8Mbit: return "8Mbit";
    case Settings::Cartridge::BackupRAM::Capacity::_4Mbit: return "4Mbit";
    }
}

FORCE_INLINE static const char *ToTOML(const Settings::Cartridge::DRAM::Capacity value) {
    switch (value) {
    default: [[fallthrough]];
    case Settings::Cartridge::DRAM::Capacity::_32Mbit: return "32Mbit";
    case Settings::Cartridge::DRAM::Capacity::_8Mbit: return "8Mbit";
    }
}

// -------------------------------------------------------------------------------------------------
// Parsers

template <typename T>
FORCE_INLINE static void Parse(toml::node_view<toml::node> &node, T &value) {
    if (auto opt = node.value<T>()) {
        value = *opt;
    }
}

template <typename T>
FORCE_INLINE static void Parse(toml::node_view<toml::node> &node, const char *name, T &value) {
    toml::node_view view{node[name]};
    Parse(view, value);
}

FORCE_INLINE static void Parse(toml::node_view<toml::node> &node, const char *name, std::filesystem::path &value) {
    if (auto opt = node[name].value<std::filesystem::path::string_type>()) {
        value = *opt;
    }
}

template <typename T>
FORCE_INLINE static void Parse(toml::node_view<toml::node> &node, const char *name, std::vector<T> &value) {
    if (toml::array *arr = node[name].as_array()) {
        value.clear();
        for (toml::node &node : *arr) {
            toml::node_view view{node};
            Parse(view, value.emplace_back());
        }
    }
}

template <typename T>
FORCE_INLINE static void Parse(toml::node_view<toml::node> &node, const char *name, util::Observable<T> &value) {
    T wrappedValue = value.Get();
    Parse(node, name, wrappedValue);
    value = wrappedValue;
}

// Reads until the InputElementArray is full or runs out of entries, skipping all invalid and "None" entries.
FORCE_INLINE static void Parse(toml::node_view<toml::node> &node, const char *name, input::InputBind &value) {
    if (toml::array *arr = node[name].as_array()) {
        value.elements.fill({});
        const size_t count = arr->size();
        size_t outIndex = 0;
        for (size_t i = 0; i < count && outIndex < input::kNumBindsPerInput; i++) {
            if (auto opt = arr->at(i).value<std::string_view>()) {
                auto &element = value.elements[i];
                input::TryParse((*opt), element);
                if (element.type != input::InputElement::Type::None) {
                    ++outIndex;
                }
            }
        }
    }
}

// -------------------------------------------------------------------------------------------------
// Value-to-string converters

// Creates a TOML array with valid entries (skips Nones).
FORCE_INLINE static toml::array ToTOML(const input::InputBind &value) {
    toml::array out{};
    for (auto &element : value.elements) {
        if (element.type != input::InputElement::Type::None) {
            out.push_back(input::ToString(element));
        }
    }
    return out;
}

template <typename T>
FORCE_INLINE static toml::array ToTOML(const std::vector<T> &value) {
    toml::array out{};
    for (auto &item : value) {
        out.push_back(ToTOML(item));
    }
    return out;
}

// -------------------------------------------------------------------------------------------------
// Implementation

Settings::Settings(SharedContext &sharedCtx) noexcept
    : m_context(sharedCtx) {

    auto mapInput = [&](InputMap &map, input::InputBind &bind) {
        assert(!map.map.contains(bind.action));
        map.map[bind.action].insert(&bind);
    };

    m_port1ControlPadInputs.context = &m_context.controlPadInputs[0];
    m_port2ControlPadInputs.context = &m_context.controlPadInputs[1];

    m_port1AnalogPadInputs.context = &m_context.analogPadInputs[0];
    m_port2AnalogPadInputs.context = &m_context.analogPadInputs[1];

    m_port1ArcadeRacerInputs.context = &m_context.arcadeRacerInputs[0];
    m_port2ArcadeRacerInputs.context = &m_context.arcadeRacerInputs[1];

    mapInput(m_actionInputs, hotkeys.openSettings);
    mapInput(m_actionInputs, hotkeys.toggleWindowedVideoOutput);
    mapInput(m_actionInputs, hotkeys.toggleFullScreen);
    mapInput(m_actionInputs, hotkeys.toggleFrameRateOSD);
    mapInput(m_actionInputs, hotkeys.nextFrameRateOSDPos);
    mapInput(m_actionInputs, hotkeys.prevFrameRateOSDPos);

    mapInput(m_actionInputs, hotkeys.toggleMute);
    mapInput(m_actionInputs, hotkeys.increaseVolume);
    mapInput(m_actionInputs, hotkeys.decreaseVolume);

    mapInput(m_actionInputs, hotkeys.loadDisc);
    mapInput(m_actionInputs, hotkeys.ejectDisc);
    mapInput(m_actionInputs, hotkeys.openCloseTray);

    mapInput(m_actionInputs, hotkeys.hardReset);
    mapInput(m_actionInputs, hotkeys.softReset);
    mapInput(m_actionInputs, hotkeys.resetButton);

    mapInput(m_actionInputs, hotkeys.turboSpeed);
    mapInput(m_actionInputs, hotkeys.turboSpeedHold);
    mapInput(m_actionInputs, hotkeys.toggleAlternateSpeed);
    mapInput(m_actionInputs, hotkeys.increaseSpeed);
    mapInput(m_actionInputs, hotkeys.decreaseSpeed);
    mapInput(m_actionInputs, hotkeys.increaseSpeedLarge);
    mapInput(m_actionInputs, hotkeys.decreaseSpeedLarge);
    mapInput(m_actionInputs, hotkeys.resetSpeed);
    mapInput(m_actionInputs, hotkeys.pauseResume);
    mapInput(m_actionInputs, hotkeys.fwdFrameStep);
    mapInput(m_actionInputs, hotkeys.revFrameStep);
    mapInput(m_actionInputs, hotkeys.rewind);
    mapInput(m_actionInputs, hotkeys.toggleRewindBuffer);

    mapInput(m_actionInputs, hotkeys.toggleDebugTrace);
    mapInput(m_actionInputs, hotkeys.dumpMemory);

    mapInput(m_actionInputs, hotkeys.saveStates.quickLoad);
    mapInput(m_actionInputs, hotkeys.saveStates.quickSave);

    mapInput(m_actionInputs, hotkeys.saveStates.select1);
    mapInput(m_actionInputs, hotkeys.saveStates.select2);
    mapInput(m_actionInputs, hotkeys.saveStates.select3);
    mapInput(m_actionInputs, hotkeys.saveStates.select4);
    mapInput(m_actionInputs, hotkeys.saveStates.select5);
    mapInput(m_actionInputs, hotkeys.saveStates.select6);
    mapInput(m_actionInputs, hotkeys.saveStates.select7);
    mapInput(m_actionInputs, hotkeys.saveStates.select8);
    mapInput(m_actionInputs, hotkeys.saveStates.select9);
    mapInput(m_actionInputs, hotkeys.saveStates.select10);

    mapInput(m_actionInputs, hotkeys.saveStates.load1);
    mapInput(m_actionInputs, hotkeys.saveStates.load2);
    mapInput(m_actionInputs, hotkeys.saveStates.load3);
    mapInput(m_actionInputs, hotkeys.saveStates.load4);
    mapInput(m_actionInputs, hotkeys.saveStates.load5);
    mapInput(m_actionInputs, hotkeys.saveStates.load6);
    mapInput(m_actionInputs, hotkeys.saveStates.load7);
    mapInput(m_actionInputs, hotkeys.saveStates.load8);
    mapInput(m_actionInputs, hotkeys.saveStates.load9);
    mapInput(m_actionInputs, hotkeys.saveStates.load10);

    mapInput(m_actionInputs, hotkeys.saveStates.save1);
    mapInput(m_actionInputs, hotkeys.saveStates.save2);
    mapInput(m_actionInputs, hotkeys.saveStates.save3);
    mapInput(m_actionInputs, hotkeys.saveStates.save4);
    mapInput(m_actionInputs, hotkeys.saveStates.save5);
    mapInput(m_actionInputs, hotkeys.saveStates.save6);
    mapInput(m_actionInputs, hotkeys.saveStates.save7);
    mapInput(m_actionInputs, hotkeys.saveStates.save8);
    mapInput(m_actionInputs, hotkeys.saveStates.save9);
    mapInput(m_actionInputs, hotkeys.saveStates.save10);

    // Saturn Control Pad on port 1
    mapInput(m_port1ControlPadInputs, input.port1.controlPadBinds.a);
    mapInput(m_port1ControlPadInputs, input.port1.controlPadBinds.b);
    mapInput(m_port1ControlPadInputs, input.port1.controlPadBinds.c);
    mapInput(m_port1ControlPadInputs, input.port1.controlPadBinds.x);
    mapInput(m_port1ControlPadInputs, input.port1.controlPadBinds.y);
    mapInput(m_port1ControlPadInputs, input.port1.controlPadBinds.z);
    mapInput(m_port1ControlPadInputs, input.port1.controlPadBinds.l);
    mapInput(m_port1ControlPadInputs, input.port1.controlPadBinds.r);
    mapInput(m_port1ControlPadInputs, input.port1.controlPadBinds.start);
    mapInput(m_port1ControlPadInputs, input.port1.controlPadBinds.up);
    mapInput(m_port1ControlPadInputs, input.port1.controlPadBinds.down);
    mapInput(m_port1ControlPadInputs, input.port1.controlPadBinds.left);
    mapInput(m_port1ControlPadInputs, input.port1.controlPadBinds.right);
    mapInput(m_port1ControlPadInputs, input.port1.controlPadBinds.dpad);

    // Saturn Control Pad on port 2
    mapInput(m_port2ControlPadInputs, input.port2.controlPadBinds.a);
    mapInput(m_port2ControlPadInputs, input.port2.controlPadBinds.b);
    mapInput(m_port2ControlPadInputs, input.port2.controlPadBinds.c);
    mapInput(m_port2ControlPadInputs, input.port2.controlPadBinds.x);
    mapInput(m_port2ControlPadInputs, input.port2.controlPadBinds.y);
    mapInput(m_port2ControlPadInputs, input.port2.controlPadBinds.z);
    mapInput(m_port2ControlPadInputs, input.port2.controlPadBinds.l);
    mapInput(m_port2ControlPadInputs, input.port2.controlPadBinds.r);
    mapInput(m_port2ControlPadInputs, input.port2.controlPadBinds.start);
    mapInput(m_port2ControlPadInputs, input.port2.controlPadBinds.up);
    mapInput(m_port2ControlPadInputs, input.port2.controlPadBinds.down);
    mapInput(m_port2ControlPadInputs, input.port2.controlPadBinds.left);
    mapInput(m_port2ControlPadInputs, input.port2.controlPadBinds.right);
    mapInput(m_port2ControlPadInputs, input.port2.controlPadBinds.dpad);

    // Saturn 3D Control Pad on port 1
    mapInput(m_port1AnalogPadInputs, input.port1.analogPadBinds.a);
    mapInput(m_port1AnalogPadInputs, input.port1.analogPadBinds.b);
    mapInput(m_port1AnalogPadInputs, input.port1.analogPadBinds.c);
    mapInput(m_port1AnalogPadInputs, input.port1.analogPadBinds.x);
    mapInput(m_port1AnalogPadInputs, input.port1.analogPadBinds.y);
    mapInput(m_port1AnalogPadInputs, input.port1.analogPadBinds.z);
    mapInput(m_port1AnalogPadInputs, input.port1.analogPadBinds.l);
    mapInput(m_port1AnalogPadInputs, input.port1.analogPadBinds.r);
    mapInput(m_port1AnalogPadInputs, input.port1.analogPadBinds.start);
    mapInput(m_port1AnalogPadInputs, input.port1.analogPadBinds.up);
    mapInput(m_port1AnalogPadInputs, input.port1.analogPadBinds.down);
    mapInput(m_port1AnalogPadInputs, input.port1.analogPadBinds.left);
    mapInput(m_port1AnalogPadInputs, input.port1.analogPadBinds.right);
    mapInput(m_port1AnalogPadInputs, input.port1.analogPadBinds.dpad);
    mapInput(m_port1AnalogPadInputs, input.port1.analogPadBinds.analogStick);
    mapInput(m_port1AnalogPadInputs, input.port1.analogPadBinds.analogL);
    mapInput(m_port1AnalogPadInputs, input.port1.analogPadBinds.analogR);
    mapInput(m_port1AnalogPadInputs, input.port1.analogPadBinds.switchMode);

    // Saturn 3D Control Pad on port 2
    mapInput(m_port2AnalogPadInputs, input.port2.analogPadBinds.a);
    mapInput(m_port2AnalogPadInputs, input.port2.analogPadBinds.b);
    mapInput(m_port2AnalogPadInputs, input.port2.analogPadBinds.c);
    mapInput(m_port2AnalogPadInputs, input.port2.analogPadBinds.x);
    mapInput(m_port2AnalogPadInputs, input.port2.analogPadBinds.y);
    mapInput(m_port2AnalogPadInputs, input.port2.analogPadBinds.z);
    mapInput(m_port2AnalogPadInputs, input.port2.analogPadBinds.l);
    mapInput(m_port2AnalogPadInputs, input.port2.analogPadBinds.r);
    mapInput(m_port2AnalogPadInputs, input.port2.analogPadBinds.start);
    mapInput(m_port2AnalogPadInputs, input.port2.analogPadBinds.up);
    mapInput(m_port2AnalogPadInputs, input.port2.analogPadBinds.down);
    mapInput(m_port2AnalogPadInputs, input.port2.analogPadBinds.left);
    mapInput(m_port2AnalogPadInputs, input.port2.analogPadBinds.right);
    mapInput(m_port2AnalogPadInputs, input.port2.analogPadBinds.dpad);
    mapInput(m_port2AnalogPadInputs, input.port2.analogPadBinds.analogStick);
    mapInput(m_port2AnalogPadInputs, input.port2.analogPadBinds.analogL);
    mapInput(m_port2AnalogPadInputs, input.port2.analogPadBinds.analogR);
    mapInput(m_port2AnalogPadInputs, input.port2.analogPadBinds.switchMode);

    // Arcade Racer on port 1
    mapInput(m_port1ArcadeRacerInputs, input.port1.arcadeRacerBinds.a);
    mapInput(m_port1ArcadeRacerInputs, input.port1.arcadeRacerBinds.b);
    mapInput(m_port1ArcadeRacerInputs, input.port1.arcadeRacerBinds.c);
    mapInput(m_port1ArcadeRacerInputs, input.port1.arcadeRacerBinds.x);
    mapInput(m_port1ArcadeRacerInputs, input.port1.arcadeRacerBinds.y);
    mapInput(m_port1ArcadeRacerInputs, input.port1.arcadeRacerBinds.z);
    mapInput(m_port1ArcadeRacerInputs, input.port1.arcadeRacerBinds.start);
    mapInput(m_port1ArcadeRacerInputs, input.port1.arcadeRacerBinds.up);
    mapInput(m_port1ArcadeRacerInputs, input.port1.arcadeRacerBinds.down);
    mapInput(m_port1ArcadeRacerInputs, input.port1.arcadeRacerBinds.wheelLeft);
    mapInput(m_port1ArcadeRacerInputs, input.port1.arcadeRacerBinds.wheelRight);
    mapInput(m_port1ArcadeRacerInputs, input.port1.arcadeRacerBinds.wheel);

    // Arcade Racer on port 2
    mapInput(m_port2ArcadeRacerInputs, input.port2.arcadeRacerBinds.a);
    mapInput(m_port2ArcadeRacerInputs, input.port2.arcadeRacerBinds.b);
    mapInput(m_port2ArcadeRacerInputs, input.port2.arcadeRacerBinds.c);
    mapInput(m_port2ArcadeRacerInputs, input.port2.arcadeRacerBinds.x);
    mapInput(m_port2ArcadeRacerInputs, input.port2.arcadeRacerBinds.y);
    mapInput(m_port2ArcadeRacerInputs, input.port2.arcadeRacerBinds.z);
    mapInput(m_port2ArcadeRacerInputs, input.port2.arcadeRacerBinds.start);
    mapInput(m_port2ArcadeRacerInputs, input.port2.arcadeRacerBinds.up);
    mapInput(m_port2ArcadeRacerInputs, input.port2.arcadeRacerBinds.down);
    mapInput(m_port2ArcadeRacerInputs, input.port2.arcadeRacerBinds.wheelLeft);
    mapInput(m_port2ArcadeRacerInputs, input.port2.arcadeRacerBinds.wheelRight);
    mapInput(m_port2ArcadeRacerInputs, input.port2.arcadeRacerBinds.wheel);

    ResetToDefaults();
}

void Settings::ResetToDefaults() {
    general.preloadDiscImagesToRAM = false;
    general.boostEmuThreadPriority = true;
    general.boostProcessPriority = true;
    general.enableRewindBuffer = false;
    general.rewindCompressionLevel = 12;
    general.mainSpeedFactor = 1.0;
    general.altSpeedFactor = 0.5;
    general.useAltSpeed = false;
    general.pauseWhenUnfocused = false;

    gui.overrideUIScale = false;
    gui.uiScale = 1.0;
    gui.rememberWindowGeometry = true;
    gui.showMessages = true;
    gui.showFrameRateOSD = false;
    gui.frameRateOSDPosition = GUI::FrameRateOSDPosition::TopRight;
    gui.showSpeedIndicatorForAllSpeeds = false;

    system.internalBackupRAMImagePath = m_context.profile.GetPath(ProfilePath::PersistentState) / "bup-int.bin";
    system.internalBackupRAMPerGame = false;

    system.ipl.overrideImage = false;
    system.ipl.path = "";
    system.ipl.variant = db::SystemVariant::Saturn;

    {
        using PeriphType = peripheral::PeripheralType;
        input.port1.type = PeriphType::ControlPad;
        input.port2.type = PeriphType::None;

        (void)ResetHotkeys();

        (void)ResetBinds(input.port1.controlPadBinds, true);
        (void)ResetBinds(input.port2.controlPadBinds, true);

        (void)ResetBinds(input.port1.analogPadBinds, true);
        (void)ResetBinds(input.port2.analogPadBinds, true);

        (void)ResetBinds(input.port1.arcadeRacerBinds, true);
        (void)ResetBinds(input.port2.arcadeRacerBinds, true);
    }

    input.gamepad.lsDeadzone = 0.15f;
    input.gamepad.rsDeadzone = 0.15f;
    input.gamepad.analogToDigitalSensitivity = 0.20f;

    video.forceIntegerScaling = false;
    video.forceAspectRatio = true;
    video.forcedAspect = 4.0 / 3.0;
    video.rotation = Video::DisplayRotation::Normal;
    video.autoResizeWindow = false;
    video.displayVideoOutputInWindow = false;
    video.fullScreen = false;
    video.doubleClickToFullScreen = false;
    video.deinterlace = false;
    video.transparentMeshes = false;

    audio.volume = 0.8;
    audio.mute = false;

    audio.stepGranularity = 0;

    audio.midiInputPort = Settings::Audio::MidiPort{.id = {}, .type = Settings::Audio::MidiPort::Type::None};
    audio.midiOutputPort = Settings::Audio::MidiPort{.id = {}, .type = Settings::Audio::MidiPort::Type::None};

    cartridge.type = Settings::Cartridge::Type::None;
    cartridge.backupRAM.imagePath = "";
    cartridge.backupRAM.capacity = Settings::Cartridge::BackupRAM::Capacity::_32Mbit;
    cartridge.dram.capacity = Settings::Cartridge::DRAM::Capacity::_32Mbit;
    cartridge.rom.imagePath = "";
    cartridge.autoLoadGameCarts = true;
}

SettingsLoadResult Settings::Load(const std::filesystem::path &path) {
    // Use defaults if configuration file does not exist
    if (!std::filesystem::is_regular_file(path)) {
        ResetToDefaults();
        this->path = path;
        return SettingsLoadResult::Success();
    }

    auto parseResult = toml::parse_file(path.native());
    if (parseResult.failed()) {
        return SettingsLoadResult::TOMLParseError(parseResult.error());
    }
    auto &data = parseResult.table();

    ResetToDefaults();

    int configVersion = 0;
    if (auto opt = data["ConfigVersion"].value<int>()) {
        configVersion = *opt;
    }

    SettingsLoadResult result{};
    if (configVersion > kConfigVersion) {
        return SettingsLoadResult::UnsupportedConfigVersion(configVersion);
    }
    auto &emuConfig = m_context.saturn.configuration;

    auto parseUIScaleOptions = [&](auto &tbl) {
        Parse(tbl, "OverrideUIScale", gui.overrideUIScale);

        // Round scale to steps of 25% and clamp to 100%-200% range
        double uiScale = gui.uiScale;
        Parse(tbl, "UIScale", uiScale);
        uiScale = std::round(uiScale / 0.25) * 0.25;
        uiScale = std::clamp(uiScale, 1.0, 2.0);
        gui.uiScale = uiScale;
    };

    if (auto tblGeneral = data["General"]) {
        Parse(tblGeneral, "PreloadDiscImagesToRAM", general.preloadDiscImagesToRAM);
        Parse(tblGeneral, "BoostEmuThreadPriority", general.boostEmuThreadPriority);
        Parse(tblGeneral, "BoostProcessPriority", general.boostProcessPriority);
        Parse(tblGeneral, "EnableRewindBuffer", general.enableRewindBuffer);
        Parse(tblGeneral, "RewindCompressionLevel", general.rewindCompressionLevel);
        Parse(tblGeneral, "MainSpeedFactor", general.mainSpeedFactor);
        Parse(tblGeneral, "AltSpeedFactor", general.altSpeedFactor);
        Parse(tblGeneral, "UseAltSpeed", general.useAltSpeed);
        Parse(tblGeneral, "PauseWhenUnfocused", general.pauseWhenUnfocused);

        // Rounds to the nearest multiple of 5% and clamps to 10%..500% range.
        auto adjustSpeed = [](double value) { return std::clamp(util::RoundToMultiple(value, 0.05), 0.1, 5.0); };
        general.mainSpeedFactor = adjustSpeed(general.mainSpeedFactor);
        general.altSpeedFactor = adjustSpeed(general.altSpeedFactor);

        if (auto tblPathOverrides = tblGeneral["PathOverrides"]) {
            auto parse = [&](const char *name, ProfilePath pathType) {
                std::filesystem::path path{};
                Parse(tblPathOverrides, name, path);
                m_context.profile.SetPathOverride(pathType, path);
            };

            parse("IPLROMImages", ProfilePath::IPLROMImages);
            parse("ROMCartImages", ProfilePath::ROMCartImages);
            parse("BackupMemory", ProfilePath::BackupMemory);
            parse("ExportedBackups", ProfilePath::ExportedBackups);
            parse("PersistentState", ProfilePath::PersistentState);
            parse("SaveStates", ProfilePath::SaveStates);
            parse("Dumps", ProfilePath::Dumps);
        }
    }

    if (auto tblGUI = data["GUI"]) {
        if (configVersion >= 3) {
            parseUIScaleOptions(tblGUI);
        }
        Parse(tblGUI, "RememberWindowGeometry", gui.rememberWindowGeometry);
        Parse(tblGUI, "ShowMessages", gui.showMessages);
        Parse(tblGUI, "ShowFrameRateOSD", gui.showFrameRateOSD);
        Parse(tblGUI, "FrameRateOSDPosition", gui.frameRateOSDPosition);
        Parse(tblGUI, "ShowSpeedIndicatorForAllSpeeds", gui.showSpeedIndicatorForAllSpeeds);
    }

    if (auto tblSystem = data["System"]) {
        Parse(tblSystem, "VideoStandard", emuConfig.system.videoStandard);
        Parse(tblSystem, "AutoDetectRegion", emuConfig.system.autodetectRegion);
        Parse(tblSystem, "PreferredRegionOrder", emuConfig.system.preferredRegionOrder);
        Parse(tblSystem, "EmulateSH2Cache", emuConfig.system.emulateSH2Cache);
        Parse(tblSystem, "InternalBackupRAMImagePath", system.internalBackupRAMImagePath);
        Parse(tblSystem, "InternalBackupRAMPerGame", system.internalBackupRAMPerGame);
        system.internalBackupRAMImagePath = Absolute(ProfilePath::PersistentState, system.internalBackupRAMImagePath);

        auto &ipl = system.ipl;
        if (auto tblIPL = tblSystem["IPL"]) {
            Parse(tblIPL, "Override", ipl.overrideImage);
            Parse(tblIPL, "Path", ipl.path);
            Parse(tblIPL, "Variant", ipl.variant);
            ipl.path = Absolute(ProfilePath::IPLROMImages, ipl.path);
        }

        auto &rtc = emuConfig.rtc;
        if (auto tblRTC = tblSystem["RTC"]) {
            Parse(tblRTC, "Mode", rtc.mode);
            if (auto tblVirtual = tblRTC["Virtual"]) {
                Parse(tblRTC, "HardResetStrategy", rtc.virtHardResetStrategy);
                Parse(tblRTC, "HardResetTimestamp", rtc.virtHardResetTimestamp);
            }
        }
    }

    if (auto tblHotkeys = data["Hotkeys"]) {
        Parse(tblHotkeys, "OpenSettings", hotkeys.openSettings);
        Parse(tblHotkeys, "ToggleWindowedVideoOutput", hotkeys.toggleWindowedVideoOutput);
        Parse(tblHotkeys, "ToggleFullScreen", hotkeys.toggleFullScreen);
        Parse(tblHotkeys, "ToggleFrameRateOSD", hotkeys.toggleFrameRateOSD);
        Parse(tblHotkeys, "NextFrameRateOSDPosition", hotkeys.nextFrameRateOSDPos);
        Parse(tblHotkeys, "PreviousFrameRateOSDPosition", hotkeys.prevFrameRateOSDPos);

        Parse(tblHotkeys, "ToggleMute", hotkeys.toggleMute);
        Parse(tblHotkeys, "IncreaseVolume", hotkeys.increaseVolume);
        Parse(tblHotkeys, "DecreaseVolume", hotkeys.decreaseVolume);

        Parse(tblHotkeys, "LoadDisc", hotkeys.loadDisc);
        Parse(tblHotkeys, "EjectDisc", hotkeys.ejectDisc);
        Parse(tblHotkeys, "OpenCloseTray", hotkeys.openCloseTray);

        Parse(tblHotkeys, "HardReset", hotkeys.hardReset);
        Parse(tblHotkeys, "SoftReset", hotkeys.softReset);
        Parse(tblHotkeys, "ResetButton", hotkeys.resetButton);

        Parse(tblHotkeys, "TurboSpeed", hotkeys.turboSpeed);
        Parse(tblHotkeys, "TurboSpeedHold", hotkeys.turboSpeedHold);
        Parse(tblHotkeys, "ToggleAlternateSpeed", hotkeys.toggleAlternateSpeed);
        Parse(tblHotkeys, "IncreaseSpeed", hotkeys.increaseSpeed);
        Parse(tblHotkeys, "DecreaseSpeed", hotkeys.decreaseSpeed);
        Parse(tblHotkeys, "ResetSpeed", hotkeys.resetSpeed);
        Parse(tblHotkeys, "PauseResume", hotkeys.pauseResume);
        Parse(tblHotkeys, "ForwardFrameStep", hotkeys.fwdFrameStep);
        Parse(tblHotkeys, "ReverseFrameStep", hotkeys.revFrameStep);
        Parse(tblHotkeys, "Rewind", hotkeys.rewind);
        Parse(tblHotkeys, "ToggleRewindBuffer", hotkeys.toggleRewindBuffer);

        Parse(tblHotkeys, "ToggleDebugTrace", hotkeys.toggleDebugTrace);
        Parse(tblHotkeys, "DumpMemory", hotkeys.dumpMemory);

        if (auto tblSaveStates = tblHotkeys["SaveStates"]) {
            Parse(tblSaveStates, "QuickLoadState", hotkeys.saveStates.quickLoad);
            Parse(tblSaveStates, "QuickSaveState", hotkeys.saveStates.quickSave);

            Parse(tblSaveStates, "SelectState1", hotkeys.saveStates.select1);
            Parse(tblSaveStates, "SelectState2", hotkeys.saveStates.select2);
            Parse(tblSaveStates, "SelectState3", hotkeys.saveStates.select3);
            Parse(tblSaveStates, "SelectState4", hotkeys.saveStates.select4);
            Parse(tblSaveStates, "SelectState5", hotkeys.saveStates.select5);
            Parse(tblSaveStates, "SelectState6", hotkeys.saveStates.select6);
            Parse(tblSaveStates, "SelectState7", hotkeys.saveStates.select7);
            Parse(tblSaveStates, "SelectState8", hotkeys.saveStates.select8);
            Parse(tblSaveStates, "SelectState9", hotkeys.saveStates.select9);
            Parse(tblSaveStates, "SelectState10", hotkeys.saveStates.select10);

            Parse(tblSaveStates, "LoadState1", hotkeys.saveStates.load1);
            Parse(tblSaveStates, "LoadState2", hotkeys.saveStates.load2);
            Parse(tblSaveStates, "LoadState3", hotkeys.saveStates.load3);
            Parse(tblSaveStates, "LoadState4", hotkeys.saveStates.load4);
            Parse(tblSaveStates, "LoadState5", hotkeys.saveStates.load5);
            Parse(tblSaveStates, "LoadState6", hotkeys.saveStates.load6);
            Parse(tblSaveStates, "LoadState7", hotkeys.saveStates.load7);
            Parse(tblSaveStates, "LoadState8", hotkeys.saveStates.load8);
            Parse(tblSaveStates, "LoadState9", hotkeys.saveStates.load9);
            Parse(tblSaveStates, "LoadState10", hotkeys.saveStates.load10);

            Parse(tblSaveStates, "SaveState1", hotkeys.saveStates.save1);
            Parse(tblSaveStates, "SaveState2", hotkeys.saveStates.save2);
            Parse(tblSaveStates, "SaveState3", hotkeys.saveStates.save3);
            Parse(tblSaveStates, "SaveState4", hotkeys.saveStates.save4);
            Parse(tblSaveStates, "SaveState5", hotkeys.saveStates.save5);
            Parse(tblSaveStates, "SaveState6", hotkeys.saveStates.save6);
            Parse(tblSaveStates, "SaveState7", hotkeys.saveStates.save7);
            Parse(tblSaveStates, "SaveState8", hotkeys.saveStates.save8);
            Parse(tblSaveStates, "SaveState9", hotkeys.saveStates.save9);
            Parse(tblSaveStates, "SaveState10", hotkeys.saveStates.save10);
        }
    }

    if (auto tblInput = data["Input"]) {
        auto parsePort = [&](const char *name, Input::Port &portSettings) {
            if (auto tblPort = tblInput[name]) {
                Parse(tblPort, "PeripheralType", portSettings.type);

                const char *controlPadName = configVersion == 1 ? "StandardPadBinds" : "ControlPadBinds";
                if (auto tblBinds = tblPort[controlPadName]) {
                    Parse(tblBinds, "A", portSettings.controlPadBinds.a);
                    Parse(tblBinds, "B", portSettings.controlPadBinds.b);
                    Parse(tblBinds, "C", portSettings.controlPadBinds.c);
                    Parse(tblBinds, "X", portSettings.controlPadBinds.x);
                    Parse(tblBinds, "Y", portSettings.controlPadBinds.y);
                    Parse(tblBinds, "Z", portSettings.controlPadBinds.z);
                    Parse(tblBinds, "L", portSettings.controlPadBinds.l);
                    Parse(tblBinds, "R", portSettings.controlPadBinds.r);
                    Parse(tblBinds, "Start", portSettings.controlPadBinds.start);
                    Parse(tblBinds, "Up", portSettings.controlPadBinds.up);
                    Parse(tblBinds, "Down", portSettings.controlPadBinds.down);
                    Parse(tblBinds, "Left", portSettings.controlPadBinds.left);
                    Parse(tblBinds, "Right", portSettings.controlPadBinds.right);
                    Parse(tblBinds, "DPad", portSettings.controlPadBinds.dpad);
                }
                if (auto tblBinds = tblPort["AnalogPadBinds"]) {
                    Parse(tblBinds, "A", portSettings.analogPadBinds.a);
                    Parse(tblBinds, "B", portSettings.analogPadBinds.b);
                    Parse(tblBinds, "C", portSettings.analogPadBinds.c);
                    Parse(tblBinds, "X", portSettings.analogPadBinds.x);
                    Parse(tblBinds, "Y", portSettings.analogPadBinds.y);
                    Parse(tblBinds, "Z", portSettings.analogPadBinds.z);
                    Parse(tblBinds, "L", portSettings.analogPadBinds.l);
                    Parse(tblBinds, "R", portSettings.analogPadBinds.r);
                    Parse(tblBinds, "Start", portSettings.analogPadBinds.start);
                    Parse(tblBinds, "Up", portSettings.analogPadBinds.up);
                    Parse(tblBinds, "Down", portSettings.analogPadBinds.down);
                    Parse(tblBinds, "Left", portSettings.analogPadBinds.left);
                    Parse(tblBinds, "Right", portSettings.analogPadBinds.right);
                    Parse(tblBinds, "DPad", portSettings.analogPadBinds.dpad);
                    Parse(tblBinds, "AnalogStick", portSettings.analogPadBinds.analogStick);
                    Parse(tblBinds, "AnalogL", portSettings.analogPadBinds.analogL);
                    Parse(tblBinds, "AnalogR", portSettings.analogPadBinds.analogR);
                    Parse(tblBinds, "SwitchMode", portSettings.analogPadBinds.switchMode);
                }
                if (auto tblBinds = tblPort["ArcadeRacerBinds"]) {
                    Parse(tblBinds, "A", portSettings.arcadeRacerBinds.a);
                    Parse(tblBinds, "B", portSettings.arcadeRacerBinds.b);
                    Parse(tblBinds, "C", portSettings.arcadeRacerBinds.c);
                    Parse(tblBinds, "X", portSettings.arcadeRacerBinds.x);
                    Parse(tblBinds, "Y", portSettings.arcadeRacerBinds.y);
                    Parse(tblBinds, "Z", portSettings.arcadeRacerBinds.z);
                    Parse(tblBinds, "Start", portSettings.arcadeRacerBinds.start);
                    Parse(tblBinds, "Up", portSettings.arcadeRacerBinds.up);
                    Parse(tblBinds, "Down", portSettings.arcadeRacerBinds.down);
                    Parse(tblBinds, "WheelLeft", portSettings.arcadeRacerBinds.wheelLeft);
                    Parse(tblBinds, "WheelRight", portSettings.arcadeRacerBinds.wheelRight);
                    Parse(tblBinds, "AnalogWheel", portSettings.arcadeRacerBinds.wheel);
                }
            }
        };
        parsePort("Port1", input.port1);
        parsePort("Port2", input.port2);

        Parse(tblInput, "GamepadLSDeadzone", input.gamepad.lsDeadzone);
        Parse(tblInput, "GamepadRSDeadzone", input.gamepad.rsDeadzone);
        Parse(tblInput, "GamepadAnalogToDigitalSensitivity", input.gamepad.analogToDigitalSensitivity);
    }

    if (auto tblVideo = data["Video"]) {
        Parse(tblVideo, "ForceIntegerScaling", video.forceIntegerScaling);
        Parse(tblVideo, "ForceAspectRatio", video.forceAspectRatio);
        Parse(tblVideo, "ForcedAspect", video.forcedAspect);
        Parse(tblVideo, "Rotation", video.rotation);

        Parse(tblVideo, "AutoResizeWindow", video.autoResizeWindow);
        Parse(tblVideo, "DisplayVideoOutputInWindow", video.displayVideoOutputInWindow);
        Parse(tblVideo, "FullScreen", video.fullScreen);
        Parse(tblVideo, "DoubleClickToFullScreen", video.doubleClickToFullScreen);

        Parse(tblVideo, "ThreadedVDP", emuConfig.video.threadedVDP);
        Parse(tblVideo, "ThreadedDeinterlacer", emuConfig.video.threadedDeinterlacer);
        Parse(tblVideo, "IncludeVDP1InRenderThread", emuConfig.video.includeVDP1InRenderThread);
        if (configVersion <= 2) {
            parseUIScaleOptions(tblVideo);
        }

        Parse(tblVideo, "Deinterlace", video.deinterlace);
        Parse(tblVideo, "TransparentMeshes", video.transparentMeshes);
    }

    if (auto tblAudio = data["Audio"]) {
        auto inputPort = audio.midiInputPort.Get();
        auto outputPort = audio.midiOutputPort.Get();
        auto stepGranularity = audio.stepGranularity.Get();

        Parse(tblAudio, "Volume", audio.volume);
        Parse(tblAudio, "Mute", audio.mute);

        Parse(tblAudio, "StepGranularity", stepGranularity);

        Parse(tblAudio, "MidiInputPortId", inputPort.id);
        Parse(tblAudio, "MidiOutputPortId", outputPort.id);
        Parse(tblAudio, "MidiInputPortType", inputPort.type);
        Parse(tblAudio, "MidiOutputPortType", outputPort.type);
        Parse(tblAudio, "InterpolationMode", emuConfig.audio.interpolation);
        Parse(tblAudio, "ThreadedSCSP", emuConfig.audio.threadedSCSP);

        audio.stepGranularity = std::min(stepGranularity, 5u);

        audio.midiInputPort = inputPort;
        audio.midiOutputPort = outputPort;
    }

    if (auto tblCart = data["Cartridge"]) {
        Parse(tblCart, "Type", cartridge.type);
        if (auto tblBackupRAM = tblCart["BackupRAM"]) {
            Parse(tblBackupRAM, "ImagePath", cartridge.backupRAM.imagePath);
            Parse(tblBackupRAM, "Capacity", cartridge.backupRAM.capacity);
            cartridge.backupRAM.imagePath = Absolute(ProfilePath::PersistentState, cartridge.backupRAM.imagePath);
        }
        if (auto tblDRAM = tblCart["DRAM"]) {
            Parse(tblDRAM, "Capacity", cartridge.dram.capacity);
        }
        if (auto tblROM = tblCart["ROM"]) {
            Parse(tblROM, "ImagePath", cartridge.rom.imagePath);
            cartridge.rom.imagePath = Absolute(ProfilePath::ROMCartImages, cartridge.rom.imagePath);
        }
        Parse(tblCart, "AutoLoadGameCarts", cartridge.autoLoadGameCarts);
    }

    if (auto tblCDBlock = data["CDBlock"]) {
        Parse(tblCDBlock, "ReadSpeed", emuConfig.cdblock.readSpeedFactor);
    }

    this->path = path;
    return SettingsLoadResult::Success();
}

SettingsSaveResult Settings::Save() {
    if (path.empty()) {
        path = "Ymir.toml";
    }

    auto &emuConfig = m_context.saturn.configuration;
    const auto &rtc = emuConfig.rtc;

    // clang-format off
    auto tbl = toml::table{{
        {"ConfigVersion", kConfigVersion},

        {"General", toml::table{{
            {"PreloadDiscImagesToRAM", general.preloadDiscImagesToRAM},
            {"BoostEmuThreadPriority", general.boostEmuThreadPriority},
            {"BoostProcessPriority", general.boostProcessPriority},
            {"EnableRewindBuffer", general.enableRewindBuffer},
            {"RewindCompressionLevel", general.rewindCompressionLevel},
            {"MainSpeedFactor", general.mainSpeedFactor.Get()},
            {"AltSpeedFactor", general.altSpeedFactor.Get()},
            {"UseAltSpeed", general.useAltSpeed.Get()},
            {"PauseWhenUnfocused", general.pauseWhenUnfocused},

            {"PathOverrides", toml::table{{
                {"IPLROMImages", m_context.profile.GetPathOverride(ProfilePath::IPLROMImages).native()},
                {"ROMCartImages", m_context.profile.GetPathOverride(ProfilePath::ROMCartImages).native()},
                {"BackupMemory", m_context.profile.GetPathOverride(ProfilePath::BackupMemory).native()},
                {"ExportedBackups", m_context.profile.GetPathOverride(ProfilePath::ExportedBackups).native()},
                {"PersistentState", m_context.profile.GetPathOverride(ProfilePath::PersistentState).native()},
                {"SaveStates", m_context.profile.GetPathOverride(ProfilePath::SaveStates).native()},
                {"Dumps", m_context.profile.GetPathOverride(ProfilePath::Dumps).native()},
            }}},
        }}},

        {"GUI", toml::table{{
            {"OverrideUIScale", gui.overrideUIScale.Get()},
            {"UIScale", gui.uiScale.Get()},
            {"RememberWindowGeometry", gui.rememberWindowGeometry},
            {"ShowMessages", gui.showMessages},
            {"ShowFrameRateOSD", gui.showFrameRateOSD},
            {"FrameRateOSDPosition", ToTOML(gui.frameRateOSDPosition)},
            {"ShowSpeedIndicatorForAllSpeeds", gui.showSpeedIndicatorForAllSpeeds},
        }}},

        {"System", toml::table{{
            {"VideoStandard", ToTOML(emuConfig.system.videoStandard)},
            {"AutoDetectRegion", emuConfig.system.autodetectRegion},
            {"PreferredRegionOrder", ToTOML(emuConfig.system.preferredRegionOrder.Get())},
            {"EmulateSH2Cache", emuConfig.system.emulateSH2Cache.Get()},
            {"InternalBackupRAMImagePath", Proximate(ProfilePath::PersistentState, system.internalBackupRAMImagePath).native()},
            {"InternalBackupRAMPerGame", system.internalBackupRAMPerGame},
        
            {"IPL", toml::table{{
                {"Override", system.ipl.overrideImage},
                {"Path", Proximate(ProfilePath::IPLROMImages, system.ipl.path).native()},
                {"Variant", ToTOML(system.ipl.variant)},
            }}},

            {"RTC", toml::table{{
                {"Mode", ToTOML(rtc.mode)},
                {"Virtual", toml::table{{
                    {"HardResetStrategy", ToTOML(rtc.virtHardResetStrategy)},
                    {"HardResetTimestamp", rtc.virtHardResetTimestamp},
                }}},
            }}},
        }}},

        {"Hotkeys", toml::table{{
            {"OpenSettings", ToTOML(hotkeys.openSettings)},
            {"ToggleWindowedVideoOutput", ToTOML(hotkeys.toggleWindowedVideoOutput)},
            {"ToggleFullScreen", ToTOML(hotkeys.toggleFullScreen)},
            {"ToggleFrameRateOSD", ToTOML(hotkeys.toggleFrameRateOSD)},
            {"NextFrameRateOSDPosition", ToTOML(hotkeys.nextFrameRateOSDPos)},
            {"PreviousFrameRateOSDPosition", ToTOML(hotkeys.prevFrameRateOSDPos)},
            
            {"ToggleMute", ToTOML(hotkeys.toggleMute)},
            {"IncreaseVolume", ToTOML(hotkeys.increaseVolume)},
            {"DecreaseVolume", ToTOML(hotkeys.decreaseVolume)},

            {"LoadDisc", ToTOML(hotkeys.loadDisc)},
            {"EjectDisc", ToTOML(hotkeys.ejectDisc)},
            {"OpenCloseTray", ToTOML(hotkeys.openCloseTray)},

            {"HardReset", ToTOML(hotkeys.hardReset)},
            {"SoftReset", ToTOML(hotkeys.softReset)},
            {"ResetButton", ToTOML(hotkeys.resetButton)},

            {"TurboSpeed", ToTOML(hotkeys.turboSpeed)},
            {"TurboSpeedHold", ToTOML(hotkeys.turboSpeedHold)},
            {"ToggleAlternateSpeed", ToTOML(hotkeys.toggleAlternateSpeed)},
            {"IncreaseSpeed", ToTOML(hotkeys.increaseSpeed)},
            {"DecreaseSpeed", ToTOML(hotkeys.decreaseSpeed)},
            {"IncreaseSpeedLarge", ToTOML(hotkeys.increaseSpeedLarge)},
            {"DecreaseSpeedLarge", ToTOML(hotkeys.decreaseSpeedLarge)},
            {"ResetSpeed", ToTOML(hotkeys.resetSpeed)},
            {"PauseResume", ToTOML(hotkeys.pauseResume)},
            {"ForwardFrameStep", ToTOML(hotkeys.fwdFrameStep)},
            {"ReverseFrameStep", ToTOML(hotkeys.revFrameStep)},
            {"Rewind", ToTOML(hotkeys.rewind)},
            {"ToggleRewindBuffer", ToTOML(hotkeys.toggleRewindBuffer)},

            {"ToggleDebugTrace", ToTOML(hotkeys.toggleDebugTrace)},
            {"DumpMemory", ToTOML(hotkeys.dumpMemory)},

            {"SaveStates", toml::table{{
                {"QuickLoadState", ToTOML(hotkeys.saveStates.quickLoad)},
                {"QuickSaveState", ToTOML(hotkeys.saveStates.quickSave)},

                {"SelectState1", ToTOML(hotkeys.saveStates.select1)},
                {"SelectState2", ToTOML(hotkeys.saveStates.select2)},
                {"SelectState3", ToTOML(hotkeys.saveStates.select3)},
                {"SelectState4", ToTOML(hotkeys.saveStates.select4)},
                {"SelectState5", ToTOML(hotkeys.saveStates.select5)},
                {"SelectState6", ToTOML(hotkeys.saveStates.select6)},
                {"SelectState7", ToTOML(hotkeys.saveStates.select7)},
                {"SelectState8", ToTOML(hotkeys.saveStates.select8)},
                {"SelectState9", ToTOML(hotkeys.saveStates.select9)},
                {"SelectState10", ToTOML(hotkeys.saveStates.select10)},

                {"LoadState1", ToTOML(hotkeys.saveStates.load1)},
                {"LoadState2", ToTOML(hotkeys.saveStates.load2)},
                {"LoadState3", ToTOML(hotkeys.saveStates.load3)},
                {"LoadState4", ToTOML(hotkeys.saveStates.load4)},
                {"LoadState5", ToTOML(hotkeys.saveStates.load5)},
                {"LoadState6", ToTOML(hotkeys.saveStates.load6)},
                {"LoadState7", ToTOML(hotkeys.saveStates.load7)},
                {"LoadState8", ToTOML(hotkeys.saveStates.load8)},
                {"LoadState9", ToTOML(hotkeys.saveStates.load9)},
                {"LoadState10", ToTOML(hotkeys.saveStates.load10)},

                {"SaveState1", ToTOML(hotkeys.saveStates.save1)},
                {"SaveState2", ToTOML(hotkeys.saveStates.save2)},
                {"SaveState3", ToTOML(hotkeys.saveStates.save3)},
                {"SaveState4", ToTOML(hotkeys.saveStates.save4)},
                {"SaveState5", ToTOML(hotkeys.saveStates.save5)},
                {"SaveState6", ToTOML(hotkeys.saveStates.save6)},
                {"SaveState7", ToTOML(hotkeys.saveStates.save7)},
                {"SaveState8", ToTOML(hotkeys.saveStates.save8)},
                {"SaveState9", ToTOML(hotkeys.saveStates.save9)},
                {"SaveState10", ToTOML(hotkeys.saveStates.save10)},
            }}},
        }}},

        {"Input", toml::table{{
            {"Port1", toml::table{{
                {"PeripheralType", ToTOML(input.port1.type)},
                {"ControlPadBinds", toml::table{{
                    {"A", ToTOML(input.port1.controlPadBinds.a)},
                    {"B", ToTOML(input.port1.controlPadBinds.b)},
                    {"C", ToTOML(input.port1.controlPadBinds.c)},
                    {"X", ToTOML(input.port1.controlPadBinds.x)},
                    {"Y", ToTOML(input.port1.controlPadBinds.y)},
                    {"Z", ToTOML(input.port1.controlPadBinds.z)},
                    {"L", ToTOML(input.port1.controlPadBinds.l)},
                    {"R", ToTOML(input.port1.controlPadBinds.r)},
                    {"Start", ToTOML(input.port1.controlPadBinds.start)},
                    {"Up", ToTOML(input.port1.controlPadBinds.up)},
                    {"Down", ToTOML(input.port1.controlPadBinds.down)},
                    {"Left", ToTOML(input.port1.controlPadBinds.left)},
                    {"Right", ToTOML(input.port1.controlPadBinds.right)},
                    {"DPad", ToTOML(input.port1.controlPadBinds.dpad)},
                }}},
                {"AnalogPadBinds", toml::table{{
                    {"A", ToTOML(input.port1.analogPadBinds.a)},
                    {"B", ToTOML(input.port1.analogPadBinds.b)},
                    {"C", ToTOML(input.port1.analogPadBinds.c)},
                    {"X", ToTOML(input.port1.analogPadBinds.x)},
                    {"Y", ToTOML(input.port1.analogPadBinds.y)},
                    {"Z", ToTOML(input.port1.analogPadBinds.z)},
                    {"L", ToTOML(input.port1.analogPadBinds.l)},
                    {"R", ToTOML(input.port1.analogPadBinds.r)},
                    {"Start", ToTOML(input.port1.analogPadBinds.start)},
                    {"Up", ToTOML(input.port1.analogPadBinds.up)},
                    {"Down", ToTOML(input.port1.analogPadBinds.down)},
                    {"Left", ToTOML(input.port1.analogPadBinds.left)},
                    {"Right", ToTOML(input.port1.analogPadBinds.right)},
                    {"DPad", ToTOML(input.port1.analogPadBinds.dpad)},
                    {"AnalogStick", ToTOML(input.port1.analogPadBinds.analogStick)},
                    {"AnalogL", ToTOML(input.port1.analogPadBinds.analogL)},
                    {"AnalogR", ToTOML(input.port1.analogPadBinds.analogR)},
                    {"SwitchMode", ToTOML(input.port1.analogPadBinds.switchMode)},
                }}},
                {"ArcadeRacerBinds", toml::table{{
                    {"A", ToTOML(input.port1.arcadeRacerBinds.a)},
                    {"B", ToTOML(input.port1.arcadeRacerBinds.b)},
                    {"C", ToTOML(input.port1.arcadeRacerBinds.c)},
                    {"X", ToTOML(input.port1.arcadeRacerBinds.x)},
                    {"Y", ToTOML(input.port1.arcadeRacerBinds.y)},
                    {"Z", ToTOML(input.port1.arcadeRacerBinds.z)},
                    {"Start", ToTOML(input.port1.arcadeRacerBinds.start)},
                    {"Up", ToTOML(input.port1.arcadeRacerBinds.up)},
                    {"Down", ToTOML(input.port1.arcadeRacerBinds.down)},
                    {"WheelLeft", ToTOML(input.port1.arcadeRacerBinds.wheelLeft)},
                    {"WheelRight", ToTOML(input.port1.arcadeRacerBinds.wheelRight)},
                    {"Wheel", ToTOML(input.port1.arcadeRacerBinds.wheel)},
                }}},
            }}},
            {"Port2", toml::table{{
                {"PeripheralType", ToTOML(input.port2.type)},
                {"ControlPadBinds", toml::table{{
                    {"A", ToTOML(input.port2.controlPadBinds.a)},
                    {"B", ToTOML(input.port2.controlPadBinds.b)},
                    {"C", ToTOML(input.port2.controlPadBinds.c)},
                    {"X", ToTOML(input.port2.controlPadBinds.x)},
                    {"Y", ToTOML(input.port2.controlPadBinds.y)},
                    {"Z", ToTOML(input.port2.controlPadBinds.z)},
                    {"L", ToTOML(input.port2.controlPadBinds.l)},
                    {"R", ToTOML(input.port2.controlPadBinds.r)},
                    {"Start", ToTOML(input.port2.controlPadBinds.start)},
                    {"Up", ToTOML(input.port2.controlPadBinds.up)},
                    {"Down", ToTOML(input.port2.controlPadBinds.down)},
                    {"Left", ToTOML(input.port2.controlPadBinds.left)},
                    {"Right", ToTOML(input.port2.controlPadBinds.right)},
                    {"DPad", ToTOML(input.port2.controlPadBinds.dpad)},
                }}},
                {"AnalogPadBinds", toml::table{{
                    {"A", ToTOML(input.port2.analogPadBinds.a)},
                    {"B", ToTOML(input.port2.analogPadBinds.b)},
                    {"C", ToTOML(input.port2.analogPadBinds.c)},
                    {"X", ToTOML(input.port2.analogPadBinds.x)},
                    {"Y", ToTOML(input.port2.analogPadBinds.y)},
                    {"Z", ToTOML(input.port2.analogPadBinds.z)},
                    {"L", ToTOML(input.port2.analogPadBinds.l)},
                    {"R", ToTOML(input.port2.analogPadBinds.r)},
                    {"Start", ToTOML(input.port2.analogPadBinds.start)},
                    {"Up", ToTOML(input.port2.analogPadBinds.up)},
                    {"Down", ToTOML(input.port2.analogPadBinds.down)},
                    {"Left", ToTOML(input.port2.analogPadBinds.left)},
                    {"Right", ToTOML(input.port2.analogPadBinds.right)},
                    {"DPad", ToTOML(input.port2.analogPadBinds.dpad)},
                    {"AnalogStick", ToTOML(input.port2.analogPadBinds.analogStick)},
                    {"AnalogL", ToTOML(input.port2.analogPadBinds.analogL)},
                    {"AnalogR", ToTOML(input.port2.analogPadBinds.analogR)},
                    {"SwitchMode", ToTOML(input.port2.analogPadBinds.switchMode)},
                }}},
                {"ArcadeRacerBinds", toml::table{{
                    {"A", ToTOML(input.port2.arcadeRacerBinds.a)},
                    {"B", ToTOML(input.port2.arcadeRacerBinds.b)},
                    {"C", ToTOML(input.port2.arcadeRacerBinds.c)},
                    {"X", ToTOML(input.port2.arcadeRacerBinds.x)},
                    {"Y", ToTOML(input.port2.arcadeRacerBinds.y)},
                    {"Z", ToTOML(input.port2.arcadeRacerBinds.z)},
                    {"Start", ToTOML(input.port2.arcadeRacerBinds.start)},
                    {"Up", ToTOML(input.port2.arcadeRacerBinds.up)},
                    {"Down", ToTOML(input.port2.arcadeRacerBinds.down)},
                    {"WheelLeft", ToTOML(input.port2.arcadeRacerBinds.wheelLeft)},
                    {"WheelRight", ToTOML(input.port2.arcadeRacerBinds.wheelRight)},
                    {"Wheel", ToTOML(input.port2.arcadeRacerBinds.wheel)},
                }}},
            }}},
            {"GamepadLSDeadzone", input.gamepad.lsDeadzone.Get()},
            {"GamepadRSDeadzone", input.gamepad.rsDeadzone.Get()},
            {"GamepadAnalogToDigitalSensitivity", input.gamepad.analogToDigitalSensitivity.Get()},
        }}},

        {"Video", toml::table{{
            {"ForceIntegerScaling", video.forceIntegerScaling},
            {"ForceAspectRatio", video.forceAspectRatio},
            {"ForcedAspect", video.forcedAspect},
            {"Rotation", ToTOML(video.rotation)},
            {"AutoResizeWindow", video.autoResizeWindow},
            {"DisplayVideoOutputInWindow", video.displayVideoOutputInWindow},
            {"FullScreen", video.fullScreen.Get()},
            {"DoubleClickToFullScreen", video.doubleClickToFullScreen},
            {"Deinterlace", video.deinterlace.Get()},
            {"TransparentMeshes", video.transparentMeshes.Get()},
            {"ThreadedVDP", emuConfig.video.threadedVDP.Get()},
            {"ThreadedDeinterlacer", emuConfig.video.threadedDeinterlacer.Get()},
            {"IncludeVDP1InRenderThread", emuConfig.video.includeVDP1InRenderThread.Get()},
        }}},

        {"Audio", toml::table{{
            {"Volume", audio.volume.Get()},
            {"Mute", audio.mute.Get()},
            {"StepGranularity", audio.stepGranularity.Get()},
            {"MidiInputPortId", audio.midiInputPort.Get().id},
            {"MidiOutputPortId", audio.midiOutputPort.Get().id},
            {"MidiInputPortType", ToTOML(audio.midiInputPort.Get().type)},
            {"MidiOutputPortType", ToTOML(audio.midiOutputPort.Get().type)},
            {"InterpolationMode", ToTOML(emuConfig.audio.interpolation)},
            {"ThreadedSCSP", emuConfig.audio.threadedSCSP.Get()},
        }}},

        {"Cartridge", toml::table{{
            {"Type", ToTOML(cartridge.type)},
            {"BackupRAM", toml::table{{
                {"ImagePath", Proximate(ProfilePath::PersistentState, cartridge.backupRAM.imagePath).native()},
                {"Capacity", ToTOML(cartridge.backupRAM.capacity)},
            }}},
            {"DRAM", toml::table{{
                {"Capacity", ToTOML(cartridge.dram.capacity)},
            }}},
            {"ROM", toml::table{{
                {"ImagePath", Proximate(ProfilePath::ROMCartImages, cartridge.rom.imagePath).native()},
            }}},
            {"AutoLoadGameCarts", cartridge.autoLoadGameCarts},
        }}},

        {"CDBlock", toml::table{{
            {"ReadSpeed", emuConfig.cdblock.readSpeedFactor.Get()},
        }}},
    }};
    // clang-format on

    std::ofstream out{path, std::ios::binary | std::ios::trunc};
    out << tbl;
    if (!out) {
        std::error_code error{errno, std::generic_category()};
        return SettingsSaveResult::FilesystemError(error);
    }

    return SettingsSaveResult::Success();
}

void Settings::CheckDirty() {
    using namespace std::chrono_literals;

    if (m_dirty && (std::chrono::steady_clock::now() - m_dirtyTimestamp) > 250ms) {
        if (auto result = Save(); !result) {
            devlog::warn<grp::settings>("Failed to save settings: {}", result.string());
        }
        m_dirty = false;
    }
}

void Settings::MakeDirty() {
    m_dirty = true;
    m_dirtyTimestamp = std::chrono::steady_clock::now();
}

void Settings::RebindInputs() {
    auto &inputContext = m_context.inputContext;
    inputContext.UnmapAllActions();

    auto bindAll = [&](InputMap &map) {
        for (auto &[action, mappings] : map.map) {
            for (auto *bind : mappings) {
                assert(bind != nullptr);
                for (auto &element : bind->elements) {
                    // Sanitize configuration in case the user tampered with the configuration file

                    // Skip ESC binds
                    if (element.type == input::InputElement::Type::KeyCombo &&
                        element.keyCombo.key == input::KeyboardKey::Escape) {
                        continue;
                    }

                    // Match input element types to action kinds
                    switch (action.kind) {
                    case input::Action::Kind::Trigger: [[fallthrough]];
                    case input::Action::Kind::RepeatableTrigger: [[fallthrough]];
                    case input::Action::Kind::Button:
                        if (!element.IsButton()) {
                            continue;
                        }
                        break;
                    case input::Action::Kind::Axis1D:
                        if (!element.IsAxis1D()) {
                            continue;
                        }
                        break;
                    case input::Action::Kind::Axis2D:
                        if (!element.IsAxis2D()) {
                            continue;
                        }
                        break;
                    }

                    (void)inputContext.MapAction(element, action, map.context);
                }
            }
        }
    };

    bindAll(m_actionInputs);

    switch (m_context.settings.input.port1.type) {
    case peripheral::PeripheralType::None: break;
    case peripheral::PeripheralType::ControlPad: bindAll(m_port1ControlPadInputs); break;
    case peripheral::PeripheralType::AnalogPad: bindAll(m_port1AnalogPadInputs); break;
    case peripheral::PeripheralType::ArcadeRacer: bindAll(m_port1ArcadeRacerInputs); break;
    }

    switch (m_context.settings.input.port2.type) {
    case peripheral::PeripheralType::None: break;
    case peripheral::PeripheralType::ControlPad: bindAll(m_port2ControlPadInputs); break;
    case peripheral::PeripheralType::AnalogPad: bindAll(m_port2AnalogPadInputs); break;
    case peripheral::PeripheralType::ArcadeRacer: bindAll(m_port2ArcadeRacerInputs); break;
    }

    SyncInputSettings();
}

std::optional<input::MappedAction> Settings::UnbindInput(const input::InputElement &element) {
    if (element.type == input::InputElement::Type::None) {
        return std::nullopt;
    }

    // Check if there is an action mapped to the input element
    auto &inputContext = m_context.inputContext;
    auto existingAction = inputContext.GetMappedAction(element);
    if (!existingAction) {
        return std::nullopt;
    }

    // In case the action belongs to a controller, make sure it is actually connected before removing the bind
    if (existingAction->context == &m_context.controlPadInputs[0] &&
        m_context.settings.input.port1.type != peripheral::PeripheralType::ControlPad) {
        return std::nullopt;
    } else if (existingAction->context == &m_context.controlPadInputs[1] &&
               m_context.settings.input.port2.type != peripheral::PeripheralType::ControlPad) {
        return std::nullopt;
    } else if (existingAction->context == &m_context.analogPadInputs[0] &&
               m_context.settings.input.port1.type != peripheral::PeripheralType::AnalogPad) {
        return std::nullopt;
    } else if (existingAction->context == &m_context.analogPadInputs[1] &&
               m_context.settings.input.port2.type != peripheral::PeripheralType::AnalogPad) {
        return std::nullopt;
    } else if (existingAction->context == &m_context.arcadeRacerInputs[0] &&
               m_context.settings.input.port1.type != peripheral::PeripheralType::ArcadeRacer) {
        return std::nullopt;
    } else if (existingAction->context == &m_context.arcadeRacerInputs[1] &&
               m_context.settings.input.port2.type != peripheral::PeripheralType::ArcadeRacer) {
        return std::nullopt;
    }

    // Unmap the input from the context.
    // The return value is exactly the same as the one from GetMappedAction above.
    (void)inputContext.UnmapInput(element);

    // Unassign the bind from the settings
    auto &map = GetInputMapForContext(existingAction->context);
    if (map.map.contains(existingAction->action)) {
        auto &inputs = map.map.at(existingAction->action);
        for (auto &inputBind : inputs) {
            assert(inputBind != nullptr);
            if (inputBind->action != existingAction->action) {
                continue;
            }
            for (auto &boundElement : inputBind->elements) {
                if (boundElement == element) {
                    boundElement = {};
                    return existingAction;
                }
            }
        }
    }

    return std::nullopt;
}

void Settings::SyncInputSettings() {
    auto &inputContext = m_context.inputContext;
    auto sync = [&](InputMap &map) {
        for (auto &[action, mappings] : map.map) {
            for (auto *bind : mappings) {
                bind->elements.fill({});
                for (int i = 0; auto &input : inputContext.GetMappedInputs(bind->action)) {
                    if (input.context == map.context) {
                        bind->elements[i++] = input.element;
                        if (i == input::kNumBindsPerInput) {
                            break;
                        }
                    }
                }
            }
        }
    };

    sync(m_actionInputs);

    switch (m_context.settings.input.port1.type) {
    case peripheral::PeripheralType::None: break;
    case peripheral::PeripheralType::ControlPad: sync(m_port1ControlPadInputs); break;
    case peripheral::PeripheralType::AnalogPad: sync(m_port1AnalogPadInputs); break;
    case peripheral::PeripheralType::ArcadeRacer: sync(m_port1ArcadeRacerInputs); break;
    }

    switch (m_context.settings.input.port2.type) {
    case peripheral::PeripheralType::None: break;
    case peripheral::PeripheralType::ControlPad: sync(m_port2ControlPadInputs); break;
    case peripheral::PeripheralType::AnalogPad: sync(m_port2AnalogPadInputs); break;
    case peripheral::PeripheralType::ArcadeRacer: sync(m_port2ArcadeRacerInputs); break;
    }
}

std::unordered_set<input::MappedAction> Settings::ResetHotkeys() {
    using namespace input;

    using Mod = KeyModifier;
    using Key = KeyboardKey;
    using GPBtn = GamepadButton;

    RebindContext rebindCtx{*this};

    rebindCtx.Rebind(hotkeys.openSettings, {KeyCombo{Mod::None, Key::F10}});
    rebindCtx.Rebind(hotkeys.toggleWindowedVideoOutput, {KeyCombo{Mod::None, Key::F9}});
    rebindCtx.Rebind(hotkeys.toggleFullScreen, {KeyCombo{Mod::Alt, Key::Return}});
    rebindCtx.Rebind(hotkeys.toggleFrameRateOSD, {KeyCombo{Mod::Shift, Key::F1}});
    rebindCtx.Rebind(hotkeys.nextFrameRateOSDPos, {KeyCombo{Mod::Control, Key::F1}});
    rebindCtx.Rebind(hotkeys.prevFrameRateOSDPos, {KeyCombo{Mod::Control | Mod::Shift, Key::F1}});

    rebindCtx.Rebind(hotkeys.toggleMute, {KeyCombo{Mod::Control, Key::M}});
    rebindCtx.Rebind(hotkeys.increaseVolume, {KeyCombo{Mod::Control, Key::EqualsPlus}});
    rebindCtx.Rebind(hotkeys.decreaseVolume, {KeyCombo{Mod::Control, Key::MinusUnderscore}});

    rebindCtx.Rebind(hotkeys.loadDisc, {KeyCombo{Mod::Control, Key::O}});
    rebindCtx.Rebind(hotkeys.ejectDisc, {KeyCombo{Mod::Control, Key::W}});
    rebindCtx.Rebind(hotkeys.openCloseTray, {KeyCombo{Mod::Control, Key::T}});

    rebindCtx.Rebind(hotkeys.hardReset, {KeyCombo{Mod::Control, Key::R}});
    rebindCtx.Rebind(hotkeys.softReset, {KeyCombo{Mod::Control | Mod::Shift, Key::R}});
    rebindCtx.Rebind(hotkeys.resetButton, {KeyCombo{Mod::Shift, Key::R}});

    rebindCtx.Rebind(hotkeys.turboSpeed, {KeyCombo{Mod::None, Key::Tab}});
    rebindCtx.Rebind(hotkeys.turboSpeedHold, {KeyCombo{Mod::None, Key::GraveAccent}});
    rebindCtx.Rebind(hotkeys.toggleAlternateSpeed, {KeyCombo{Mod::None, Key::Slash}});
    rebindCtx.Rebind(hotkeys.increaseSpeed, {KeyCombo{Mod::None, Key::Period}});
    rebindCtx.Rebind(hotkeys.decreaseSpeed, {KeyCombo{Mod::None, Key::Comma}});
    rebindCtx.Rebind(hotkeys.increaseSpeedLarge, {KeyCombo{Mod::Shift, Key::Period}});
    rebindCtx.Rebind(hotkeys.decreaseSpeedLarge, {KeyCombo{Mod::Shift, Key::Comma}});
    rebindCtx.Rebind(hotkeys.resetSpeed, {KeyCombo{Mod::Control, Key::Slash}});
    rebindCtx.Rebind(hotkeys.pauseResume, {KeyCombo{Mod::None, Key::Pause}, KeyCombo{Mod::None, Key::Spacebar}});
    rebindCtx.Rebind(hotkeys.fwdFrameStep, {KeyCombo{Mod::None, Key::RightBracket}});
    rebindCtx.Rebind(hotkeys.revFrameStep, {KeyCombo{Mod::None, Key::LeftBracket}});
    rebindCtx.Rebind(hotkeys.rewind, {KeyCombo{Mod::None, Key::Backspace}});
    rebindCtx.Rebind(hotkeys.toggleRewindBuffer, {KeyCombo{Mod::None, Key::F8}});

    rebindCtx.Rebind(hotkeys.toggleDebugTrace, {KeyCombo{Mod::None, Key::F11}});
    rebindCtx.Rebind(hotkeys.dumpMemory, {KeyCombo{Mod::Control, Key::F11}});

    rebindCtx.Rebind(hotkeys.saveStates.quickLoad, {KeyCombo{Mod::None, Key::F3}});
    rebindCtx.Rebind(hotkeys.saveStates.quickSave, {KeyCombo{Mod::None, Key::F2}});

    rebindCtx.Rebind(hotkeys.saveStates.select1, {KeyCombo{Mod::None, Key::Alpha1}});
    rebindCtx.Rebind(hotkeys.saveStates.select2, {KeyCombo{Mod::None, Key::Alpha2}});
    rebindCtx.Rebind(hotkeys.saveStates.select3, {KeyCombo{Mod::None, Key::Alpha3}});
    rebindCtx.Rebind(hotkeys.saveStates.select4, {KeyCombo{Mod::None, Key::Alpha4}});
    rebindCtx.Rebind(hotkeys.saveStates.select5, {KeyCombo{Mod::None, Key::Alpha5}});
    rebindCtx.Rebind(hotkeys.saveStates.select6, {KeyCombo{Mod::None, Key::Alpha6}});
    rebindCtx.Rebind(hotkeys.saveStates.select7, {KeyCombo{Mod::None, Key::Alpha7}});
    rebindCtx.Rebind(hotkeys.saveStates.select8, {KeyCombo{Mod::None, Key::Alpha8}});
    rebindCtx.Rebind(hotkeys.saveStates.select9, {KeyCombo{Mod::None, Key::Alpha9}});
    rebindCtx.Rebind(hotkeys.saveStates.select10, {KeyCombo{Mod::None, Key::Alpha0}});

    rebindCtx.Rebind(hotkeys.saveStates.load1, {KeyCombo{Mod::Control, Key::Alpha1}});
    rebindCtx.Rebind(hotkeys.saveStates.load2, {KeyCombo{Mod::Control, Key::Alpha2}});
    rebindCtx.Rebind(hotkeys.saveStates.load3, {KeyCombo{Mod::Control, Key::Alpha3}});
    rebindCtx.Rebind(hotkeys.saveStates.load4, {KeyCombo{Mod::Control, Key::Alpha4}});
    rebindCtx.Rebind(hotkeys.saveStates.load5, {KeyCombo{Mod::Control, Key::Alpha5}});
    rebindCtx.Rebind(hotkeys.saveStates.load6, {KeyCombo{Mod::Control, Key::Alpha6}});
    rebindCtx.Rebind(hotkeys.saveStates.load7, {KeyCombo{Mod::Control, Key::Alpha7}});
    rebindCtx.Rebind(hotkeys.saveStates.load8, {KeyCombo{Mod::Control, Key::Alpha8}});
    rebindCtx.Rebind(hotkeys.saveStates.load9, {KeyCombo{Mod::Control, Key::Alpha9}});
    rebindCtx.Rebind(hotkeys.saveStates.load10, {KeyCombo{Mod::Control, Key::Alpha0}});

    rebindCtx.Rebind(hotkeys.saveStates.save1, {KeyCombo{Mod::Shift, Key::Alpha1}});
    rebindCtx.Rebind(hotkeys.saveStates.save2, {KeyCombo{Mod::Shift, Key::Alpha2}});
    rebindCtx.Rebind(hotkeys.saveStates.save3, {KeyCombo{Mod::Shift, Key::Alpha3}});
    rebindCtx.Rebind(hotkeys.saveStates.save4, {KeyCombo{Mod::Shift, Key::Alpha4}});
    rebindCtx.Rebind(hotkeys.saveStates.save5, {KeyCombo{Mod::Shift, Key::Alpha5}});
    rebindCtx.Rebind(hotkeys.saveStates.save6, {KeyCombo{Mod::Shift, Key::Alpha6}});
    rebindCtx.Rebind(hotkeys.saveStates.save7, {KeyCombo{Mod::Shift, Key::Alpha7}});
    rebindCtx.Rebind(hotkeys.saveStates.save8, {KeyCombo{Mod::Shift, Key::Alpha8}});
    rebindCtx.Rebind(hotkeys.saveStates.save9, {KeyCombo{Mod::Shift, Key::Alpha9}});
    rebindCtx.Rebind(hotkeys.saveStates.save10, {KeyCombo{Mod::Shift, Key::Alpha0}});

    RebindInputs();

    return rebindCtx.GetReplacedActions();
}

std::unordered_set<input::MappedAction> Settings::ResetBinds(Input::Port::ControlPadBinds &binds, bool useDefaults) {
    using namespace input;

    using Key = KeyboardKey;
    using GPBtn = GamepadButton;
    using GPAxis2 = GamepadAxis2D;

    RebindContext rebindCtx{*this};

    if (!useDefaults) {
        rebindCtx.Rebind(binds.a, {});
        rebindCtx.Rebind(binds.b, {});
        rebindCtx.Rebind(binds.c, {});
        rebindCtx.Rebind(binds.x, {});
        rebindCtx.Rebind(binds.y, {});
        rebindCtx.Rebind(binds.z, {});
        rebindCtx.Rebind(binds.l, {});
        rebindCtx.Rebind(binds.r, {});
        rebindCtx.Rebind(binds.start, {});
        rebindCtx.Rebind(binds.up, {});
        rebindCtx.Rebind(binds.down, {});
        rebindCtx.Rebind(binds.left, {});
        rebindCtx.Rebind(binds.right, {});
        rebindCtx.Rebind(binds.dpad, {});
    } else if (&binds == &input.port1.controlPadBinds) {
        // Default port 1 Control Pad controller inputs
        rebindCtx.Rebind(binds.a, {{{Key::J}, {0, GPBtn::X}}});
        rebindCtx.Rebind(binds.b, {{{Key::K}, {0, GPBtn::A}}});
        rebindCtx.Rebind(binds.c, {{{Key::L}, {0, GPBtn::B}}});
        rebindCtx.Rebind(binds.x, {{{Key::U}, {0, GPBtn::LeftBumper}}});
        rebindCtx.Rebind(binds.y, {{{Key::I}, {0, GPBtn::Y}}});
        rebindCtx.Rebind(binds.z, {{{Key::O}, {0, GPBtn::RightBumper}}});
        rebindCtx.Rebind(binds.l, {{{Key::Q}, {0, GPBtn::LeftTrigger}}});
        rebindCtx.Rebind(binds.r, {{{Key::E}, {0, GPBtn::RightTrigger}}});
        rebindCtx.Rebind(binds.start, {{{Key::F}, {Key::G}, {Key::H}, {0, GPBtn::Start}}});
        rebindCtx.Rebind(binds.up, {{{Key::W}}});
        rebindCtx.Rebind(binds.down, {{{Key::S}}});
        rebindCtx.Rebind(binds.left, {{{Key::A}}});
        rebindCtx.Rebind(binds.right, {{{Key::D}}});
        rebindCtx.Rebind(binds.dpad, {{{0, GPAxis2::DPad}, {0, GPAxis2::LeftStick}}});
    } else if (&binds == &input.port2.controlPadBinds) {
        // Default port 2 Control Pad controller inputs
        rebindCtx.Rebind(binds.a, {{{Key::KeyPad1}, {1, GPBtn::X}}});
        rebindCtx.Rebind(binds.b, {{{Key::KeyPad2}, {1, GPBtn::A}}});
        rebindCtx.Rebind(binds.c, {{{Key::KeyPad3}, {1, GPBtn::B}}});
        rebindCtx.Rebind(binds.x, {{{Key::KeyPad4}, {1, GPBtn::LeftBumper}}});
        rebindCtx.Rebind(binds.y, {{{Key::KeyPad5}, {1, GPBtn::Y}}});
        rebindCtx.Rebind(binds.z, {{{Key::KeyPad6}, {1, GPBtn::RightBumper}}});
        rebindCtx.Rebind(binds.l, {{{Key::KeyPad7}, {Key::Insert}, {1, GPBtn::LeftTrigger}}});
        rebindCtx.Rebind(binds.r, {{{Key::KeyPad9}, {Key::PageUp}, {1, GPBtn::RightTrigger}}});
        rebindCtx.Rebind(binds.start, {{{Key::KeyPadEnter}, {1, GPBtn::Start}}});
        rebindCtx.Rebind(binds.up, {{{Key::Up}, {Key::Home}}});
        rebindCtx.Rebind(binds.down, {{{Key::Down}, {Key::End}}});
        rebindCtx.Rebind(binds.left, {{{Key::Left}, {Key::Delete}}});
        rebindCtx.Rebind(binds.right, {{{Key::Right}, {Key::PageDown}}});
        rebindCtx.Rebind(binds.dpad, {{{1, GPAxis2::DPad}, {1, GPAxis2::LeftStick}}});
    }

    RebindInputs();

    return rebindCtx.GetReplacedActions();
}

std::unordered_set<input::MappedAction> Settings::ResetBinds(Input::Port::AnalogPadBinds &binds, bool useDefaults) {
    using namespace input;

    using Key = KeyboardKey;
    using GPBtn = GamepadButton;
    using GPAxis1 = GamepadAxis1D;
    using GPAxis2 = GamepadAxis2D;

    RebindContext rebindCtx{*this};

    if (!useDefaults) {
        rebindCtx.Rebind(binds.a, {});
        rebindCtx.Rebind(binds.b, {});
        rebindCtx.Rebind(binds.c, {});
        rebindCtx.Rebind(binds.x, {});
        rebindCtx.Rebind(binds.y, {});
        rebindCtx.Rebind(binds.z, {});
        rebindCtx.Rebind(binds.l, {});
        rebindCtx.Rebind(binds.r, {});
        rebindCtx.Rebind(binds.start, {});
        rebindCtx.Rebind(binds.up, {});
        rebindCtx.Rebind(binds.down, {});
        rebindCtx.Rebind(binds.left, {});
        rebindCtx.Rebind(binds.right, {});
        rebindCtx.Rebind(binds.dpad, {});
        rebindCtx.Rebind(binds.analogStick, {});
        rebindCtx.Rebind(binds.analogL, {});
        rebindCtx.Rebind(binds.analogR, {});
        rebindCtx.Rebind(binds.switchMode, {});
    } else if (&binds == &input.port1.analogPadBinds) {
        // Default port 1 Control Pad controller inputs
        rebindCtx.Rebind(binds.a, {{{Key::J}, {0, GPBtn::X}}});
        rebindCtx.Rebind(binds.b, {{{Key::K}, {0, GPBtn::A}}});
        rebindCtx.Rebind(binds.c, {{{Key::L}, {0, GPBtn::B}}});
        rebindCtx.Rebind(binds.x, {{{Key::U}, {0, GPBtn::LeftBumper}}});
        rebindCtx.Rebind(binds.y, {{{Key::I}, {0, GPBtn::Y}}});
        rebindCtx.Rebind(binds.z, {{{Key::O}, {0, GPBtn::RightBumper}}});
        rebindCtx.Rebind(binds.l, {{{Key::Q}}});
        rebindCtx.Rebind(binds.r, {{{Key::E}}});
        rebindCtx.Rebind(binds.start, {{{Key::F}, {Key::G}, {Key::H}, {0, GPBtn::Start}}});
        rebindCtx.Rebind(binds.up, {{{Key::W}}});
        rebindCtx.Rebind(binds.down, {{{Key::S}}});
        rebindCtx.Rebind(binds.left, {{{Key::A}}});
        rebindCtx.Rebind(binds.right, {{{Key::D}}});
        rebindCtx.Rebind(binds.dpad, {{{0, GPAxis2::DPad}}});
        rebindCtx.Rebind(binds.analogStick, {{{0, GPAxis2::LeftStick}}});
        rebindCtx.Rebind(binds.analogL, {{{0, GPAxis1::LeftTrigger}}});
        rebindCtx.Rebind(binds.analogR, {{{0, GPAxis1::RightTrigger}}});
        rebindCtx.Rebind(binds.switchMode, {{{0, GPBtn::LeftThumb}}});
    } else if (&binds == &input.port2.analogPadBinds) {
        // Default port 2 Control Pad controller inputs
        rebindCtx.Rebind(binds.a, {{{Key::KeyPad1}, {1, GPBtn::X}}});
        rebindCtx.Rebind(binds.b, {{{Key::KeyPad2}, {1, GPBtn::A}}});
        rebindCtx.Rebind(binds.c, {{{Key::KeyPad3}, {1, GPBtn::B}}});
        rebindCtx.Rebind(binds.x, {{{Key::KeyPad4}, {1, GPBtn::LeftBumper}}});
        rebindCtx.Rebind(binds.y, {{{Key::KeyPad5}, {1, GPBtn::Y}}});
        rebindCtx.Rebind(binds.z, {{{Key::KeyPad6}, {1, GPBtn::RightBumper}}});
        rebindCtx.Rebind(binds.l, {{{Key::KeyPad7}, {Key::Insert}}});
        rebindCtx.Rebind(binds.r, {{{Key::KeyPad9}, {Key::PageUp}}});
        rebindCtx.Rebind(binds.start, {{{Key::KeyPadEnter}, {1, GPBtn::Start}}});
        rebindCtx.Rebind(binds.up, {{{Key::Up}, {Key::Home}}});
        rebindCtx.Rebind(binds.down, {{{Key::Down}, {Key::End}}});
        rebindCtx.Rebind(binds.left, {{{Key::Left}, {Key::Delete}}});
        rebindCtx.Rebind(binds.right, {{{Key::Right}, {Key::PageDown}}});
        rebindCtx.Rebind(binds.dpad, {{{1, GPAxis2::DPad}, {1, GPAxis2::LeftStick}}});
        rebindCtx.Rebind(binds.analogStick, {{{1, GPAxis2::LeftStick}}});
        rebindCtx.Rebind(binds.analogL, {{{1, GPAxis1::LeftTrigger}}});
        rebindCtx.Rebind(binds.analogR, {{{1, GPAxis1::RightTrigger}}});
        rebindCtx.Rebind(binds.switchMode, {{{1, GPBtn::LeftThumb}}});
    }

    RebindInputs();

    return rebindCtx.GetReplacedActions();
}

std::unordered_set<input::MappedAction> Settings::ResetBinds(Input::Port::ArcadeRacerBinds &binds, bool useDefaults) {
    using namespace input;

    using Key = KeyboardKey;
    using GPBtn = GamepadButton;
    using GPAxis1 = GamepadAxis1D;

    RebindContext rebindCtx{*this};

    if (!useDefaults) {
        rebindCtx.Rebind(binds.a, {});
        rebindCtx.Rebind(binds.b, {});
        rebindCtx.Rebind(binds.c, {});
        rebindCtx.Rebind(binds.x, {});
        rebindCtx.Rebind(binds.y, {});
        rebindCtx.Rebind(binds.z, {});
        rebindCtx.Rebind(binds.start, {});
        rebindCtx.Rebind(binds.up, {});
        rebindCtx.Rebind(binds.down, {});
        rebindCtx.Rebind(binds.wheelLeft, {});
        rebindCtx.Rebind(binds.wheelRight, {});
        rebindCtx.Rebind(binds.wheel, {});
    } else if (&binds == &input.port1.arcadeRacerBinds) {
        // Default port 1 Arcade Racer controller inputs
        rebindCtx.Rebind(binds.a, {{{Key::J}, {0, GPBtn::X}}});
        rebindCtx.Rebind(binds.b, {{{Key::K}, {0, GPBtn::A}}});
        rebindCtx.Rebind(binds.c, {{{Key::L}, {0, GPBtn::B}}});
        rebindCtx.Rebind(binds.x, {{{Key::U}, {0, GPBtn::LeftBumper}}});
        rebindCtx.Rebind(binds.y, {{{Key::I}, {0, GPBtn::Y}}});
        rebindCtx.Rebind(binds.z, {{{Key::O}, {0, GPBtn::RightBumper}}});
        rebindCtx.Rebind(binds.start, {{{Key::F}, {Key::G}, {Key::H}, {0, GPBtn::Start}}});
        rebindCtx.Rebind(binds.up, {{{Key::W}, {0, GPBtn::DpadUp}, {0, GPBtn::RightTrigger}, {0, GPBtn::RightBumper}}});
        rebindCtx.Rebind(binds.down,
                         {{{Key::S}, {0, GPBtn::DpadDown}, {0, GPBtn::LeftTrigger}, {0, GPBtn::LeftBumper}}});
        rebindCtx.Rebind(binds.wheelLeft, {{{Key::A}}});
        rebindCtx.Rebind(binds.wheelRight, {{{Key::D}}});
        rebindCtx.Rebind(binds.wheel, {{{0, GPAxis1::LeftStickX}}});
    } else if (&binds == &input.port2.arcadeRacerBinds) {
        // Default port 2 Arcade Racer controller inputs
        rebindCtx.Rebind(binds.a, {{{Key::KeyPad1}, {1, GPBtn::X}}});
        rebindCtx.Rebind(binds.b, {{{Key::KeyPad2}, {1, GPBtn::A}}});
        rebindCtx.Rebind(binds.c, {{{Key::KeyPad3}, {1, GPBtn::B}}});
        rebindCtx.Rebind(binds.x, {{{Key::KeyPad4}, {1, GPBtn::LeftBumper}}});
        rebindCtx.Rebind(binds.y, {{{Key::KeyPad5}, {1, GPBtn::Y}}});
        rebindCtx.Rebind(binds.z, {{{Key::KeyPad6}, {1, GPBtn::RightBumper}}});
        rebindCtx.Rebind(binds.start, {{{Key::KeyPadEnter}, {1, GPBtn::Start}}});
        rebindCtx.Rebind(
            binds.up,
            {{{Key::Up}, {Key::Home}, {1, GPBtn::DpadUp}, {1, GPBtn::RightTrigger}, {1, GPBtn::RightBumper}}});
        rebindCtx.Rebind(
            binds.down,
            {{{Key::Down}, {Key::End}, {1, GPBtn::DpadDown}, {1, GPBtn::LeftTrigger}, {1, GPBtn::LeftBumper}}});
        rebindCtx.Rebind(binds.wheelLeft, {{{Key::Left}, {Key::Delete}}});
        rebindCtx.Rebind(binds.wheelRight, {{{Key::Right}, {Key::PageDown}}});
        rebindCtx.Rebind(binds.wheel, {{{1, GPAxis1::LeftStickX}}});
    }

    RebindInputs();

    return rebindCtx.GetReplacedActions();
}

Settings::InputMap &Settings::GetInputMapForContext(void *context) {
    if (context == &m_context.controlPadInputs[0]) {
        // Port 1 Control Pad inputs
        return m_port1ControlPadInputs;
    } else if (context == &m_context.controlPadInputs[1]) {
        // Port 2 Control Pad inputs
        return m_port2ControlPadInputs;
    } else if (context == &m_context.analogPadInputs[0]) {
        // Port 1 3D Control Pad inputs
        return m_port1AnalogPadInputs;
    } else if (context == &m_context.analogPadInputs[1]) {
        // Port 2 3D Control Pad inputs
        return m_port2AnalogPadInputs;
    } else if (context == &m_context.arcadeRacerInputs[0]) {
        // Port 1 Arcade Racer inputs
        return m_port1ArcadeRacerInputs;
    } else if (context == &m_context.arcadeRacerInputs[1]) {
        // Port 2 Arcade Racer inputs
        return m_port2ArcadeRacerInputs;
    } else {
        // Hotkeys
        return m_actionInputs;
    }
}

std::filesystem::path Settings::Proximate(ProfilePath base, std::filesystem::path path) const {
    return std::filesystem::proximate(path, m_context.profile.GetPath(base));
}

std::filesystem::path Settings::Absolute(ProfilePath base, std::filesystem::path path) const {
    if (path.empty()) {
        return "";
    }
    if (path.is_absolute()) {
        return path;
    }
    return m_context.profile.GetPath(base) / path;
}

const char *BupCapacityLongName(Settings::Cartridge::BackupRAM::Capacity capacity) {
    switch (capacity) {
    case Settings::Cartridge::BackupRAM::Capacity::_4Mbit: return "4 Mbit (512 KiB)";
    case Settings::Cartridge::BackupRAM::Capacity::_8Mbit: return "8 Mbit (1 MiB)";
    case Settings::Cartridge::BackupRAM::Capacity::_16Mbit: return "16 Mbit (2 MiB)";
    case Settings::Cartridge::BackupRAM::Capacity::_32Mbit: return "32 Mbit (4 MiB)";
    default: return "32 Mbit (4 MiB)";
    }
}

const char *BupCapacityShortName(Settings::Cartridge::BackupRAM::Capacity capacity) {
    switch (capacity) {
    case Settings::Cartridge::BackupRAM::Capacity::_4Mbit: return "4 Mbit";
    case Settings::Cartridge::BackupRAM::Capacity::_8Mbit: return "8 Mbit";
    case Settings::Cartridge::BackupRAM::Capacity::_16Mbit: return "16 Mbit";
    case Settings::Cartridge::BackupRAM::Capacity::_32Mbit: return "32 Mbit";
    default: return "32 Mbit";
    }
}

Settings::Cartridge::BackupRAM::Capacity SizeToCapacity(uint32 size) {
    if (size <= 512_KiB) {
        return Settings::Cartridge::BackupRAM::Capacity::_4Mbit;
    } else if (size <= 1_MiB) {
        return Settings::Cartridge::BackupRAM::Capacity::_8Mbit;
    } else if (size <= 2_MiB) {
        return Settings::Cartridge::BackupRAM::Capacity::_16Mbit;
    } else {
        return Settings::Cartridge::BackupRAM::Capacity::_32Mbit;
    }
}

ymir::bup::BackupMemorySize CapacityToBupSize(Settings::Cartridge::BackupRAM::Capacity capacity) {
    switch (capacity) {
    case Settings::Cartridge::BackupRAM::Capacity::_4Mbit: return ymir::bup::BackupMemorySize::_4Mbit;
    case Settings::Cartridge::BackupRAM::Capacity::_8Mbit: return ymir::bup::BackupMemorySize::_8Mbit;
    case Settings::Cartridge::BackupRAM::Capacity::_16Mbit: return ymir::bup::BackupMemorySize::_16Mbit;
    case Settings::Cartridge::BackupRAM::Capacity::_32Mbit: return ymir::bup::BackupMemorySize::_32Mbit;
    default: return ymir::bup::BackupMemorySize::_32Mbit;
    }
}

uint32 CapacityToSize(Settings::Cartridge::BackupRAM::Capacity capacity) {
    switch (capacity) {
    case Settings::Cartridge::BackupRAM::Capacity::_4Mbit: return 512_KiB;
    case Settings::Cartridge::BackupRAM::Capacity::_8Mbit: return 1_MiB;
    case Settings::Cartridge::BackupRAM::Capacity::_16Mbit: return 2_MiB;
    case Settings::Cartridge::BackupRAM::Capacity::_32Mbit: return 4_MiB;
    default: return 4_MiB;
    }
}

void Settings::RebindContext::Rebind(input::InputBind &bind,
                                     const std::array<input::InputElement, input::kNumBindsPerInput> &defaults) {
    // Unbind the old inputs and remember which actions were bound to them to exclude from the set of replaced actions
    for (auto &input : bind.elements) {
        if (auto replaced = m_sharedCtx.inputContext.UnmapInput(input)) {
            m_previousActions.insert(*replaced);
        }
    }

    // Replace the binds
    bind.elements = defaults;

    // Unbind the new inputs and add the replaced actions to the set if not previously bound to one of the actions we're
    // replacing
    for (auto &input : defaults) {
        if (auto replaced = m_sharedCtx.inputContext.UnmapInput(input)) {
            if (!m_previousActions.contains(*replaced)) {
                m_replacedActions.insert(*replaced);

                // Also remove the bind from the settings
                auto &map = m_settings.GetInputMapForContext(replaced->context);
                if (map.map.contains(replaced->action)) {
                    for (auto *replacedBind : map.map.at(replaced->action)) {
                        assert(replacedBind != nullptr);
                        for (auto &element : replacedBind->elements) {
                            for (auto &defaultElem : defaults) {
                                if (element == defaultElem) {
                                    element = {};
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

} // namespace app
