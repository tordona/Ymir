#include "settings.hpp"

#include <satemu/util/date_time.hpp>
#include <satemu/util/dev_log.hpp>

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

static void ParseEnum(toml::node_view<toml::node> &node, const char *name, sys::VideoStandard &value) {
    value = sys::VideoStandard::NTSC;
    if (auto opt = node[name].value<std::string>()) {
        if (*opt == "NTSC"s) {
            value = sys::VideoStandard::NTSC;
        } else if (*opt == "PAL"s) {
            value = sys::VideoStandard::PAL;
        }
    }
}

static void ParseEnum(toml::node_view<toml::node> &node, const char *name, smpc::rtc::Mode &value) {
    value = smpc::rtc::Mode::Host;
    if (auto opt = node[name].value<std::string>()) {
        if (*opt == "Host"s) {
            value = smpc::rtc::Mode::Host;
        } else if (*opt == "Virtual"s) {
            value = smpc::rtc::Mode::Virtual;
        }
    }
}

static void ParseEnum(toml::node_view<toml::node> &node, const char *name, smpc::rtc::HardResetStrategy &value) {
    value = smpc::rtc::HardResetStrategy::Preserve;
    if (auto opt = node[name].value<std::string>()) {
        if (*opt == "PreserveCurrentTime"s) {
            value = smpc::rtc::HardResetStrategy::Preserve;
        } else if (*opt == "SyncToHost"s) {
            value = smpc::rtc::HardResetStrategy::SyncToHost;
        } else if (*opt == "SyncToFixedStartingTime"s) {
            value = smpc::rtc::HardResetStrategy::ResetToFixedTime;
        }
    }
}

static void ParseEnum(toml::node_view<toml::node> &node, const char *name, scsp::Interpolation &value) {
    value = scsp::Interpolation::NearestNeighbor;
    if (auto opt = node[name].value<std::string>()) {
        if (*opt == "Nearest"s) {
            value = scsp::Interpolation::NearestNeighbor;
        } else if (*opt == "Linear"s) {
            value = scsp::Interpolation::Linear;
        }
    }
}

// -------------------------------------------------------------------------------------------------
// Enum-to-string converters

static const char *EnumName(const sys::VideoStandard value) {
    switch (value) {
    default:
    case sys::VideoStandard::NTSC: return "NTSC";
    case sys::VideoStandard::PAL: return "PAL";
    }
}

static const char *EnumName(const smpc::rtc::Mode value) {
    switch (value) {
    default:
    case smpc::rtc::Mode::Host: return "Host";
    case smpc::rtc::Mode::Virtual: return "Virtual";
    }
}

static const char *EnumName(const smpc::rtc::HardResetStrategy value) {
    switch (value) {
    default:
    case smpc::rtc::HardResetStrategy::Preserve: return "Preserve";
    case smpc::rtc::HardResetStrategy::SyncToHost: return "SyncToHost";
    case smpc::rtc::HardResetStrategy::ResetToFixedTime: return "ResetToFixedTime";
    }
}

static const char *EnumName(const scsp::Interpolation value) {
    switch (value) {
    default:
    case scsp::Interpolation::NearestNeighbor: return "Nearest";
    case scsp::Interpolation::Linear: return "Linear";
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

    system.videoStandard = sys::VideoStandard::NTSC;

    system.emulateSH2Cache = false;

    system.rtc.mode = smpc::rtc::Mode::Host;
    system.rtc.hostTimeOffset = 0;
    system.rtc.virtBaseTime = util::datetime::to_timestamp(
        util::datetime::DateTime{.year = 1994, .month = 1, .day = 1, .hour = 0, .minute = 0, .second = 0});
    system.rtc.virtHardResetStrategy = smpc::rtc::HardResetStrategy::Preserve;

    // TODO: input

    video.forceIntegerScaling = true;
    video.forceAspectRatio = false;
    video.forcedAspect = 4.0 / 3.0;
    video.autoResizeWindow = true;
    video.displayVideoOutputInWindow = false;

    video.threadedRendering = true;
    video.threadedVDP1 = true;

    audio.interpolationMode = scsp::Interpolation::Linear;
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
        ParseEnum(tblSystem, "VideoStandard", system.videoStandard);
        ParseSimple(tblSystem, "EmulateSH2Cache", system.emulateSH2Cache);

        auto &rtc = system.rtc;

        if (auto tblRTC = tblSystem["RTC"]) {
            ParseEnum(tblRTC, "Mode", rtc.mode);
            ParseSimple(tblRTC, "HostTimeOffset", rtc.hostTimeOffset);
            ParseSimple(tblRTC, "VirtualBaseTime", rtc.virtBaseTime);
            ParseEnum(tblRTC, "VirtualHardResetStrategy", rtc.virtHardResetStrategy);
        }
    }

    /*if (auto tblInput = data["Input"]) {
        // TODO
    }*/

    if (auto tblVideo = data["Video"]) {
        ParseSimple(tblVideo, "ForceIntegerScaling", video.forceIntegerScaling);
        ParseSimple(tblVideo, "ForceAspectRatio", video.forceAspectRatio);
        ParseSimple(tblVideo, "ForcedAspect", video.forcedAspect);

        ParseSimple(tblVideo, "AutoResizeWindow", video.autoResizeWindow);
        ParseSimple(tblVideo, "DisplayVideoOutputInWindow", video.displayVideoOutputInWindow);

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
            {"VideoStandard", EnumName(system.videoStandard)},
            {"EmulateSH2Cache", system.emulateSH2Cache},
        
            {"RTC", toml::table{{
                {"Mode", EnumName(rtc.mode)},
                {"HostTimeOffset", rtc.hostTimeOffset},
                {"VirtualBaseTime", rtc.virtBaseTime},
                {"VirtualHardResetStrategy", EnumName(rtc.virtHardResetStrategy)},
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
