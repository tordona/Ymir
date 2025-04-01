#pragma once

#include "gui_event.hpp"

namespace app::events::gui {

inline GUIEvent LoadDisc() {
    return {.type = GUIEvent::Type::LoadDisc};
}

} // namespace app::events::gui
