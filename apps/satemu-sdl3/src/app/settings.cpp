#include "settings.hpp"

namespace app {

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

    video.threadedVDPRendering = true;
}

bool Settings::Load(const std::filesystem::path &path, std::error_code &error) {
    error.clear();

    this->path = path;
    return false;
}

bool Settings::Save(std::error_code &error) {
    error.clear();
    if (path.empty()) {
        error.assign(ENOENT, std::generic_category());
        return false;
    }

    return false;
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
