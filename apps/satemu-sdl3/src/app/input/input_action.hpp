#pragma once

#include <satemu/core/types.hpp>

namespace app::input {

// Distinguishes actions of a given kind within an input context. Apps can use any mapping scheme they wish for actions.
// 4 billion different actions should be more than enough for any kind of app.
//
// Any number of input elements can be mapped to a given action.
using ActionID = uint32;

// Identifier for single-shot actions.
struct SingleShotAction {
    ActionID id;

    constexpr bool operator==(const SingleShotAction &) const = default;
};

// Identifier for binary actions.
struct BinaryAction {
    ActionID id;

    constexpr bool operator==(const BinaryAction &) const = default;
};

} // namespace app::input

// ---------------------------------------------------------------------------------------------------------------------
// Hashing

template <>
struct std::hash<app::input::SingleShotAction> {
    std::size_t operator()(const app::input::SingleShotAction &e) const noexcept {
        return std::hash<app::input::ActionID>{}(e.id);
    }
};

template <>
struct std::hash<app::input::BinaryAction> {
    std::size_t operator()(const app::input::BinaryAction &e) const noexcept {
        return std::hash<app::input::ActionID>{}(e.id);
    }
};
