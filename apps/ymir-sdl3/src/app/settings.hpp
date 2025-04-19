#pragma once

#include <ymir/core/configuration.hpp>

#include <ymir/hw/smpc/peripheral/peripheral_defs.hpp>

#include <ymir/db/ipl_db.hpp>

#include <app/input/input_action.hpp>
#include <app/input/input_context.hpp>
#include <app/input/input_events.hpp>

#include <app/actions.hpp>
#include <app/profile.hpp>

#include <ymir/util/observable.hpp>

#include <fmt/format.h>
#include <toml++/toml.hpp>

#include <array>
#include <chrono>
#include <filesystem>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>

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

// Number of simultaneous input binds allowed per input event
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

struct SharedContext;

struct Settings {
    Settings(SharedContext &sharedCtx) noexcept;

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

        bool enableRewindBuffer;
        // TODO: rewind buffer size
        int rewindCompressionLevel;
    } general;

    struct System {
        std::filesystem::path internalBackupRAMImagePath;
        struct IPL {
            bool overrideImage;
            std::filesystem::path path;
            ymir::db::SystemVariant variant;
        } ipl;
    } system;

    struct Hotkeys {
        InputBind openSettings{actions::general::OpenSettings};
        InputBind toggleWindowedVideoOutput{actions::general::ToggleWindowedVideoOutput};

        InputBind toggleMute{actions::audio::ToggleMute};
        InputBind increaseVolume{actions::audio::IncreaseVolume};
        InputBind decreaseVolume{actions::audio::DecreaseVolume};

        InputBind loadDisc{actions::cd_drive::LoadDisc};
        InputBind ejectDisc{actions::cd_drive::EjectDisc};
        InputBind openCloseTray{actions::cd_drive::OpenCloseTray};

        InputBind hardReset{actions::sys::HardReset};
        InputBind softReset{actions::sys::SoftReset};
        InputBind resetButton{actions::sys::ResetButton};

        InputBind turboSpeed{actions::emu::TurboSpeed};
        InputBind pauseResume{actions::emu::PauseResume};
        InputBind fwdFrameStep{actions::emu::ForwardFrameStep};
        InputBind revFrameStep{actions::emu::ReverseFrameStep};
        InputBind rewind{actions::emu::Rewind};
        InputBind toggleRewindBuffer{actions::emu::ToggleRewindBuffer};

        InputBind toggleDebugTrace{actions::dbg::ToggleDebugTrace};
        InputBind dumpMemory{actions::dbg::DumpMemory};

        struct SaveStates {
            InputBind quickLoad{actions::save_states::QuickLoadState};
            InputBind quickSave{actions::save_states::QuickSaveState};

            InputBind select1{actions::save_states::SelectState1};
            InputBind select2{actions::save_states::SelectState2};
            InputBind select3{actions::save_states::SelectState3};
            InputBind select4{actions::save_states::SelectState4};
            InputBind select5{actions::save_states::SelectState5};
            InputBind select6{actions::save_states::SelectState6};
            InputBind select7{actions::save_states::SelectState7};
            InputBind select8{actions::save_states::SelectState8};
            InputBind select9{actions::save_states::SelectState9};
            InputBind select10{actions::save_states::SelectState10};

            InputBind load1{actions::save_states::LoadState1};
            InputBind load2{actions::save_states::LoadState2};
            InputBind load3{actions::save_states::LoadState3};
            InputBind load4{actions::save_states::LoadState4};
            InputBind load5{actions::save_states::LoadState5};
            InputBind load6{actions::save_states::LoadState6};
            InputBind load7{actions::save_states::LoadState7};
            InputBind load8{actions::save_states::LoadState8};
            InputBind load9{actions::save_states::LoadState9};
            InputBind load10{actions::save_states::LoadState10};

            InputBind save1{actions::save_states::SaveState1};
            InputBind save2{actions::save_states::SaveState2};
            InputBind save3{actions::save_states::SaveState3};
            InputBind save4{actions::save_states::SaveState4};
            InputBind save5{actions::save_states::SaveState5};
            InputBind save6{actions::save_states::SaveState6};
            InputBind save7{actions::save_states::SaveState7};
            InputBind save8{actions::save_states::SaveState8};
            InputBind save9{actions::save_states::SaveState9};
            InputBind save10{actions::save_states::SaveState10};
        } saveStates;
    } hotkeys;

    struct Input {
        struct Port {
            util::Observable<ymir::peripheral::PeripheralType> type;

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

    struct Audio {
        util::Observable<float> volume;
        util::Observable<bool> mute;
    } audio;

    struct Cartridge {
        enum Type { None, BackupRAM, DRAM };
        Type type;

        struct BackupRAM {
            std::filesystem::path imagePath;
        } backupRAM;

        struct DRAM {
            enum Capacity { _32Mbit, _8Mbit };
            Capacity capacity;
        } dram;
    } cartridge;

    // ---------------------------------------------------------------------------------------------

    // Restores all default input binds for the specified standard pad
    void ResetBinds(Input::Port::StandardPadBinds &binds);

private:
    ymir::core::Configuration &m_emuConfig;
    input::InputContext &m_inputContext;
    Profile &m_profile;

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
