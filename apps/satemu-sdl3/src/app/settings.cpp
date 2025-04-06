#include "settings.hpp"

#include <satemu/util/dev_log.hpp>

using namespace std::literals;

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

static void ParseEnum(toml::node_view<toml::node> &node, const char *name, RTCMode value) {
    if (auto opt = node[name].value<std::string>()) {
        if (*opt == "Host"s) {
            value = RTCMode::Host;
        } else if (*opt == "Virtual"s) {
            value = RTCMode::Virtual;
        } else {
            value = RTCMode::Host;
        }
    }
}

static void ParseEnum(toml::node_view<toml::node> &node, const char *name, VirtualRTCResetBehavior value) {
    if (auto opt = node[name].value<std::string>()) {
        if (*opt == "PreserveCurrentTime"s) {
            value = VirtualRTCResetBehavior::PreserveCurrentTime;
        } else if (*opt == "SyncToHost"s) {
            value = VirtualRTCResetBehavior::SyncToHost;
        } else if (*opt == "SyncToFixedStartingTime"s) {
            value = VirtualRTCResetBehavior::SyncToFixedStartingTime;
        } else {
            value = VirtualRTCResetBehavior::PreserveCurrentTime;
        }
    }
}

static void ParseEnum(toml::node_view<toml::node> &node, const char *name, AudioInterpolationMode value) {
    if (auto opt = node[name].value<std::string>()) {
        if (*opt == "Nearest"s) {
            value = AudioInterpolationMode::Nearest;
        } else if (*opt == "Linear"s) {
            value = AudioInterpolationMode::Linear;
        } else {
            value = AudioInterpolationMode::Nearest;
        }
    }
}

// -------------------------------------------------------------------------------------------------
// Enum-to-string converters

static const char *EnumName(const RTCMode value) {
    switch (value) {
    case RTCMode::Host: return "Host";
    case RTCMode::Virtual: return "Virtual";
    default: return "Host";
    }
}

static const char *EnumName(const VirtualRTCResetBehavior value) {
    switch (value) {
    case VirtualRTCResetBehavior::PreserveCurrentTime: return "PreserveCurrentTime";
    case VirtualRTCResetBehavior::SyncToHost: return "SyncToHost";
    case VirtualRTCResetBehavior::SyncToFixedStartingTime: return "SyncToFixedStartingTime";
    default: return "PreserveCurrentTime";
    }
}

static const char *EnumName(const AudioInterpolationMode value) {
    switch (value) {
    case AudioInterpolationMode::Nearest: return "Nearest";
    case AudioInterpolationMode::Linear: return "Linear";
    default: return "Nearest";
    }
}

// -------------------------------------------------------------------------------------------------
// Parsers

template <typename T>
void ParseSimple(toml::node_view<toml::node> &node, const char *name, T &value) {
    if (auto opt = node[name].value<T>()) {
        value = *opt;
    }
}

void ParsePath(toml::node_view<toml::node> &node, const char *name, std::filesystem::path &value) {
    if (auto opt = node[name].value<std::filesystem::path::string_type>()) {
        value = *opt;
    }
}

// -------------------------------------------------------------------------------------------------
// Implementation

void Settings::ResetToDefaults() {
    general.preloadDiscImagesToRAM = false;
    general.boostEmuThreadPriority = true;
    general.boostProcessPriority = true;

    system.biosPath = "";

    system.emulateSH2Cache = false;

    system.rtc.mode = RTCMode::Host;
    system.rtc.hostTimeOffset = 0;
    system.rtc.virtBaseTime = 757382400; // 01/01/1994 00:00:00
    system.rtc.virtResetBehavior = VirtualRTCResetBehavior::PreserveCurrentTime;

    // TODO: input

    video.threadedRendering = true;
    video.threadedVDP1 = true;

    audio.interpolationMode = AudioInterpolationMode::Nearest;
    audio.threadedSCSP = true;
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
        ParseSimple(tblGeneral, "PreloadDiscImagesToRAM", general.preloadDiscImagesToRAM);
        ParseSimple(tblGeneral, "BoostEmuThreadPriority", general.boostEmuThreadPriority);
        ParseSimple(tblGeneral, "BoostProcessPriority", general.boostProcessPriority);
    }

    if (auto tblSystem = data["System"]) {
        ParseSimple(tblSystem, "BiosPath", system.biosPath);
        ParseSimple(tblSystem, "EmulateSH2Cache", system.emulateSH2Cache);

        auto &rtc = system.rtc;

        if (auto tblRTC = tblSystem["RTC"]) {
            ParseEnum(tblRTC, "Mode", rtc.mode);
            ParseSimple(tblRTC, "HostTimeOffset", rtc.hostTimeOffset);
            ParseSimple(tblRTC, "VirtualBaseTime", rtc.virtBaseTime);
            ParseEnum(tblRTC, "VirtualResetBehavior", rtc.virtResetBehavior);
        }
    }

    /*if (auto tblInput = data["Input"]) {
        // TODO
    }*/

    if (auto tblVideo = data["Video"]) {
        ParseSimple(tblVideo, "ThreadedRendering", video.threadedRendering);
        ParseSimple(tblVideo, "ThreadedVDP1", video.threadedVDP1);
    }

    if (auto tblAudio = data["Audio"]) {
        ParseEnum(tblAudio, "InterpolationMode", audio.interpolationMode);
        ParseSimple(tblAudio, "ThreadedSCSP", audio.threadedSCSP);
    }

    return SettingsLoadResult::Success();
}

SettingsSaveResult Settings::Save() {
    if (path.empty()) {
        path = "satemu.toml";
    }

    const auto &rtc = system.rtc;

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
            {"EmulateSH2Cache", system.emulateSH2Cache},
        
            {"RTC", toml::table{{
                {"Mode", EnumName(rtc.mode)},
                {"HostTimeOffset", rtc.hostTimeOffset},
                {"VirtualBaseTime", rtc.virtBaseTime},
                {"VirtualResetBehavior", EnumName(rtc.virtResetBehavior)},
            }}},
        }}},

        /*{"Input", toml::table{{
            // TODO
        }}},*/

        {"Video", toml::table{{
            {"ThreadedRendering", video.threadedRendering},
            {"ThreadedVDP1", video.threadedVDP1},
        }}},

        {"Audio", toml::table{{
            {"InterpolationMode", EnumName(audio.interpolationMode)},
            {"ThreadedSCSP", audio.threadedSCSP},
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
