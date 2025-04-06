#pragma once

#include <satemu/core/types.hpp>

#include <chrono>
#include <filesystem>

namespace app {

enum class RTCMode { Host, Emulated };
enum class EmulatedRTCResetBehavior { SyncToHost, SyncToFixedStartingTime, PreserveCurrentTime };

enum class AudioInterpolationMode { Nearest, Linear };

struct Settings {
    Settings() noexcept {
        ResetToDefaults();
    }

    void ResetToDefaults();
    bool Load(const std::filesystem::path &path, std::error_code &error);
    bool Save(std::error_code &error);

    // Auto-saves if the settings have been dirty for a while
    void CheckDirty();
    void MakeDirty();

    // ---------------------------------------------------------------------------------------------

    std::filesystem::path path;

    struct General {
        bool preloadDiscImagesToRAM;

        bool boostEmuThreadPriority;
        bool boostProcessPriority;
    } general;

    struct System {
        std::string biosPath;

        bool emulateSH2Cache;

        struct RTC {
            RTCMode mode;

            sint64 hostTimeOffset;

            float emuTimeScale;
            sint64 emuBaseTime;
            EmulatedRTCResetBehavior emuResetBehavior;
        } rtc;
    } system;

    struct Input {
        // TODO: remappable key bindings (support multiple binds per button)
    } input;

    struct Audio {
        AudioInterpolationMode interpolationMode;
        bool threadedSCSP;
    } audio;

    struct Video {
        bool threadedRendering;
        bool threadedVDP1;
    } video;

private:
    bool m_dirty = false;
    std::chrono::steady_clock::time_point m_dirtyTimestamp;
};

} // namespace app
