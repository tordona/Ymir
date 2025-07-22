#pragma once

#include <ymir/core/types.hpp>

namespace app::input {

// Identifies actions within an input context. Apps can use any mapping scheme they wish for actions.
// 4 billion different actions should be more than enough for any kind of app.
//
// Any number of input elements can be mapped to a given action.
struct Action {
    enum class Kind {
        // Invoked when a button is pressed, only once per press.
        Trigger,

        // Invoked when a button is pressed, including keyboard key repeats.
        RepeatableTrigger,

        // Synced to a button or keyboard key.
        Button,

        // Synced to an absolute 1D axis such as sticks, triggers or wheels.
        AbsoluteAxis1D,

        // Synced to an absolute 2D axis such as sticks.
        AbsoluteAxis2D

        // TODO: introduce relative 1D and 2D axes for mouse movement and mouse wheel
    };

    uint32 id;
    Kind kind;
    const char *group;
    const char *name;

    static constexpr Action Trigger(uint32 id, const char *group, const char *name) {
        return {id, Kind::Trigger, group, name};
    }
    static constexpr Action RepeatableTrigger(uint32 id, const char *group, const char *name) {
        return {id, Kind::RepeatableTrigger, group, name};
    }
    static constexpr Action Button(uint32 id, const char *group, const char *name) {
        return {id, Kind::Button, group, name};
    }
    static constexpr Action AbsoluteAxis1D(uint32 id, const char *group, const char *name) {
        return {id, Kind::AbsoluteAxis1D, group, name};
    }
    static constexpr Action AbsoluteAxis2D(uint32 id, const char *group, const char *name) {
        return {id, Kind::AbsoluteAxis2D, group, name};
    }

    constexpr bool operator==(const Action &rhs) const {
        return id == rhs.id && kind == rhs.kind;
    }

    constexpr auto operator<=>(const Action &rhs) const {
        if (id != rhs.id) {
            return id <=> rhs.id;
        }
        return kind <=> rhs.kind;
    }
};

} // namespace app::input

// ---------------------------------------------------------------------------------------------------------------------
// Hashing

template <>
struct std::hash<app::input::Action> {
    std::size_t operator()(const app::input::Action &a) const noexcept {
        return std::hash<uint64>{}(static_cast<uint64>(a.id) | (static_cast<uint64>(a.kind) << 32ull));
    }
};
