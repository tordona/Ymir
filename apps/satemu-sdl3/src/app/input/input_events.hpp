#pragma once

#include "input_primitives.hpp"

#include <satemu/core/types.hpp>

namespace app::input {

// An input event includes keyboard and mouse combos as well as gamepad buttons.
struct InputEvent {
    enum class Type {
        None,
        KeyCombo,
        MouseCombo,
        GamepadButton,
    };
    Type type = Type::None;

    union {
        KeyCombo keyCombo;
        MouseCombo mouseCombo;
        struct {
            uint32 id;
            GamepadButton button;
        } gamepad;
    };

    InputEvent()
        : type(Type::None) {}

    InputEvent(KeyCombo keyCombo)
        : type(Type::KeyCombo)
        , keyCombo(keyCombo) {}

    InputEvent(MouseCombo mouseCombo)
        : type(Type::MouseCombo)
        , mouseCombo(mouseCombo) {}

    InputEvent(uint32 id, GamepadButton button)
        : type(Type::GamepadButton)
        , gamepad{.id = id, .button = button} {}

    constexpr bool operator==(const InputEvent &rhs) const {
        if (type != rhs.type) {
            return false;
        }
        switch (type) {
        case Type::None: return true;
        case Type::KeyCombo: return keyCombo == rhs.keyCombo;
        case Type::MouseCombo: return mouseCombo == rhs.mouseCombo;
        case Type::GamepadButton: return gamepad.id == rhs.gamepad.id && gamepad.button == rhs.gamepad.button;
        default: return false;
        }
    }
};

// TODO: axis events

// ---------------------------------------------------------------------------------------------------------------------
// Human-readable string converters

std::string ToHumanString(const InputEvent &bind);

// ---------------------------------------------------------------------------------------------------------------------
// String converters

std::string ToString(const InputEvent &bind);

// ---------------------------------------------------------------------------------------------------------------------
// String parsers

bool TryParse(std::string_view str, InputEvent &event);

} // namespace app::input

// ---------------------------------------------------------------------------------------------------------------------
// Hashing

template <>
struct std::hash<app::input::InputEvent> {
    std::size_t operator()(const app::input::InputEvent &e) const noexcept {
        using Type = app::input::InputEvent::Type;

        std::size_t base = 0;
        switch (e.type) {
        case Type::None: base = 0; break;
        case Type::KeyCombo:
            base = static_cast<std::size_t>(e.keyCombo.key) | (static_cast<std::size_t>(e.keyCombo.modifiers) << 24ull);
            break;
        case Type::MouseCombo:
            base = static_cast<std::size_t>(e.mouseCombo.button) |
                   (static_cast<std::size_t>(e.mouseCombo.modifiers) << 24ull);
            break;
        case Type::GamepadButton:
            base = static_cast<std::size_t>(e.gamepad.button) | (static_cast<std::size_t>(e.gamepad.id) << 24ull);
            break;
        }
        return base | (static_cast<std::size_t>(e.type) << 32ull);
    }
};
