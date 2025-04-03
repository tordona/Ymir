#pragma once

namespace app {

struct GUIEvent {
    enum class Type {
        LoadDisc,
        OpenBackupMemoryCartFileDialog,

        OpenBackupMemoryManager,
    };

    Type type;
};

} // namespace app
