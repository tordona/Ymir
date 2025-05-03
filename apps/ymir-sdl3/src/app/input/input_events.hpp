#pragma once

#include "input_primitives.hpp"

#include <ymir/core/types.hpp>

namespace app::input {

// An input element includes keyboard and mouse combos, gamepad buttons and any axis movement (both 1D and 2D).
struct InputElement {
    enum class Type {
        None,
        KeyCombo,
        MouseCombo,
        GamepadButton,
        MouseAxis1D,
        MouseAxis2D,
        GamepadAxis1D,
        GamepadAxis2D,
    };
    Type type = Type::None;

    union {
        KeyCombo keyCombo;
        MouseCombo mouseCombo;
        struct GamepadButtonEvent {
            uint32 id;
            GamepadButton button;
            constexpr bool operator==(const GamepadButtonEvent &rhs) const = default;
        } gamepadButton;
        struct MouseAxis1DEvent {
            MouseAxis1D axis;
            constexpr bool operator==(const MouseAxis1DEvent &rhs) const = default;
        } mouseAxis1D;
        struct MouseAxis2DEvent {
            MouseAxis2D axis;
            constexpr bool operator==(const MouseAxis2DEvent &rhs) const = default;
        } mouseAxis2D;
        struct GamepadAxis1DEvent {
            uint32 id;
            GamepadAxis1D axis;
            constexpr bool operator==(const GamepadAxis1DEvent &rhs) const = default;
        } gamepadAxis1D;
        struct GamepadAxis2DEvent {
            uint32 id;
            GamepadAxis2D axis;
            constexpr bool operator==(const GamepadAxis2DEvent &rhs) const = default;
        } gamepadAxis2D;
    };

    InputElement()
        : type(Type::None) {}

    InputElement(KeyCombo keyCombo)
        : type(Type::KeyCombo)
        , keyCombo(keyCombo) {}

    InputElement(MouseCombo mouseCombo)
        : type(Type::MouseCombo)
        , mouseCombo(mouseCombo) {}

    InputElement(uint32 id, GamepadButton button)
        : type(Type::GamepadButton)
        , gamepadButton{.id = id, .button = button} {}

    InputElement(MouseAxis1D axis)
        : type(Type::MouseAxis1D)
        , mouseAxis1D{.axis = axis} {}

    InputElement(MouseAxis2D axis)
        : type(Type::MouseAxis2D)
        , mouseAxis2D{.axis = axis} {}

    InputElement(uint32 id, GamepadAxis1D axis)
        : type(Type::GamepadAxis1D)
        , gamepadAxis1D{.id = id, .axis = axis} {}

    InputElement(uint32 id, GamepadAxis2D axis)
        : type(Type::GamepadAxis2D)
        , gamepadAxis2D{.id = id, .axis = axis} {}

    bool IsButton() const {
        return type == Type::KeyCombo || type == Type::MouseCombo || type == Type::GamepadButton;
    }

    bool IsAxis1D() const {
        return type == Type::MouseAxis1D || type == Type::GamepadAxis1D;
    }

    bool IsAxis2D() const {
        return type == Type::MouseAxis2D || type == Type::GamepadAxis2D;
    }

    constexpr bool operator==(const InputElement &rhs) const {
        if (type != rhs.type) {
            return false;
        }
        switch (type) {
        case Type::None: return true;
        case Type::KeyCombo: return keyCombo == rhs.keyCombo;
        case Type::MouseCombo: return mouseCombo == rhs.mouseCombo;
        case Type::GamepadButton: return gamepadButton == rhs.gamepadButton;
        case Type::MouseAxis1D: return mouseAxis1D == rhs.mouseAxis1D;
        case Type::MouseAxis2D: return mouseAxis2D == rhs.mouseAxis2D;
        case Type::GamepadAxis1D: return gamepadAxis1D == rhs.gamepadAxis1D;
        case Type::GamepadAxis2D: return gamepadAxis2D == rhs.gamepadAxis2D;
        default: return false;
        }
    }
};

// An input event is an occurence of an input element with a particular value.
struct InputEvent {
    InputElement element;
    union {
        bool button;
        float axis1D;
        struct {
            float x, y;
        } axis2D;
    };
};

// ---------------------------------------------------------------------------------------------------------------------
// Human-readable string converters

std::string ToHumanString(const InputElement &bind);

// ---------------------------------------------------------------------------------------------------------------------
// String converters

std::string ToString(const InputElement &bind);

// ---------------------------------------------------------------------------------------------------------------------
// String parsers

bool TryParse(std::string_view str, InputElement &event);

} // namespace app::input

// ---------------------------------------------------------------------------------------------------------------------
// Hashing

template <>
struct std::hash<app::input::InputElement> {
    std::size_t operator()(const app::input::InputElement &e) const noexcept {
        using Type = app::input::InputElement::Type;

        std::size_t hash = std::hash<uint8>{}(static_cast<uint8>(e.type));
        switch (e.type) {
        case Type::None: hash = 0; break;
        case Type::KeyCombo:
            hash ^= std::hash<uint8>{}(static_cast<uint8>(e.keyCombo.key));
            hash ^= std::hash<uint8>{}(static_cast<uint8>(e.keyCombo.modifiers)) << 1ull;
            break;
        case Type::MouseCombo:
            hash ^= std::hash<uint8>{}(static_cast<uint8>(e.mouseCombo.button));
            hash ^= std::hash<uint8>{}(static_cast<uint8>(e.mouseCombo.modifiers)) << 1ull;
            break;
        case Type::GamepadButton:
            hash ^= std::hash<uint32>{}(e.gamepadButton.id);
            hash ^= std::hash<uint8>{}(static_cast<uint8>(e.gamepadButton.button)) << 1ull;
            break;
        case Type::MouseAxis1D: hash ^= std::hash<uint8>{}(static_cast<uint8>(e.mouseAxis1D.axis)); break;
        case Type::MouseAxis2D: hash ^= std::hash<uint8>{}(static_cast<uint8>(e.mouseAxis2D.axis)); break;
        case Type::GamepadAxis1D:
            hash ^= std::hash<uint32>{}(e.gamepadAxis1D.id);
            hash ^= std::hash<uint8>{}(static_cast<uint8>(e.gamepadAxis1D.axis)) << 1ull;
            break;
        case Type::GamepadAxis2D:
            hash ^= std::hash<uint32>{}(e.gamepadAxis2D.id);
            hash ^= std::hash<uint8>{}(static_cast<uint8>(e.gamepadAxis2D.axis)) << 1ull;
            break;
        }
        return hash;
    }
};
