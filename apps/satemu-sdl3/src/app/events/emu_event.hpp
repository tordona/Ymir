#pragma once

#include <functional>
#include <string>
#include <variant>

namespace app {

struct SharedContext;

struct EmuEvent {
    enum class Type {
        FactoryReset,
        HardReset,
        SoftReset,
        SetResetButton,

        FrameStep,
        SetPaused,

        OpenCloseTray,
        LoadDisc,
        EjectDisc,
        EjectCartridge,

        RunFunction,

        Shutdown
    };

    Type type;

    std::variant<bool, std::string, std::function<void(SharedContext &)>> value;
};

} // namespace app
