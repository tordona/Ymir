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

        // Synced to an 1D axis.
        Axis1D,

        // Synced to a 2D axis.
        Axis2D
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
    static constexpr Action Axis1D(uint32 id, const char *group, const char *name) {
        return {id, Kind::Axis1D, group, name};
    }
    static constexpr Action Axis2D(uint32 id, const char *group, const char *name) {
        return {id, Kind::Axis2D, group, name};
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
