#include "settings.hpp"

#include <app/actions.hpp>

#include <app/shared_context.hpp>

#include <satemu/sys/saturn.hpp>

#include <satemu/util/date_time.hpp>
#include <satemu/util/dev_log.hpp>
#include <satemu/util/inline.hpp>

using namespace std::literals;
using namespace satemu;

namespace app {

namespace grp {

    // -----------------------------------------------------------------------------
    // Dev log groups

    // Hierarchy:
    //
    // base

    struct base {
        static constexpr bool enabled = true;
        static constexpr devlog::Level level = devlog::level::debug;
        static constexpr std::string_view name = "Settings";
    };

} // namespace grp

inline constexpr int kConfigVersion = 1;

// -------------------------------------------------------------------------------------------------
// Enum parsers

FORCE_INLINE static void Parse(toml::node_view<toml::node> &node, const char *name, config::sys::VideoStandard &value) {
    value = config::sys::VideoStandard::NTSC;
    if (auto opt = node[name].value<std::string>()) {
        if (*opt == "NTSC"s) {
            value = config::sys::VideoStandard::NTSC;
        } else if (*opt == "PAL"s) {
            value = config::sys::VideoStandard::PAL;
        }
    }
}

FORCE_INLINE static void Parse(toml::node_view<toml::node> &node, const char *name, config::rtc::Mode &value) {
    value = config::rtc::Mode::Host;
    if (auto opt = node[name].value<std::string>()) {
        if (*opt == "Host"s) {
            value = config::rtc::Mode::Host;
        } else if (*opt == "Virtual"s) {
            value = config::rtc::Mode::Virtual;
        }
    }
}

FORCE_INLINE static void Parse(toml::node_view<toml::node> &node, const char *name,
                               config::rtc::HardResetStrategy &value) {
    value = config::rtc::HardResetStrategy::Preserve;
    if (auto opt = node[name].value<std::string>()) {
        if (*opt == "PreserveCurrentTime"s) {
            value = config::rtc::HardResetStrategy::Preserve;
        } else if (*opt == "SyncToHost"s) {
            value = config::rtc::HardResetStrategy::SyncToHost;
        } else if (*opt == "SyncToFixedStartingTime"s) {
            value = config::rtc::HardResetStrategy::ResetToFixedTime;
        }
    }
}

FORCE_INLINE static void Parse(toml::node_view<toml::node> &node, const char *name, peripheral::PeripheralType &value) {
    value = peripheral::PeripheralType::None;
    if (auto opt = node[name].value<std::string>()) {
        if (*opt == "None"s) {
            value = peripheral::PeripheralType::None;
        } else if (*opt == "StandardPad"s) {
            value = peripheral::PeripheralType::StandardPad;
        }
    }
}

FORCE_INLINE static void Parse(toml::node_view<toml::node> &node, const char *name,
                               config::audio::SampleInterpolationMode &value) {
    value = config::audio::SampleInterpolationMode::NearestNeighbor;
    if (auto opt = node[name].value<std::string>()) {
        if (*opt == "Nearest"s) {
            value = config::audio::SampleInterpolationMode::NearestNeighbor;
        } else if (*opt == "Linear"s) {
            value = config::audio::SampleInterpolationMode::Linear;
        }
    }
}

// -------------------------------------------------------------------------------------------------
// Enum-to-string converters

FORCE_INLINE static const char *ToTOML(const config::sys::VideoStandard value) {
    switch (value) {
    default:
    case config::sys::VideoStandard::NTSC: return "NTSC";
    case config::sys::VideoStandard::PAL: return "PAL";
    }
}

FORCE_INLINE static const char *ToTOML(const config::rtc::Mode value) {
    switch (value) {
    default:
    case config::rtc::Mode::Host: return "Host";
    case config::rtc::Mode::Virtual: return "Virtual";
    }
}

FORCE_INLINE static const char *ToTOML(const config::rtc::HardResetStrategy value) {
    switch (value) {
    default:
    case config::rtc::HardResetStrategy::Preserve: return "Preserve";
    case config::rtc::HardResetStrategy::SyncToHost: return "SyncToHost";
    case config::rtc::HardResetStrategy::ResetToFixedTime: return "ResetToFixedTime";
    }
}

FORCE_INLINE static const char *ToTOML(const peripheral::PeripheralType value) {
    switch (value) {
    default:
    case peripheral::PeripheralType::None: return "None";
    case peripheral::PeripheralType::StandardPad: return "StandardPad";
    }
}

FORCE_INLINE static const char *ToTOML(const config::audio::SampleInterpolationMode value) {
    switch (value) {
    default:
    case config::audio::SampleInterpolationMode::NearestNeighbor: return "Nearest";
    case config::audio::SampleInterpolationMode::Linear: return "Linear";
    }
}

/*FORCE_INLINE static std::string ToTOML(const input::InputEvent &value) {
    return input::ToString(value);
}*/

// Creates a TOML array with valid entries (skips Nones).
FORCE_INLINE static toml::array ToTOML(const InputBind &value) {
    toml::array out{};
    for (auto &event : value.events) {
        if (event.type != input::InputEvent::Type::None) {
            out.push_back(input::ToString(event));
        }
    }
    return out;
}

// -------------------------------------------------------------------------------------------------
// Parsers

template <typename T>
FORCE_INLINE static void Parse(toml::node_view<toml::node> &node, const char *name, T &value) {
    if (auto opt = node[name].value<T>()) {
        value = *opt;
    }
}

FORCE_INLINE static void Parse(toml::node_view<toml::node> &node, const char *name, std::filesystem::path &value) {
    if (auto opt = node[name].value<std::filesystem::path::string_type>()) {
        value = *opt;
    }
}

template <typename T>
FORCE_INLINE static void Parse(toml::node_view<toml::node> &node, const char *name, util::Observable<T> &value) {
    T wrappedValue = value.Get();
    Parse(node, name, wrappedValue);
    value = wrappedValue;
}

/*FORCE_INLINE static void Parse(toml::node_view<toml::node> &node, const char *name, input::InputEvent &value) {
    if (auto opt = node[name].value<std::string_view>()) {
        input::TryParse((*opt), value);
    }
}*/

// Reads until the InputEventArray is full or runs out of entries, skipping all invalid and "None" entries.
FORCE_INLINE static void Parse(toml::node_view<toml::node> &node, const char *name, InputBind &value) {
    if (toml::array *arr = node[name].as_array()) {
        value.events.fill({});
        const size_t count = arr->size();
        size_t outIndex = 0;
        for (size_t i = 0; i < count && outIndex < kNumBindsPerInput; i++) {
            if (auto opt = arr->at(i).value<std::string_view>()) {
                auto &event = value.events[i];
                input::TryParse((*opt), event);
                if (event.type != input::InputEvent::Type::None) {
                    ++outIndex;
                }
            }
        }
    }
}

// -------------------------------------------------------------------------------------------------
// Implementation

Settings::Settings(SharedContext &sharedCtx) noexcept
    : m_emuConfig(sharedCtx.saturn.configuration)
    , m_inputContext(sharedCtx.inputContext) {

    auto mapActionInput = [&](InputBind &bind, void *context = nullptr) {
        m_actionInputs[bind.action].insert({&bind, context});
    };

    mapActionInput(hotkeys.openSettings);
    mapActionInput(hotkeys.toggleWindowedVideoOutput);
    mapActionInput(hotkeys.loadDisc);
    mapActionInput(hotkeys.ejectDisc);
    mapActionInput(hotkeys.openCloseTray);
    mapActionInput(hotkeys.hardReset);
    mapActionInput(hotkeys.softReset);
    mapActionInput(hotkeys.resetButton);
    mapActionInput(hotkeys.fastForward);
    mapActionInput(hotkeys.frameStep);
    mapActionInput(hotkeys.pauseResume);
    mapActionInput(hotkeys.toggleRewindBuffer);
    mapActionInput(hotkeys.toggleDebugTrace);
    mapActionInput(hotkeys.dumpMemory);

    mapActionInput(hotkeys.saveStates.quickLoad);
    mapActionInput(hotkeys.saveStates.quickSave);

    mapActionInput(hotkeys.saveStates.select1);
    mapActionInput(hotkeys.saveStates.select2);
    mapActionInput(hotkeys.saveStates.select3);
    mapActionInput(hotkeys.saveStates.select4);
    mapActionInput(hotkeys.saveStates.select5);
    mapActionInput(hotkeys.saveStates.select6);
    mapActionInput(hotkeys.saveStates.select7);
    mapActionInput(hotkeys.saveStates.select8);
    mapActionInput(hotkeys.saveStates.select9);
    mapActionInput(hotkeys.saveStates.select10);

    mapActionInput(hotkeys.saveStates.load1);
    mapActionInput(hotkeys.saveStates.load2);
    mapActionInput(hotkeys.saveStates.load3);
    mapActionInput(hotkeys.saveStates.load4);
    mapActionInput(hotkeys.saveStates.load5);
    mapActionInput(hotkeys.saveStates.load6);
    mapActionInput(hotkeys.saveStates.load7);
    mapActionInput(hotkeys.saveStates.load8);
    mapActionInput(hotkeys.saveStates.load9);
    mapActionInput(hotkeys.saveStates.load10);

    mapActionInput(hotkeys.saveStates.save1);
    mapActionInput(hotkeys.saveStates.save2);
    mapActionInput(hotkeys.saveStates.save3);
    mapActionInput(hotkeys.saveStates.save4);
    mapActionInput(hotkeys.saveStates.save5);
    mapActionInput(hotkeys.saveStates.save6);
    mapActionInput(hotkeys.saveStates.save7);
    mapActionInput(hotkeys.saveStates.save8);
    mapActionInput(hotkeys.saveStates.save9);
    mapActionInput(hotkeys.saveStates.save10);

    auto ctx1 = &sharedCtx.standardPadButtons[0];
    mapActionInput(input.port1.standardPadBinds.a, ctx1);
    mapActionInput(input.port1.standardPadBinds.b, ctx1);
    mapActionInput(input.port1.standardPadBinds.c, ctx1);
    mapActionInput(input.port1.standardPadBinds.x, ctx1);
    mapActionInput(input.port1.standardPadBinds.y, ctx1);
    mapActionInput(input.port1.standardPadBinds.z, ctx1);
    mapActionInput(input.port1.standardPadBinds.l, ctx1);
    mapActionInput(input.port1.standardPadBinds.r, ctx1);
    mapActionInput(input.port1.standardPadBinds.start, ctx1);
    mapActionInput(input.port1.standardPadBinds.up, ctx1);
    mapActionInput(input.port1.standardPadBinds.down, ctx1);
    mapActionInput(input.port1.standardPadBinds.left, ctx1);
    mapActionInput(input.port1.standardPadBinds.right, ctx1);

    auto ctx2 = &sharedCtx.standardPadButtons[1];
    mapActionInput(input.port2.standardPadBinds.a, ctx2);
    mapActionInput(input.port2.standardPadBinds.b, ctx2);
    mapActionInput(input.port2.standardPadBinds.c, ctx2);
    mapActionInput(input.port2.standardPadBinds.x, ctx2);
    mapActionInput(input.port2.standardPadBinds.y, ctx2);
    mapActionInput(input.port2.standardPadBinds.z, ctx2);
    mapActionInput(input.port2.standardPadBinds.l, ctx2);
    mapActionInput(input.port2.standardPadBinds.r, ctx2);
    mapActionInput(input.port2.standardPadBinds.start, ctx2);
    mapActionInput(input.port2.standardPadBinds.up, ctx2);
    mapActionInput(input.port2.standardPadBinds.down, ctx2);
    mapActionInput(input.port2.standardPadBinds.left, ctx2);
    mapActionInput(input.port2.standardPadBinds.right, ctx2);

    ResetToDefaults();
}

void Settings::ResetToDefaults() {
    general.preloadDiscImagesToRAM = false;
    general.boostEmuThreadPriority = true;
    general.boostProcessPriority = true;
    general.enableRewindBuffer = false;
    general.rewindCompressionSpeed = 6;

    system.biosPath = "";

    {
        using PeriphType = peripheral::PeripheralType;
        input.port1.type = PeriphType::StandardPad;
        input.port2.type = PeriphType::None;

        ResetHotkeys();

        ResetBinds(input.port1.standardPadBinds);
        ResetBinds(input.port2.standardPadBinds);
    }

    video.forceIntegerScaling = false;
    video.forceAspectRatio = true;
    video.forcedAspect = 4.0 / 3.0;
    video.autoResizeWindow = false;
    video.displayVideoOutputInWindow = false;
}

SettingsLoadResult Settings::Load(const std::filesystem::path &path) {
    // Read TOML table from the file
    auto parseResult = toml::parse_file(path.string());
    if (parseResult.failed()) {
        return SettingsLoadResult::TOMLParseError(parseResult.error());
    }
    auto &data = parseResult.table();

    // Update values
    ResetToDefaults();

    int configVersion = 0;
    if (auto opt = data["ConfigVersion"].value<int>()) {
        configVersion = *opt;
    }

    SettingsLoadResult result{};
    switch (configVersion) {
    case 1: result = LoadV1(data); break;
    default: return SettingsLoadResult::UnsupportedConfigVersion(configVersion);
    }

    this->path = path;
    return result;
}

SettingsLoadResult Settings::LoadV1(toml::table &data) {
    if (auto tblGeneral = data["General"]) {
        Parse(tblGeneral, "PreloadDiscImagesToRAM", general.preloadDiscImagesToRAM);
        Parse(tblGeneral, "BoostEmuThreadPriority", general.boostEmuThreadPriority);
        Parse(tblGeneral, "BoostProcessPriority", general.boostProcessPriority);
        Parse(tblGeneral, "EnableRewindBuffer", general.enableRewindBuffer);
        Parse(tblGeneral, "RewindCompressionSpeed", general.rewindCompressionSpeed);
    }

    if (auto tblSystem = data["System"]) {
        Parse(tblSystem, "BiosPath", system.biosPath);
        Parse(tblSystem, "VideoStandard", m_emuConfig.system.videoStandard);
        Parse(tblSystem, "AutoDetectRegion", m_emuConfig.system.autodetectRegion);
        Parse(tblSystem, "EmulateSH2Cache", m_emuConfig.system.emulateSH2Cache);

        auto &rtc = m_emuConfig.rtc;

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
        Parse(tblHotkeys, "LoadDisc", hotkeys.loadDisc);
        Parse(tblHotkeys, "EjectDisc", hotkeys.ejectDisc);
        Parse(tblHotkeys, "OpenCloseTray", hotkeys.openCloseTray);
        Parse(tblHotkeys, "HardReset", hotkeys.hardReset);
        Parse(tblHotkeys, "SoftReset", hotkeys.softReset);
        Parse(tblHotkeys, "ResetButton", hotkeys.resetButton);
        Parse(tblHotkeys, "FastForward", hotkeys.fastForward);
        Parse(tblHotkeys, "FrameStep", hotkeys.frameStep);
        Parse(tblHotkeys, "PauseResume", hotkeys.pauseResume);
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

                if (auto tblStandardPadBinds = tblPort["StandardPadBinds"]) {
                    Parse(tblStandardPadBinds, "A", portSettings.standardPadBinds.a);
                    Parse(tblStandardPadBinds, "B", portSettings.standardPadBinds.b);
                    Parse(tblStandardPadBinds, "C", portSettings.standardPadBinds.c);
                    Parse(tblStandardPadBinds, "X", portSettings.standardPadBinds.x);
                    Parse(tblStandardPadBinds, "Y", portSettings.standardPadBinds.y);
                    Parse(tblStandardPadBinds, "Z", portSettings.standardPadBinds.z);
                    Parse(tblStandardPadBinds, "L", portSettings.standardPadBinds.l);
                    Parse(tblStandardPadBinds, "R", portSettings.standardPadBinds.r);
                    Parse(tblStandardPadBinds, "Start", portSettings.standardPadBinds.start);
                    Parse(tblStandardPadBinds, "Up", portSettings.standardPadBinds.up);
                    Parse(tblStandardPadBinds, "Down", portSettings.standardPadBinds.down);
                    Parse(tblStandardPadBinds, "Left", portSettings.standardPadBinds.left);
                    Parse(tblStandardPadBinds, "Right", portSettings.standardPadBinds.right);
                }
            }
        };
        parsePort("Port1", input.port1);
        parsePort("Port2", input.port2);
    }

    if (auto tblVideo = data["Video"]) {
        Parse(tblVideo, "ForceIntegerScaling", video.forceIntegerScaling);
        Parse(tblVideo, "ForceAspectRatio", video.forceAspectRatio);
        Parse(tblVideo, "ForcedAspect", video.forcedAspect);

        Parse(tblVideo, "AutoResizeWindow", video.autoResizeWindow);
        Parse(tblVideo, "DisplayVideoOutputInWindow", video.displayVideoOutputInWindow);

        Parse(tblVideo, "ThreadedVDP1", m_emuConfig.video.threadedVDP1);
        Parse(tblVideo, "ThreadedVDP2", m_emuConfig.video.threadedVDP2);
    }

    if (auto tblAudio = data["Audio"]) {
        Parse(tblAudio, "InterpolationMode", m_emuConfig.audio.interpolation);
        Parse(tblAudio, "ThreadedSCSP", m_emuConfig.audio.threadedSCSP);
    }

    return SettingsLoadResult::Success();
}

SettingsSaveResult Settings::Save() {
    if (path.empty()) {
        path = "satemu.toml";
    }

    const auto &rtc = m_emuConfig.rtc;

    // clang-format off
    auto tbl = toml::table{{
        {"ConfigVersion", kConfigVersion},

        {"General", toml::table{{
            {"PreloadDiscImagesToRAM", general.preloadDiscImagesToRAM},
            {"BoostEmuThreadPriority", general.boostEmuThreadPriority},
            {"BoostProcessPriority", general.boostProcessPriority},
            {"EnableRewindBuffer", general.enableRewindBuffer},
            {"RewindCompressionSpeed", general.rewindCompressionSpeed},
        }}},

        {"System", toml::table{{
            {"BiosPath", system.biosPath.string()},
            {"VideoStandard", ToTOML(m_emuConfig.system.videoStandard)},
            {"AutoDetectRegion", m_emuConfig.system.autodetectRegion},
            {"EmulateSH2Cache", m_emuConfig.system.emulateSH2Cache.Get()},
        
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
            {"LoadDisc", ToTOML(hotkeys.loadDisc)},
            {"EjectDisc", ToTOML(hotkeys.ejectDisc)},
            {"OpenCloseTray", ToTOML(hotkeys.openCloseTray)},
            {"HardReset", ToTOML(hotkeys.hardReset)},
            {"SoftReset", ToTOML(hotkeys.softReset)},
            {"ResetButton", ToTOML(hotkeys.resetButton)},
            {"FastForward", ToTOML(hotkeys.fastForward)},
            {"FrameStep", ToTOML(hotkeys.frameStep)},
            {"PauseResume", ToTOML(hotkeys.pauseResume)},
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
                {"StandardPadBinds", toml::table{{
                    {"A", ToTOML(input.port1.standardPadBinds.a)},
                    {"B", ToTOML(input.port1.standardPadBinds.b)},
                    {"C", ToTOML(input.port1.standardPadBinds.c)},
                    {"X", ToTOML(input.port1.standardPadBinds.x)},
                    {"Y", ToTOML(input.port1.standardPadBinds.y)},
                    {"Z", ToTOML(input.port1.standardPadBinds.z)},
                    {"L", ToTOML(input.port1.standardPadBinds.l)},
                    {"R", ToTOML(input.port1.standardPadBinds.r)},
                    {"Start", ToTOML(input.port1.standardPadBinds.start)},
                    {"Up", ToTOML(input.port1.standardPadBinds.up)},
                    {"Down", ToTOML(input.port1.standardPadBinds.down)},
                    {"Left", ToTOML(input.port1.standardPadBinds.left)},
                    {"Right", ToTOML(input.port1.standardPadBinds.right)},
                }}},
            }}},
            {"Port2", toml::table{{
                {"PeripheralType", ToTOML(input.port2.type)},
                {"StandardPadBinds", toml::table{{
                    {"A", ToTOML(input.port2.standardPadBinds.a)},
                    {"B", ToTOML(input.port2.standardPadBinds.b)},
                    {"C", ToTOML(input.port2.standardPadBinds.c)},
                    {"X", ToTOML(input.port2.standardPadBinds.x)},
                    {"Y", ToTOML(input.port2.standardPadBinds.y)},
                    {"Z", ToTOML(input.port2.standardPadBinds.z)},
                    {"L", ToTOML(input.port2.standardPadBinds.l)},
                    {"R", ToTOML(input.port2.standardPadBinds.r)},
                    {"Start", ToTOML(input.port2.standardPadBinds.start)},
                    {"Up", ToTOML(input.port2.standardPadBinds.up)},
                    {"Down", ToTOML(input.port2.standardPadBinds.down)},
                    {"Left", ToTOML(input.port2.standardPadBinds.left)},
                    {"Right", ToTOML(input.port2.standardPadBinds.right)},
                }}},
            }}},
        }}},

        {"Video", toml::table{{
            {"ForceIntegerScaling", video.forceIntegerScaling},
            {"ForceAspectRatio", video.forceAspectRatio},
            {"ForcedAspect", video.forcedAspect},
            {"AutoResizeWindow", video.autoResizeWindow},
            {"DisplayVideoOutputInWindow", video.displayVideoOutputInWindow},
            {"ThreadedVDP1", m_emuConfig.video.threadedVDP1.Get()},
            {"ThreadedVDP2", m_emuConfig.video.threadedVDP2.Get()},
        }}},

        {"Audio", toml::table{{
            {"InterpolationMode", ToTOML(m_emuConfig.audio.interpolation)},
            {"ThreadedSCSP", m_emuConfig.audio.threadedSCSP.Get()},
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
            devlog::warn<grp::base>("Failed to save settings: {}", result.string());
        }
        m_dirty = false;
    }
}

void Settings::MakeDirty() {
    m_dirty = true;
    m_dirtyTimestamp = std::chrono::steady_clock::now();
}

void Settings::RebindInputs() {
    m_inputContext.UnmapAllActions();

    for (auto &[action, mappings] : m_actionInputs) {
        for (auto &[bind, context] : mappings) {
            for (auto &event : bind->events) {
                // Sanitization -- skip ESC binds if they were manually added in the configuration file
                if (event.type == input::InputEvent::Type::KeyCombo &&
                    event.keyCombo.key == input::KeyboardKey::Escape) {
                    continue;
                }
                m_inputContext.MapAction(event, action, context);
            }
        }
    }

    SyncInputSettings();
}

void Settings::RebindAction(input::ActionID action) {
    m_inputContext.UnmapAction(action);

    if (auto it = m_actionInputs.find(action); it != m_actionInputs.end()) {
        for (auto &[bind, context] : it->second) {
            for (auto &event : bind->events) {
                m_inputContext.MapAction(event, action, context);
            }
        }
    }

    SyncInputSettings();
}

void Settings::SyncInputSettings() {
    for (auto &[action, mappings] : m_actionInputs) {
        for (auto &[bind, context] : mappings) {
            bind->events.fill({});
            for (int i = 0; auto &input : m_inputContext.GetMappedInputs(bind->action)) {
                if (input.context == context) {
                    bind->events[i++] = input.event;
                    if (i == kNumBindsPerInput) {
                        break;
                    }
                }
            }
        }
    }
}

void Settings::ResetHotkeys() {
    using Mod = input::KeyModifier;
    using Key = input::KeyboardKey;
    using KeyCombo = input::KeyCombo;

    hotkeys.loadDisc.events = {KeyCombo{Mod::Control, Key::O}};
    hotkeys.ejectDisc.events = {KeyCombo{Mod::Control, Key::W}};
    hotkeys.openCloseTray.events = {KeyCombo{Mod::Control, Key::T}};

    hotkeys.toggleWindowedVideoOutput.events = {KeyCombo{Mod::None, Key::F9}};

    hotkeys.openSettings.events = {KeyCombo{Mod::None, Key::F10}};

    hotkeys.hardReset.events = {KeyCombo{Mod::Control, Key::R}};
    hotkeys.softReset.events = {KeyCombo{Mod::Control | Mod::Shift, Key::R}};

    hotkeys.fastForward.events = {KeyCombo{Mod::None, Key::Tab}};
    hotkeys.pauseResume.events = {KeyCombo{Mod::None, Key::Pause}, KeyCombo{Mod::Control, Key::P}};
    hotkeys.frameStep.events = {KeyCombo{Mod::None, Key::RightBracket}};
    hotkeys.toggleRewindBuffer.events = {KeyCombo{Mod::None, Key::F8}};

    hotkeys.resetButton.events = {KeyCombo{Mod::Shift, Key::R}};

    hotkeys.toggleDebugTrace.events = {KeyCombo{Mod::None, Key::F11}};
    hotkeys.dumpMemory.events = {KeyCombo{Mod::Control, Key::F11}};

    hotkeys.saveStates.quickLoad.events = {KeyCombo{Mod::None, Key::F3}};
    hotkeys.saveStates.quickSave.events = {KeyCombo{Mod::None, Key::F2}};

    hotkeys.saveStates.select1.events = {KeyCombo{Mod::None, Key::Alpha1}};
    hotkeys.saveStates.select2.events = {KeyCombo{Mod::None, Key::Alpha2}};
    hotkeys.saveStates.select3.events = {KeyCombo{Mod::None, Key::Alpha3}};
    hotkeys.saveStates.select4.events = {KeyCombo{Mod::None, Key::Alpha4}};
    hotkeys.saveStates.select5.events = {KeyCombo{Mod::None, Key::Alpha5}};
    hotkeys.saveStates.select6.events = {KeyCombo{Mod::None, Key::Alpha6}};
    hotkeys.saveStates.select7.events = {KeyCombo{Mod::None, Key::Alpha7}};
    hotkeys.saveStates.select8.events = {KeyCombo{Mod::None, Key::Alpha8}};
    hotkeys.saveStates.select9.events = {KeyCombo{Mod::None, Key::Alpha9}};
    hotkeys.saveStates.select10.events = {KeyCombo{Mod::None, Key::Alpha0}};

    hotkeys.saveStates.load1.events = {KeyCombo{Mod::Control, Key::Alpha1}};
    hotkeys.saveStates.load2.events = {KeyCombo{Mod::Control, Key::Alpha2}};
    hotkeys.saveStates.load3.events = {KeyCombo{Mod::Control, Key::Alpha3}};
    hotkeys.saveStates.load4.events = {KeyCombo{Mod::Control, Key::Alpha4}};
    hotkeys.saveStates.load5.events = {KeyCombo{Mod::Control, Key::Alpha5}};
    hotkeys.saveStates.load6.events = {KeyCombo{Mod::Control, Key::Alpha6}};
    hotkeys.saveStates.load7.events = {KeyCombo{Mod::Control, Key::Alpha7}};
    hotkeys.saveStates.load8.events = {KeyCombo{Mod::Control, Key::Alpha8}};
    hotkeys.saveStates.load9.events = {KeyCombo{Mod::Control, Key::Alpha9}};
    hotkeys.saveStates.load10.events = {KeyCombo{Mod::Control, Key::Alpha0}};

    hotkeys.saveStates.save1.events = {KeyCombo{Mod::Shift, Key::Alpha1}};
    hotkeys.saveStates.save2.events = {KeyCombo{Mod::Shift, Key::Alpha2}};
    hotkeys.saveStates.save3.events = {KeyCombo{Mod::Shift, Key::Alpha3}};
    hotkeys.saveStates.save4.events = {KeyCombo{Mod::Shift, Key::Alpha4}};
    hotkeys.saveStates.save5.events = {KeyCombo{Mod::Shift, Key::Alpha5}};
    hotkeys.saveStates.save6.events = {KeyCombo{Mod::Shift, Key::Alpha6}};
    hotkeys.saveStates.save7.events = {KeyCombo{Mod::Shift, Key::Alpha7}};
    hotkeys.saveStates.save8.events = {KeyCombo{Mod::Shift, Key::Alpha8}};
    hotkeys.saveStates.save9.events = {KeyCombo{Mod::Shift, Key::Alpha9}};
    hotkeys.saveStates.save10.events = {KeyCombo{Mod::Shift, Key::Alpha0}};
}

void Settings::ResetBinds(Input::Port::StandardPadBinds &binds) {
    using Key = input::KeyboardKey;

    if (&binds == &input.port1.standardPadBinds) {
        // Default port 1 Standard Pad controller inputs
        input.port1.standardPadBinds.a.events = {{{Key::J}}};
        input.port1.standardPadBinds.b.events = {{{Key::K}}};
        input.port1.standardPadBinds.c.events = {{{Key::L}}};
        input.port1.standardPadBinds.x.events = {{{Key::U}}};
        input.port1.standardPadBinds.y.events = {{{Key::I}}};
        input.port1.standardPadBinds.z.events = {{{Key::O}}};
        input.port1.standardPadBinds.l.events = {{{Key::Q}}};
        input.port1.standardPadBinds.r.events = {{{Key::E}}};
        input.port1.standardPadBinds.start.events = {{{Key::G}, {Key::F}, {Key::H}, {Key::Return}}};
        input.port1.standardPadBinds.up.events = {{{Key::W}}};
        input.port1.standardPadBinds.down.events = {{{Key::S}}};
        input.port1.standardPadBinds.left.events = {{{Key::A}}};
        input.port1.standardPadBinds.right.events = {{{Key::D}}};
    } else if (&binds == &input.port2.standardPadBinds) {
        // Default port 2 Standard Pad controller inputs
        input.port2.standardPadBinds.a.events = {{{Key::KeyPad1}}};
        input.port2.standardPadBinds.b.events = {{{Key::KeyPad2}}};
        input.port2.standardPadBinds.c.events = {{{Key::KeyPad3}}};
        input.port2.standardPadBinds.x.events = {{{Key::KeyPad4}}};
        input.port2.standardPadBinds.y.events = {{{Key::KeyPad5}}};
        input.port2.standardPadBinds.z.events = {{{Key::KeyPad6}}};
        input.port2.standardPadBinds.l.events = {{{Key::KeyPad7}, {Key::Insert}}};
        input.port2.standardPadBinds.r.events = {{{Key::KeyPad9}, {Key::PageUp}}};
        input.port2.standardPadBinds.start.events = {{{Key::KeyPadEnter}}};
        input.port2.standardPadBinds.up.events = {{{Key::Up}, {Key::Home}}};
        input.port2.standardPadBinds.down.events = {{{Key::Down}, {Key::End}}};
        input.port2.standardPadBinds.left.events = {{{Key::Left}, {Key::Delete}}};
        input.port2.standardPadBinds.right.events = {{{Key::Right}, {Key::PageDown}}};
    }
    RebindInputs();
}

} // namespace app
