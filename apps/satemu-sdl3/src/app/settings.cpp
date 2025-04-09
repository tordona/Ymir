#include "settings.hpp"

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

FORCE_INLINE static const char *EnumName(const config::sys::VideoStandard value) {
    switch (value) {
    default:
    case config::sys::VideoStandard::NTSC: return "NTSC";
    case config::sys::VideoStandard::PAL: return "PAL";
    }
}

FORCE_INLINE static const char *EnumName(const config::rtc::Mode value) {
    switch (value) {
    default:
    case config::rtc::Mode::Host: return "Host";
    case config::rtc::Mode::Virtual: return "Virtual";
    }
}

FORCE_INLINE static const char *EnumName(const config::rtc::HardResetStrategy value) {
    switch (value) {
    default:
    case config::rtc::HardResetStrategy::Preserve: return "Preserve";
    case config::rtc::HardResetStrategy::SyncToHost: return "SyncToHost";
    case config::rtc::HardResetStrategy::ResetToFixedTime: return "ResetToFixedTime";
    }
}

FORCE_INLINE static const char *EnumName(const peripheral::PeripheralType value) {
    switch (value) {
    default:
    case peripheral::PeripheralType::None: return "None";
    case peripheral::PeripheralType::StandardPad: return "StandardPad";
    }
}

FORCE_INLINE static const char *EnumName(const config::audio::SampleInterpolationMode value) {
    switch (value) {
    default:
    case config::audio::SampleInterpolationMode::NearestNeighbor: return "Nearest";
    case config::audio::SampleInterpolationMode::Linear: return "Linear";
    }
}

FORCE_INLINE static toml::array ArrayNames(const InputEventArray &value) {
    toml::array out{};
    for (size_t i = 0; i < kNumBindsPerInput; i++) {
        out.push_back(input::ToString(value[i]));
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

/*FORCE_INLINE static void Parse(toml::node_view<toml::node> &node, const char *name, std::filesystem::path &value) {
    if (auto opt = node[name].value<std::filesystem::path::string_type>()) {
        value = *opt;
    }
}*/

template <typename T>
FORCE_INLINE static void Parse(toml::node_view<toml::node> &node, const char *name, util::Observable<T> &value) {
    T wrappedValue = value.Get();
    Parse(node, name, wrappedValue);
    value = wrappedValue;
}

FORCE_INLINE static void Parse(toml::node_view<toml::node> &node, const char *name, InputEventArray &value) {
    if (toml::array *arr = node[name].as_array()) {
        value.fill({});
        const size_t count = std::min(arr->size(), kNumBindsPerInput);
        for (size_t i = 0; i < count; i++) {
            if (auto opt = arr->at(i).value<std::string_view>()) {
                input::TryParse((*opt), value[i]);
            }
        }
    }
}

// -------------------------------------------------------------------------------------------------
// Implementation

void Settings::ResetToDefaults() {
    general.preloadDiscImagesToRAM = false;
    general.boostEmuThreadPriority = true;
    general.boostProcessPriority = true;

    system.biosPath = "";

    input.port1.type = peripheral::PeripheralType::StandardPad;
    input.port2.type = peripheral::PeripheralType::None;

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
    }

    if (auto tblSystem = data["System"]) {
        Parse(tblSystem, "BiosPath", system.biosPath);
        Parse(tblSystem, "VideoStandard", m_emuConfig.system.videoStandard);
        Parse(tblSystem, "AutoDetectRegion", m_emuConfig.system.autodetectRegion);
        Parse(tblSystem, "EmulateSH2Cache", m_emuConfig.system.emulateSH2Cache);

        auto &rtc = m_emuConfig.rtc;

        if (auto tblRTC = tblSystem["RTC"]) {
            Parse(tblRTC, "Mode", rtc.mode);
            Parse(tblRTC, "VirtualHardResetStrategy", rtc.virtHardResetStrategy);
            Parse(tblRTC, "VirtualHardResetTimestamp", rtc.virtHardResetTimestamp);
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
        }}},

        {"System", toml::table{{
            {"BiosPath", system.biosPath},
            {"VideoStandard", EnumName(m_emuConfig.system.videoStandard)},
            {"AutoDetectRegion", m_emuConfig.system.autodetectRegion},
            {"EmulateSH2Cache", m_emuConfig.system.emulateSH2Cache.Get()},
        
            {"RTC", toml::table{{
                {"Mode", EnumName(rtc.mode)},
                {"VirtualHardResetStrategy", EnumName(rtc.virtHardResetStrategy)},
                {"VirtualHardResetTimestamp", rtc.virtHardResetTimestamp},
            }}},
        }}},

        {"Input", toml::table{{
            {"Port1", toml::table{{
                {"PeripheralType", EnumName(input.port1.type)},
                {"StandardPadBinds", toml::table{{
                    {"A", ArrayNames(input.port1.standardPadBinds.a)},
                    {"B", ArrayNames(input.port1.standardPadBinds.b)},
                    {"C", ArrayNames(input.port1.standardPadBinds.c)},
                    {"X", ArrayNames(input.port1.standardPadBinds.x)},
                    {"Y", ArrayNames(input.port1.standardPadBinds.y)},
                    {"Z", ArrayNames(input.port1.standardPadBinds.z)},
                    {"L", ArrayNames(input.port1.standardPadBinds.l)},
                    {"R", ArrayNames(input.port1.standardPadBinds.r)},
                    {"Start", ArrayNames(input.port1.standardPadBinds.start)},
                    {"Up", ArrayNames(input.port1.standardPadBinds.up)},
                    {"Down", ArrayNames(input.port1.standardPadBinds.down)},
                    {"Left", ArrayNames(input.port1.standardPadBinds.left)},
                    {"Right", ArrayNames(input.port1.standardPadBinds.right)},
                }}},
            }}},
            {"Port2", toml::table{{
                {"PeripheralType", EnumName(input.port2.type)},
                {"StandardPadBinds", toml::table{{
                    {"A", ArrayNames(input.port2.standardPadBinds.a)},
                    {"B", ArrayNames(input.port2.standardPadBinds.b)},
                    {"C", ArrayNames(input.port2.standardPadBinds.c)},
                    {"X", ArrayNames(input.port2.standardPadBinds.x)},
                    {"Y", ArrayNames(input.port2.standardPadBinds.y)},
                    {"Z", ArrayNames(input.port2.standardPadBinds.z)},
                    {"L", ArrayNames(input.port2.standardPadBinds.l)},
                    {"R", ArrayNames(input.port2.standardPadBinds.r)},
                    {"Start", ArrayNames(input.port2.standardPadBinds.start)},
                    {"Up", ArrayNames(input.port2.standardPadBinds.up)},
                    {"Down", ArrayNames(input.port2.standardPadBinds.down)},
                    {"Left", ArrayNames(input.port2.standardPadBinds.left)},
                    {"Right", ArrayNames(input.port2.standardPadBinds.right)},
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
            {"InterpolationMode", EnumName(m_emuConfig.audio.interpolation)},
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

} // namespace app
