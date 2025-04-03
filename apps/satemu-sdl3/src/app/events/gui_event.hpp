#pragma once

namespace app {

struct GUIEvent {
    enum class Type {
        LoadDisc,
        OpenBackupMemoryCartFileDialog,
    };

    Type type;
};

} // namespace app
