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
        DebugWriteMain,
        DebugWriteSH2,
        DebugDivide,

        OpenCloseTray,
        LoadDisc,
        EjectDisc,

        Shutdown
    };

    Type type;

    // TODO: support 16-bit and 32-bit reads/writes
    struct DebugWriteMainData {
        uint32 address;
        uint8 value;
        bool enableSideEffects;
    };
    struct DebugWriteSH2Data {
        uint32 address;
        uint8 value;
        bool enableSideEffects;
        bool bypassSH2Cache;
        bool master;
    };
    struct DebugDivideData {
        bool div64;
        bool master;
    };

    std::variant<bool, std::string, DebugWriteMainData, DebugWriteSH2Data, DebugDivideData> value;

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

    static EmuEvent DebugWriteMain(uint32 address, uint8 value, bool enableSideEffects) {
        return {.type = Type::DebugWriteMain,
                .value =
                    DebugWriteMainData{.address = address, .value = value, .enableSideEffects = enableSideEffects}};
    }

    static EmuEvent DebugWriteSH2(uint32 address, uint8 value, bool enableSideEffects, bool bypassSH2Cache,
                                  bool master) {
        return {.type = Type::DebugWriteSH2,
                .value = DebugWriteSH2Data{.address = address,
                                           .value = value,
                                           .enableSideEffects = enableSideEffects,
                                           .bypassSH2Cache = bypassSH2Cache,
                                           .master = master}};
    }

    static EmuEvent DebugDivide(bool div64, bool master) {
        return {.type = Type::DebugDivide, .value = DebugDivideData{.div64 = div64, .master = master}};
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
