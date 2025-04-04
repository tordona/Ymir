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

inline GUIEvent SaveFile(SaveFileParams &&params) {
    return {.type = GUIEvent::Type::SaveFile, .value = std::move(params)};
}

inline GUIEvent SelectDirectory(SelectDirectoryParams &&params) {
    return {.type = GUIEvent::Type::SelectDirectory, .value = std::move(params)};
}

} // namespace app::events::gui
