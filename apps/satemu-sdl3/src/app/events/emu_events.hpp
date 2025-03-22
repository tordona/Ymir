#pragma once

#include <satemu/core/types.hpp>

#include <string>
#include <variant>

namespace app {

struct EmuEvent {
    enum class Type {
        FactoryReset,
        HardReset,
        SoftReset,
        FrameStep,

        SetPaused,

        SetDebugTrace,
        MemoryDump,
        DebugWrite,

        OpenCloseTray,
        LoadDisc,
        EjectDisc,

        Shutdown
    };

    Type type;

    // TODO: support 16-bit and 32-bit reads/writes
    struct DebugWriteData {
        uint32 address;
        uint8 value;
        bool enableSideEffects;
    };

    std::variant<bool, std::string, DebugWriteData> value;

    static EmuEvent FactoryReset() {
        return {.type = Type::FactoryReset};
    }

    static EmuEvent HardReset() {
        return {.type = Type::HardReset};
    }

    static EmuEvent SoftReset(bool resetLevel) {
        return {.type = Type::SoftReset, .value = resetLevel};
    }

    static EmuEvent FrameStep() {
        return {.type = Type::FrameStep};
    }

    static EmuEvent SetPaused(bool paused) {
        return {.type = Type::SetPaused, .value = paused};
    }

    static EmuEvent SetDebugTrace(bool enabled) {
        return {.type = Type::SetDebugTrace, .value = enabled};
    }

    static EmuEvent MemoryDump() {
        return {.type = Type::MemoryDump};
    }

    static EmuEvent DebugWrite(uint32 address, uint8 value, bool enableSideEffects) {
        return {.type = Type::DebugWrite,
                .value = DebugWriteData{.address = address, .value = value, .enableSideEffects = enableSideEffects}};
    }

    static EmuEvent OpenCloseTray() {
        return {.type = Type::OpenCloseTray};
    }

    static EmuEvent LoadDisc(std::string path) {
        return {.type = Type::LoadDisc, .value = path};
    }

    static EmuEvent EjectDisc() {
        return {.type = Type::EjectDisc};
    }

    static EmuEvent Shutdown() {
        return {.type = Type::Shutdown};
    }
};

} // namespace app
