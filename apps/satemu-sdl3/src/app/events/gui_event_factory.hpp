#pragma once

#include "gui_event.hpp"

namespace app::events::gui {

inline GUIEvent LoadDisc() {
    return {.type = GUIEvent::Type::LoadDisc};
}

inline GUIEvent OpenBackupMemoryCartFileDialog() {
    return {.type = GUIEvent::Type::OpenBackupMemoryCartFileDialog};
}

inline GUIEvent OpenBackupMemoryManager() {
    return {.type = GUIEvent::Type::OpenBackupMemoryManager};
}

inline GUIEvent OpenFile(FileDialogParams &&params) {
    return {.type = GUIEvent::Type::OpenFile, .value = std::move(params)};
}

inline GUIEvent OpenManyFiles(FileDialogParams &&params) {
    return {.type = GUIEvent::Type::OpenManyFiles, .value = std::move(params)};
}

inline GUIEvent SaveFile(FileDialogParams &&params) {
    return {.type = GUIEvent::Type::SaveFile, .value = std::move(params)};
}

inline GUIEvent SelectFolder(FolderDialogParams &&params) {
    return {.type = GUIEvent::Type::SelectFolder, .value = std::move(params)};
}

inline GUIEvent SetProcessPriority(bool boost) {
    return {.type = GUIEvent::Type::SetProcessPriority, .value = boost};
}

} // namespace app::events::gui
