#pragma once

#include <app/input/input_action.hpp>
#include <app/ui/defs/settings_defs.hpp>

#include <filesystem>
#include <string>
#include <variant>
#include <vector>

namespace app {

// A filter for the file dialog.
// Follows SDL3 rules:
// - filters must be specified
// - filters are a list of file extensions, separated by semicolons (e.g. "bmp;jpg;png")
// - use "*" to match all files
struct FileDialogFilter {
    const char *name;
    const char *filters;
};

// Parameters for open/save files dialogs.
struct FileDialogParams {
    std::string dialogTitle;
    std::filesystem::path defaultPath;
    std::vector<FileDialogFilter> filters;
    void *userdata;
    void (*callback)(void *userdata, const char *const *filelist, int filter);
};

// Parameters for select folder dialogs.
struct FolderDialogParams {
    std::string dialogTitle;
    std::filesystem::path defaultPath;
    void *userdata;
    void (*callback)(void *userdata, const char *const *filelist, int filter);
};

// Parameters for opening peripheral binds configuration windows.
struct PeripheralBindsParams {
    uint32 portIndex;
    uint32 slotIndex;
};

struct GUIEvent {
    enum class Type {
        LoadDisc,
        OpenBackupMemoryCartFileDialog,
        OpenPeripheralBindsEditor,

        OpenFile,      // Invoke generic open single file dialog; uses FileDialogParams
        OpenManyFiles, // Invoke generic open multiple files dialog; uses FileDialogParams
        SaveFile,      // Invoke generic save file dialog; uses FileDialogParams
        SelectFolder,  // Invoke generic select folder dialog; uses FolderDialogParams

        OpenBackupMemoryManager,
        OpenSettings, // Opens a specific Settings tab; uses ui::SettingsTab

        SetProcessPriority,

        FitWindowToScreen,

        RebindInputs,
        RebindAction,

        ShowErrorMessage,

        EnableRewindBuffer,

        TryLoadIPLROM,
        ReloadIPLROM,

        // Emulator notifications

        StateSaved, // A save state slot was just saved
    };

    Type type;
    std::variant<std::monostate, bool, uint32, std::string, std::filesystem::path, input::Action, PeripheralBindsParams,
                 FileDialogParams, FolderDialogParams, ui::SettingsTab>
        value;
};

} // namespace app
