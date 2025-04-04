#pragma once

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

// Parameters for a save file dialog.
struct SaveFileParams {
    std::string dialogTitle;
    std::filesystem::path defaultPath;
    std::vector<FileDialogFilter> filters;
    void *userdata;
    void (*callback)(void *userdata, const char *const *filelist, int filter);
};

// Parameters for a select directory dialog.
struct SelectDirectoryParams {
    std::string dialogTitle;
    std::filesystem::path defaultPath;
    void *userdata;
    void (*callback)(void *userdata, const char *const *filelist, int filter);
};

struct GUIEvent {
    enum class Type {
        LoadDisc,
        OpenBackupMemoryCartFileDialog,

        // Generic/customizable save file dialog; uses SaveFileParams
        SaveFile,
        // Generic/customizable select directory dialog; uses SelectDirectoryParams
        SelectDirectory,

        OpenBackupMemoryManager,
    };

    Type type;
    std::variant<std::monostate, SaveFileParams, SelectDirectoryParams> value;
};

} // namespace app
