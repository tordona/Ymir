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

} // namespace app::events::gui
