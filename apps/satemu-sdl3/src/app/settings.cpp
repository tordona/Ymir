#include "settings.hpp"

#include <toml++/toml.hpp>

namespace app {

inline constexpr int kConfigVersion = 1;

void Settings::ResetToDefaults() {
    general.preloadDiscImagesToRAM = false;
    general.boostEmuThreadPriority = true;
    general.boostProcessPriority = true;

    system.biosPath = "";

    system.emulateSH2Cache = false;

    system.rtc.mode = RTCMode::Host;
    system.rtc.hostTimeOffset = 0;
    system.rtc.emuTimeScale = 1.0f;
    system.rtc.emuBaseTime = 757382400; // 01/01/1994 00:00:00
    system.rtc.emuResetBehavior = EmulatedRTCResetBehavior::PreserveCurrentTime;

    // TODO: input

    audio.interpolationMode = AudioInterpolationMode::Nearest;
    audio.threadedSCSP = true;

    video.threadedRendering = true;
    video.threadedVDP1 = true;
}

bool Settings::Load(const std::filesystem::path &path, std::error_code &error) {
    error.clear();

    // TODO: implement

    this->path = path;
    return false;
}

bool Settings::Save(std::error_code &error) {
    error.clear();
    if (path.empty()) {
        error.assign(ENOENT, std::generic_category());
        return false;
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
            {"BoostProcessPriority", general.boostProcessPriority},
        
            {"RTC", toml::table{{
                {"Mode", rtc.mode},
                {"HostTimeOffset", rtc.hostTimeOffset},
                {"EmuTimeScale", rtc.emuTimeScale},
                {"EmuBaseTime", rtc.emuBaseTime},
                {"EmuResetBehavior", rtc.emuResetBehavior},
            }}},
        }}},

        /*{"Input", toml::table{{
            // TODO
        }}},*/

        {"Audio", toml::table{{
            {"InterpolationMode", audio.interpolationMode},
            {"ThreadedSCSP", audio.threadedSCSP},
        }}},

        {"Video", toml::table{{
            {"ThreadedRendering", video.threadedRendering},
            {"ThreadedVDP1", video.threadedVDP1},
        }}},
    }};
    // clang-format on

    std::ofstream out{path, std::ios::binary | std::ios::trunc};
    out << tbl;

    return true;
}

void Settings::CheckDirty() {
    using namespace std::chrono_literals;

    if (m_dirty && (std::chrono::steady_clock::now() - m_dirtyTimestamp) > 250ms) {
        std::error_code error{};
        if (!Save(error)) {
        }
        m_dirty = false;
    }
}

void Settings::MakeDirty() {
    m_dirty = true;
    m_dirtyTimestamp = std::chrono::steady_clock::now();
}

} // namespace app
