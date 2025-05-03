#include "input_context.hpp"

namespace app::input {

InputContext::InputContext() {
    m_keyStates.fill(false);
    m_mouseButtonStates.fill(false);
    m_gamepadButtonStates.fill(false);

    m_mouseAxes1D.fill({});
    m_gamepadAxes1D.clear();

    m_mouseAxes2D.fill({});
    m_gamepadAxes2D.clear();
}

// ---------------------------------------------------------------------------------------------------------------------
// Input primitive processing

void InputContext::ProcessPrimitive(KeyboardKey key, KeyModifier modifiers, bool pressed) {
    m_currModifiers = modifiers;
    const auto index = static_cast<size_t>(key);
    if (m_keyStates[index] != pressed || key == KeyboardKey::None) {
        // The "None" key is used for key modifier-only elements (e.g. Ctrl+Shift)
        if (key != KeyboardKey::None) {
            m_keyStates[index] = pressed;
        }

        if (m_captureCallback) [[unlikely]] {
            switch (key) {
            case KeyboardKey::Escape: CancelCapture(); break;

            case KeyboardKey::LeftControl:
            case KeyboardKey::RightControl:
            case KeyboardKey::LeftAlt:
            case KeyboardKey::RightAlt:
            case KeyboardKey::LeftShift:
            case KeyboardKey::RightShift:
            case KeyboardKey::LeftGui:
            case KeyboardKey::RightGui:
                if (!pressed) {
                    switch (key) {
                    case KeyboardKey::LeftControl:
                    case KeyboardKey::RightControl: modifiers |= KeyModifier::Control; break;
                    case KeyboardKey::LeftAlt:
                    case KeyboardKey::RightAlt: modifiers |= KeyModifier::Alt; break;
                    case KeyboardKey::LeftShift:
                    case KeyboardKey::RightShift: modifiers |= KeyModifier::Shift; break;
                    case KeyboardKey::LeftGui:
                    case KeyboardKey::RightGui: modifiers |= KeyModifier::Super; break;
                    default: break;
                    }
                    InvokeCaptureCallback(InputElement{KeyCombo{modifiers, KeyboardKey::None}});
                }
                break;

            default:
                if (pressed) {
                    InvokeCaptureCallback(InputElement{KeyCombo{modifiers, key}});
                }
                break;
            }
        } else {
            ProcessEvent({.element = {KeyCombo{modifiers, key}}, .button = pressed});
        }
    }
}

void InputContext::ProcessPrimitive(MouseButton button, bool pressed) {
    const auto index = static_cast<size_t>(button);
    if (m_mouseButtonStates[index] != pressed) {
        m_mouseButtonStates[index] = pressed;
        if (m_captureCallback) [[unlikely]] {
            if (pressed) {
                InvokeCaptureCallback(InputElement{MouseCombo{m_currModifiers, button}});
            }
        } else {
            ProcessEvent({.element = {MouseCombo{m_currModifiers, button}}, .button = pressed});
        }
    }
}

void InputContext::ProcessPrimitive(uint32 id, GamepadButton button, bool pressed) {
    const auto index = static_cast<size_t>(button);
    if (m_gamepadButtonStates[index] != pressed) {
        m_gamepadButtonStates[index] = pressed;
        if (m_captureCallback) [[unlikely]] {
            if (pressed) {
                InvokeCaptureCallback(InputElement{id, button});
            }
        } else {
            ProcessEvent({.element = {id, button}, .button = pressed});
        }

        // Convert D-Pad buttons into axis primitives
        switch (button) {
        case GamepadButton::DpadLeft: ProcessPrimitive(id, GamepadAxis1D::DPadX, pressed ? -1.0f : 0.0f); break;
        case GamepadButton::DpadRight: ProcessPrimitive(id, GamepadAxis1D::DPadX, pressed ? +1.0f : 0.0f); break;
        case GamepadButton::DpadUp: ProcessPrimitive(id, GamepadAxis1D::DPadY, pressed ? -1.0f : 0.0f); break;
        case GamepadButton::DpadDown: ProcessPrimitive(id, GamepadAxis1D::DPadY, pressed ? +1.0f : 0.0f); break;
        default: break;
        }
    }
}

void InputContext::ProcessPrimitive(MouseAxis1D axis, float value) {
    const MouseAxis2D axis2D = Get2DAxisFrom1DAxis(axis);
    const auto index1D = static_cast<size_t>(axis);

    m_mouseAxes1D[index1D].value = value;
    m_mouseAxes1D[index1D].changed = true;

    if (axis2D != MouseAxis2D::None) {
        const auto index2D = static_cast<size_t>(axis2D);
        if (GetAxisDirection(axis) == AxisDirection::Horizontal) {
            m_mouseAxes2D[index2D].x = value;
        } else {
            m_mouseAxes2D[index2D].y = value;
        }
        m_mouseAxes2D[index2D].changed = true;
    }

    m_axesDirty = true;
}

static float ApplyDeadzone(float value, float deadzone) {
    // Limit deadzone to 90%
    deadzone = std::min(deadzone, 0.9f);

    // Map values in the deadzone to 0.0f
    if (abs(value) < deadzone) {
        return 0.0f;
    }

    // Linearly map values outsize of the deadzone to 0.0f..1.0f
    const float sign = value < 0.0f ? -1.0f : +1.0f;
    return sign * (abs(value) - deadzone) / (1.0f - deadzone);
}

void InputContext::ProcessPrimitive(uint32 id, GamepadAxis1D axis, float value) {
    const GamepadAxis2D axis2D = Get2DAxisFrom1DAxis(axis);
    const auto index1D = static_cast<size_t>(axis);

    // Apply deadzone mapping to LS/RS values
    switch (axis) {
    case GamepadAxis1D::LeftStickX: value = ApplyDeadzone(value, GamepadLSDeadzones.x); break;
    case GamepadAxis1D::LeftStickY: value = ApplyDeadzone(value, GamepadLSDeadzones.y); break;
    case GamepadAxis1D::RightStickX: value = ApplyDeadzone(value, GamepadRSDeadzones.x); break;
    case GamepadAxis1D::RightStickY: value = ApplyDeadzone(value, GamepadRSDeadzones.y); break;
    default: break;
    }

    m_gamepadAxes1D[id][index1D].value = value;
    m_gamepadAxes1D[id][index1D].changed = true;

    if (axis2D != GamepadAxis2D::None) {
        const auto index2D = static_cast<size_t>(axis2D);
        if (GetAxisDirection(axis) == AxisDirection::Horizontal) {
            m_gamepadAxes2D[id][index2D].x = value;
        } else {
            m_gamepadAxes2D[id][index2D].y = value;
        }
        m_gamepadAxes2D[id][index2D].changed = true;
    }

    m_axesDirty = true;
}

void InputContext::ProcessAxes() {
    if (!m_axesDirty) {
        return;
    }
    m_axesDirty = false;

    for (size_t i = 0; i < m_mouseAxes1D.size(); ++i) {
        const auto axis = static_cast<MouseAxis1D>(i);
        Axis1D &state = m_mouseAxes1D[i];
        if (!state.changed) {
            continue;
        }
        state.changed = false;
        ProcessEvent({.element = {axis}, .axis1D = state.value});
    }
    for (size_t i = 0; i < m_mouseAxes2D.size(); ++i) {
        const auto axis = static_cast<MouseAxis2D>(i);
        Axis2D &state = m_mouseAxes2D[i];
        if (!state.changed) {
            continue;
        }
        state.changed = false;
        ProcessEvent({.element = {axis}, .axis2D = {.x = state.x, .y = state.y}});
    }

    for (auto &[id, axes] : m_gamepadAxes1D) {
        for (size_t i = 0; i < axes.size(); ++i) {
            const auto axis = static_cast<GamepadAxis1D>(i);
            Axis1D &state = axes[i];
            if (!state.changed) {
                continue;
            }
            state.changed = false;
            ProcessEvent({.element = {id, axis}, .axis1D = state.value});
        }
    }
    for (auto &[id, axes] : m_gamepadAxes2D) {
        for (size_t i = 0; i < axes.size(); ++i) {
            const auto axis = static_cast<GamepadAxis2D>(i);
            Axis2D &state = axes[i];
            if (!state.changed) {
                continue;
            }
            state.changed = false;
            ProcessEvent({.element = {id, axis}, .axis2D = {.x = state.x, .y = state.y}});
        }
    }
}

void InputContext::Capture(CaptureCallback &&callback) {
    m_captureCallback.swap(callback);
}

void InputContext::CancelCapture() {
    m_captureCallback = {};
}

bool InputContext::IsCapturing() const {
    return (bool)m_captureCallback;
}

void InputContext::ProcessEvent(const InputEvent &event) {
    if (auto action = m_actions.find(event.element); action != m_actions.end()) {
        switch (event.element.type) {
        case InputElement::Type::KeyCombo: [[fallthrough]];
        case InputElement::Type::MouseCombo: [[fallthrough]];
        case InputElement::Type::GamepadButton:
            if (auto handler = m_actionHandlers.find(action->second.action); handler != m_actionHandlers.end()) {
                handler->second(action->second.context, event.button);
            }
            break;
        case InputElement::Type::MouseAxis1D: [[fallthrough]];
        case InputElement::Type::GamepadAxis1D:
            if (auto handler = m_axis1DHandlers.find(action->second.action); handler != m_axis1DHandlers.end()) {
                handler->second(action->second.context, event.axis1D);
            }
            break;
        case InputElement::Type::MouseAxis2D: [[fallthrough]];
        case InputElement::Type::GamepadAxis2D:
            if (auto handler = m_axis2DHandlers.find(action->second.action); handler != m_axis2DHandlers.end()) {
                handler->second(action->second.context, event.axis2D.x, event.axis2D.y);
            }
            break;
        }
    }
}

void InputContext::InvokeCaptureCallback(InputElement &&event) {
    if (m_captureCallback && m_captureCallback(event)) {
        m_captureCallback = {};
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Event-action mapping

void InputContext::MapAction(InputElement event, ActionID action, void *context) {
    if (event.type == InputElement::Type::None) {
        return;
    }

    if (m_actions.contains(event)) {
        const auto &prev = m_actions.at(event);
        m_actionsReverse[prev.action].erase({event, context});
        if (m_actionsReverse[prev.action].empty()) {
            m_actionsReverse.erase(prev.action);
        }
    }
    m_actions[event] = {action, context};
    m_actionsReverse[action].insert({event, context});
}

std::optional<MappedAction> InputContext::GetMappedAction(InputElement event) const {
    if (m_actions.contains(event)) {
        return m_actions.at(event);
    } else {
        return std::nullopt;
    }
}

const std::unordered_map<InputElement, MappedAction> &InputContext::GetMappedInputElementActions() const {
    return m_actions;
}

std::unordered_set<MappedInputElement> InputContext::GetMappedInputs(ActionID action) const {
    if (m_actionsReverse.contains(action)) {
        return m_actionsReverse.at(action);
    } else {
        return {};
    }
}

const std::unordered_map<ActionID, std::unordered_set<MappedInputElement>> &InputContext::GetAllMappedInputs() const {
    return m_actionsReverse;
}

void InputContext::UnmapAction(ActionID action) {
    if (m_actionsReverse.contains(action)) {
        for (auto &evt : m_actionsReverse.at(action)) {
            m_actions.erase(evt.element);
        }
        m_actionsReverse.erase(action);
    }
}

void InputContext::UnmapAllActions() {
    m_actions.clear();
    m_actionsReverse.clear();
}

// ---------------------------------------------------------------------------------------------------------------------
// Action handler mapping

void InputContext::SetActionHandler(ActionID action, ActionHandler handler) {
    m_actionHandlers[action] = handler;
}

void InputContext::ClearActionHandler(ActionID action) {
    m_actionHandlers.erase(action);
}

void InputContext::SetAxis1DHandler(ActionID action, Axis1DHandler handler) {
    m_axis1DHandlers[action] = handler;
}

void InputContext::ClearAxis1DHandler(ActionID action) {
    m_axis1DHandlers.erase(action);
}

void InputContext::SetAxis2DHandler(ActionID action, Axis2DHandler handler) {
    m_axis2DHandlers[action] = handler;
}

void InputContext::ClearAxis2DHandler(ActionID action) {
    m_axis2DHandlers.erase(action);
}

} // namespace app::input
