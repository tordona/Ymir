#pragma once

namespace app {

struct GUIEvent {
    enum class Type {
        LoadDisc,
    };

    Type type;
};

} // namespace app
