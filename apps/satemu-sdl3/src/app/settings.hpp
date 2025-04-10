#pragma once

#include <satemu/core/configuration.hpp>

#include <satemu/hw/smpc/peripheral/peripheral_defs.hpp>

#include <app/input/input_context.hpp>
#include <app/input/input_events.hpp>

#include <satemu/util/observable.hpp>

#include <fmt/format.h>
#include <toml++/toml.hpp>

#include <array>
#include <chrono>
#include <filesystem>
#include <sstream>
#include <variant>

// ---------------------------------------------------------------------------------------------------------------------
// Forward declarations

namespace satemu {

struct Saturn;

} // namespace satemu

// ---------------------------------------------------------------------------------------------------------------------

namespace app {

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

// Number of simultaneous input bindings allowed per input event
inline constexpr size_t kNumBindsPerInput = 4;
using InputEventArray = std::array<input::InputEvent, kNumBindsPerInput>;

struct Settings {
    Settings(satemu::core::Configuration &emuConfig) noexcept
        : m_emuConfig(emuConfig) {
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

    // Clears and rebinds all configured inputs to the context
    void RebindInputs(input::InputContext &ctx, satemu::Saturn &saturn);

    // ---------------------------------------------------------------------------------------------

    std::filesystem::path path;

    struct General {
        bool preloadDiscImagesToRAM;

        bool boostEmuThreadPriority;
        bool boostProcessPriority;
    } general;

    struct System {
        std::string biosPath;
    } system;

    struct Hotkeys {
        InputEventArray loadDisc;
        InputEventArray ejectDisc;
        InputEventArray openCloseTray;

        InputEventArray toggleWindowedVideoOutput;

        InputEventArray openSettings;

        InputEventArray hardReset;
        InputEventArray softReset;

        InputEventArray frameStep;
        InputEventArray pauseResume;
        InputEventArray fastForward;

        InputEventArray resetButton;

        InputEventArray toggleDebugTrace;
        InputEventArray dumpMemory;
    } hotkeys;

    struct Input {
        struct Port {
            util::Observable<satemu::peripheral::PeripheralType> type;

            struct StandardPadBinds {
                InputEventArray a;
                InputEventArray b;
                InputEventArray c;
                InputEventArray x;
                InputEventArray y;
                InputEventArray z;
                InputEventArray l;
                InputEventArray r;
                InputEventArray start;
                InputEventArray up;
                InputEventArray down;
                InputEventArray left;
                InputEventArray right;
            } standardPadBinds;
        };
        Port port1;
        Port port2;
    } input;

    struct Video {
        bool forceIntegerScaling;
        bool forceAspectRatio;
        double forcedAspect;
        bool autoResizeWindow;
        bool displayVideoOutputInWindow;
    } video;

private:
    satemu::core::Configuration &m_emuConfig;

    bool m_dirty = false;
    std::chrono::steady_clock::time_point m_dirtyTimestamp;
};

} // namespace app
