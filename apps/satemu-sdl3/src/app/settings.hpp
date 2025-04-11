#pragma once

#include <satemu/core/configuration.hpp>

#include <satemu/hw/smpc/peripheral/peripheral_defs.hpp>

#include <app/input/input_action.hpp>
#include <app/input/input_context.hpp>
#include <app/input/input_events.hpp>

#include <app/actions.hpp>

#include <satemu/util/observable.hpp>

#include <fmt/format.h>
#include <toml++/toml.hpp>

#include <array>
#include <chrono>
#include <filesystem>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
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

struct InputBind {
    InputBind(input::ActionID action)
        : action(action) {}

    const input::ActionID action;
    std::array<input::InputEvent, kNumBindsPerInput> events;
};

struct InputBindWithContext {
    InputBind *bind;
    void *context;

    constexpr bool operator==(const InputBindWithContext &) const = default;
};

struct Settings {
    Settings(satemu::Saturn &saturn, input::InputContext &inputContext) noexcept;

    void ResetToDefaults();

    SettingsLoadResult Load(const std::filesystem::path &path);

private:
    SettingsLoadResult LoadV1(toml::table &data);

public:
    SettingsSaveResult Save();

    // Auto-saves if the settings have been dirty for a while
    void CheckDirty();
    void MakeDirty();

    // Clears and rebinds all configured inputs
    void RebindInputs();

    // Clears and rebinds the inputs of the specified action
    void RebindAction(input::ActionID action);

    // Synchronizes input settings with those from the input context
    void SyncInputSettings();

    // Restores all default hotkeys
    void ResetHotkeys();

    // ---------------------------------------------------------------------------------------------

    std::filesystem::path path;

    struct General {
        bool preloadDiscImagesToRAM;

        bool boostEmuThreadPriority;
        bool boostProcessPriority;
    } general;

    struct System {
        std::filesystem::path biosPath;
    } system;

    struct Hotkeys {
        InputBind openSettings{actions::general::OpenSettings};
        InputBind toggleWindowedVideoOutput{actions::general::ToggleWindowedVideoOutput};

        InputBind loadDisc{actions::cd_drive::LoadDisc};
        InputBind ejectDisc{actions::cd_drive::EjectDisc};
        InputBind openCloseTray{actions::cd_drive::OpenCloseTray};

        InputBind hardReset{actions::sys::HardReset};
        InputBind softReset{actions::sys::SoftReset};
        InputBind resetButton{actions::sys::ResetButton};

        InputBind pauseResume{actions::emu::PauseResume};
        InputBind frameStep{actions::emu::FrameStep};
        InputBind fastForward{actions::emu::FastForward};

        InputBind toggleDebugTrace{actions::dbg::ToggleDebugTrace};
        InputBind dumpMemory{actions::dbg::DumpMemory};
    } hotkeys;

    struct Input {
        struct Port {
            util::Observable<satemu::peripheral::PeripheralType> type;

            struct StandardPadBinds {
                InputBind a{actions::std_saturn_pad::A};
                InputBind b{actions::std_saturn_pad::B};
                InputBind c{actions::std_saturn_pad::C};
                InputBind x{actions::std_saturn_pad::X};
                InputBind y{actions::std_saturn_pad::Y};
                InputBind z{actions::std_saturn_pad::Z};
                InputBind l{actions::std_saturn_pad::L};
                InputBind r{actions::std_saturn_pad::R};
                InputBind start{actions::std_saturn_pad::Start};
                InputBind up{actions::std_saturn_pad::Up};
                InputBind down{actions::std_saturn_pad::Down};
                InputBind left{actions::std_saturn_pad::Left};
                InputBind right{actions::std_saturn_pad::Right};
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
    input::InputContext &m_inputContext;

    bool m_dirty = false;
    std::chrono::steady_clock::time_point m_dirtyTimestamp;

    std::unordered_map<input::ActionID, std::unordered_set<InputBindWithContext>> m_actionInputs;
};

} // namespace app

// ---------------------------------------------------------------------------------------------------------------------
// Hashing

template <>
struct std::hash<app::InputBindWithContext> {
    std::size_t operator()(const app::InputBindWithContext &e) const noexcept {
        return std::hash<void *>{}(e.bind) ^ std::hash<void *>{}(e.context);
    }
};
