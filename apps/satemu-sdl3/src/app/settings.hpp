#pragma once

#include <satemu/core/types.hpp>

#include <fmt/format.h>
#include <toml++/toml.hpp>

#include <chrono>
#include <filesystem>
#include <sstream>
#include <variant>

namespace app {

enum class RTCMode { Host, Virtual };
enum class VirtualRTCResetBehavior { PreserveCurrentTime, SyncToHost, SyncToFixedStartingTime };

enum class AudioInterpolationMode { Nearest, Linear };

struct SettingsLoadResult {
    enum class Type { Success, TOMLParseError, UnsupportedConfigVersion };

    static SettingsLoadResult Success() {
        return {.type = Type::Success};
    }

    static SettingsLoadResult TOMLParseError(toml::parse_error error) {
        return {.type = Type::TOMLParseError, .value = error};
    }

    static SettingsLoadResult UnsupportedConfigVersion(int version) {
        return {.type = Type::UnsupportedConfigVersion, .value = version};
    }

    operator bool() {
        return type == Type::Success;
    }

    std::string string() const {
        switch (type) {
        case Type::Success: return "Success";
        case Type::TOMLParseError: //
        {
            auto &error = std::get<toml::parse_error>(value);
            std::ostringstream ss{};
            ss << error.source();
            return fmt::format("TOML parse error: {} (at {})", error.description(), ss.str());
        }
        case Type::UnsupportedConfigVersion:
            return fmt::format("Unsupported configuration version: {}", std::get<int>(value));
        default: return "Unspecified error";
        }
    }

    Type type;
    std::variant<std::monostate, toml::parse_error, int> value;
};

struct SettingsSaveResult {
    enum class Type { Success, FilesystemError };

    static SettingsSaveResult Success() {
        return {.type = Type::Success};
    }

    static SettingsSaveResult FilesystemError(std::error_code error) {
        return {.type = Type::FilesystemError, .value = error};
    }

    operator bool() {
        return type == Type::Success;
    }

    std::string string() const {
        switch (type) {
        case Type::Success: return "Success";
        case Type::FilesystemError:
            return fmt::format("Filesystem error: {}", std::get<std::error_code>(value).message());
        default: return "Unspecified error";
        }
    }

    Type type;
    std::variant<std::monostate, std::error_code> value;
};

struct Settings {
    Settings() noexcept {
        ResetToDefaults();
    }

    void ResetToDefaults();

    SettingsLoadResult Load(const std::filesystem::path &path);

private:
    SettingsLoadResult LoadV1(toml::table &data);

public:
    SettingsSaveResult Save();

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
            VirtualRTCResetBehavior emuResetBehavior;
        } rtc;
    } system;

    struct Input {
        // TODO: remappable key bindings (support multiple binds per button)
    } input;

    struct Video {
        bool threadedRendering;
        bool threadedVDP1;
    } video;

    struct Audio {
        AudioInterpolationMode interpolationMode;
        bool threadedSCSP;
    } audio;

private:
    bool m_dirty = false;
    std::chrono::steady_clock::time_point m_dirtyTimestamp;
};

} // namespace app
