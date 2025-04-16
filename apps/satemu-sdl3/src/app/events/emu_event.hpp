#pragma once

#include <satemu/sys/backup_ram.hpp>

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

        SetPaused,
        ForwardFrameStep,
        ReverseFrameStep,

        OpenCloseTray,
        LoadDisc,
        EjectDisc,

        RemoveCartridge,

        ReplaceInternalBackupMemory,
        ReplaceExternalBackupMemory,

        RunFunction,

        SetThreadPriority,

        Shutdown,
    };

    Type type;

    std::variant<std::monostate, bool, std::string, satemu::bup::BackupMemory, std::function<void(SharedContext &)>>
        value;
};

} // namespace app
