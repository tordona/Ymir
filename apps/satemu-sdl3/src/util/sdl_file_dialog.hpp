#pragma once

#include <satemu/util/dev_log.hpp>

#include <SDL3/SDL_dialog.h>

#include <cassert>
#include <concepts>

namespace util {

namespace grp {

    // -----------------------------------------------------------------------------
    // Dev log groups

    // Hierarchy:
    //
    // base

    struct base {
        static constexpr bool enabled = true;
        static constexpr devlog::Level level = devlog::level::debug;
        static constexpr std::string_view name = "FileDialog";
    };

} // namespace grp

// SDL3 file dialog callback function wrapping a function with the following signature:
//   void (*)(void *userdata, const char *selection, int filter)
// The wrapper function expects only one file or directory to be selected. Useful for save file or open directory
// dialogs.
//
// If a file is selected, the wrapped callback is invoked.
// If multiple files are selected, the callback is invoked with the first file in the selection. In debug builds, an
// assertion is raised in this case.
// If the user cancels or the file dialog fails to open, a dev log message is printed.
template <auto callback>
    requires std::invocable<decltype(callback), void *, const char *, int>
void WrapSingleSelectionCallback(void *userdata, const char *const *filelist, int filter) {
    if (filelist == nullptr) {
        devlog::error<grp::base>("Failed to open generic file dialog: {}", SDL_GetError());
    } else if (*filelist == nullptr) {
        devlog::info<grp::base>("Generic file dialog cancelled");
    } else {
        // Only one file or directory should be selected
        assert(filelist[1] == nullptr);
        const char *file = *filelist;
        callback(userdata, file, filter);
    }
}

} // namespace util
