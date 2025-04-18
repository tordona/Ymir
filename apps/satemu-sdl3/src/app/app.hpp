#pragma once

#include "cmdline_opts.hpp"

#include "audio_system.hpp"
#include "shared_context.hpp"

#include "ui/windows/about_window.hpp"
#include "ui/windows/backup_ram_manager_window.hpp"
#include "ui/windows/peripheral_binds_window.hpp"
#include "ui/windows/settings_window.hpp"
#include "ui/windows/system_state_window.hpp"

#include "ui/windows/debug/debug_output_window.hpp"
#include "ui/windows/debug/memory_viewer_window.hpp"
#include "ui/windows/debug/scu_window_set.hpp"
#include "ui/windows/debug/sh2_window_set.hpp"

#include <util/ipl_rom_loader.hpp>

#include <satemu/util/dev_log.hpp>

#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_properties.h>

#include <imgui.h>

#include <filesystem>
#include <string_view>
#include <thread>
#include <vector>

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
        static constexpr std::string_view name = "App";
    };

} // namespace grp

class App {
public:
    App();

    int Run(const CommandLineOptions &options);

private:
    CommandLineOptions m_options;

    SharedContext m_context;
    SDL_PropertiesID m_fileDialogProps;

    std::thread m_emuThread;

    AudioSystem m_audioSystem;

    void RunEmulator();

    void EmulatorThread();

    void RebindInputs();
    void RebindAction(input::ActionID action);

    template <int port>
    void ReadPeripheral(satemu::peripheral::PeripheralReport &report);

    void ScanIPLROMs();
    util::IPLROMLoadResult LoadIPLROM(bool startup);
    std::filesystem::path GetIPLROMPath(bool startup);

    void LoadSaveStates();
    void PersistSaveState(uint32 slot);
    void EnableRewindBuffer(bool enable);
    void ToggleRewindBuffer();

    void OpenLoadDiscDialog();
    void ProcessOpenDiscImageFileDialogSelection(const char *const *filelist, int filter);
    bool LoadDiscImage(std::filesystem::path path);

    void OpenBackupMemoryCartFileDialog();
    void ProcessOpenBackupMemoryCartFileDialogSelection(const char *const *filelist, int filter);

    void InvokeOpenFileDialog(const FileDialogParams &params) const;
    void InvokeOpenManyFilesDialog(const FileDialogParams &params) const;
    void InvokeSaveFileDialog(const FileDialogParams &params) const;
    void InvokeSelectFolderDialog(const FolderDialogParams &params) const;

    void InvokeFileDialog(SDL_FileDialogType type, const char *title, void *filters, int numFilters, bool allowMany,
                          const char *location, void *userdata, SDL_DialogFileCallback callback) const;

    // -----------------------------------------------------------------------------------------------------------------
    // Windows

    void DrawWindows();
    void OpenMemoryViewer();
    void OpenPeripheralBindsEditor(const PeripheralBindsParams &params);

    ui::SystemStateWindow m_systemStateWindow;
    ui::BackupMemoryManagerWindow m_bupMgrWindow;

    ui::SH2WindowSet m_masterSH2WindowSet;
    ui::SH2WindowSet m_slaveSH2WindowSet;

    ui::SCUWindowSet m_scuWindowSet;

    ui::DebugOutputWindow m_debugOutputWindow;

    std::vector<ui::MemoryViewerWindow> m_memoryViewerWindows;

    ui::SettingsWindow m_settingsWindow;
    ui::PeripheralBindsWindow m_periphBindsWindow;
    ui::AboutWindow m_aboutWindow;

    // Error modal dialog

    void DrawErrorModal();

    void OpenSimpleErrorModal(std::string message);
    void OpenErrorModal(std::function<void()> fnContents);

    bool m_openErrorModal = false; // Open error modal on the next frame
    std::function<void()> m_errorModalContents;
};

} // namespace app
