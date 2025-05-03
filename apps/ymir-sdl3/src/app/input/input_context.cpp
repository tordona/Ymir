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
    // Canonicalize key event by converting modifier key presses into modifiers.
    // The "None" key is used for key modifier-only elements (e.g. Ctrl+Shift).
    switch (key) {
    case KeyboardKey::LeftControl: [[fallthrough]];
    case KeyboardKey::RightControl:
        key = KeyboardKey::None;
        modifiers |= KeyModifier::Control;
        break;
    case KeyboardKey::LeftShift: [[fallthrough]];
    case KeyboardKey::RightShift:
        key = KeyboardKey::None;
        modifiers |= KeyModifier::Shift;
        break;
    case KeyboardKey::LeftAlt: [[fallthrough]];
    case KeyboardKey::RightAlt:
        key = KeyboardKey::None;
        modifiers |= KeyModifier::Alt;
        break;
    case KeyboardKey::LeftGui: [[fallthrough]];
    case KeyboardKey::RightGui:
        key = KeyboardKey::None;
        modifiers |= KeyModifier::Super;
        break;
    default: break;
    }

    m_currModifiers = modifiers;
    const auto index = static_cast<size_t>(key);
    if (m_keyStates[index] != pressed || key == KeyboardKey::None) {

        if (key != KeyboardKey::None) {
            m_keyStates[index] = pressed;
        }
        ProcessEvent({.element = {KeyCombo{modifiers, key}}, .buttonPressed = pressed});
    }
}

void InputContext::ProcessPrimitive(MouseButton button, bool pressed) {
    const auto index = static_cast<size_t>(button);
    if (m_mouseButtonStates[index] != pressed) {
        m_mouseButtonStates[index] = pressed;
        ProcessEvent({.element = {MouseCombo{m_currModifiers, button}}, .buttonPressed = pressed});
    }
}

void InputContext::ProcessPrimitive(uint32 id, GamepadButton button, bool pressed) {
    const auto index = static_cast<size_t>(button);
    if (m_gamepadButtonStates[index] != pressed) {
        m_gamepadButtonStates[index] = pressed;
        ProcessEvent({.element = {id, button}, .buttonPressed = pressed});

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

    // Apply deadzone mapping to LS/RS values.
    // Convert triggers into button presses once they reach the threshold.
    switch (axis) {
    case GamepadAxis1D::LeftStickX: value = ApplyDeadzone(value, GamepadLSDeadzones.x); break;
    case GamepadAxis1D::LeftStickY: value = ApplyDeadzone(value, GamepadLSDeadzones.y); break;
    case GamepadAxis1D::RightStickX: value = ApplyDeadzone(value, GamepadRSDeadzones.x); break;
    case GamepadAxis1D::RightStickY: value = ApplyDeadzone(value, GamepadRSDeadzones.y); break;
    case GamepadAxis1D::LeftTrigger:
        ProcessPrimitive(id, GamepadButton::LeftTrigger, value >= GamepadTriggerToButtonThreshold);
        break;
    case GamepadAxis1D::RightTrigger:
        ProcessPrimitive(id, GamepadButton::RightTrigger, value >= GamepadTriggerToButtonThreshold);
        break;
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
        ProcessEvent({.element = {axis}, .axis1DValue = state.value});
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
            ProcessEvent({.element = {id, axis}, .axis1DValue = state.value});
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

void InputContext::ClearAllKeyboardInputs() {
    for (size_t i = 0; i < m_keyStates.size(); ++i) {
        if (m_keyStates[i]) {
            m_keyStates[i] = false;
            const auto key = static_cast<KeyboardKey>(i);
            ProcessEvent({.element = {KeyCombo{m_currModifiers, key}}, .buttonPressed = false});
        }
    }
    if (m_currModifiers != KeyModifier::None) {
        for (size_t i = 0; i < m_mouseButtonStates.size(); ++i) {
            if (m_mouseButtonStates[i]) {
                const auto button = static_cast<MouseButton>(i);
                ProcessEvent({.element = {MouseCombo{m_currModifiers, button}}, .buttonPressed = false});
            }
        }
    }
    m_currModifiers = KeyModifier::None;
}

void InputContext::ClearAllMouseInputs() {
    for (size_t i = 0; i < m_mouseButtonStates.size(); ++i) {
        if (m_mouseButtonStates[i]) {
            m_mouseButtonStates[i] = false;
            const auto button = static_cast<MouseButton>(i);
            ProcessEvent({.element = {MouseCombo{m_currModifiers, button}}, .buttonPressed = false});
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
    if (m_captureCallback) [[unlikely]] {
        if (event.element.type == InputElement::Type::KeyCombo && event.element.keyCombo.key == KeyboardKey::Escape &&
            event.buttonPressed) {
            CancelCapture();
        } else {
            InvokeCaptureCallback(event);
        }
        return;
    }

    if (auto action = m_actions.find(event.element); action != m_actions.end()) {
        switch (event.element.type) {
        case InputElement::Type::None: break;
        case InputElement::Type::KeyCombo: [[fallthrough]];
        case InputElement::Type::MouseCombo: [[fallthrough]];
        case InputElement::Type::GamepadButton:
            if (auto handler = m_buttonHandlers.find(action->second.action); handler != m_buttonHandlers.end()) {
                handler->second(action->second.context, event.element, event.buttonPressed);
            }
            break;
        case InputElement::Type::MouseAxis1D: [[fallthrough]];
        case InputElement::Type::GamepadAxis1D:
            if (auto handler = m_axis1DHandlers.find(action->second.action); handler != m_axis1DHandlers.end()) {
                handler->second(action->second.context, event.element, event.axis1DValue);
            }
            break;
        case InputElement::Type::MouseAxis2D: [[fallthrough]];
        case InputElement::Type::GamepadAxis2D:
            if (auto handler = m_axis2DHandlers.find(action->second.action); handler != m_axis2DHandlers.end()) {
                handler->second(action->second.context, event.element, event.axis2D.x, event.axis2D.y);
            }
            break;
        }
    }
}

void InputContext::InvokeCaptureCallback(const InputEvent &event) {
    if (m_captureCallback && m_captureCallback(event)) {
        m_captureCallback = {};
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Event-action mapping

void InputContext::MapAction(InputElement event, Action action, void *context) {
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

std::unordered_set<MappedInputElement> InputContext::GetMappedInputs(Action action) const {
    if (m_actionsReverse.contains(action)) {
        return m_actionsReverse.at(action);
    } else {
        return {};
    }
}

const std::unordered_map<Action, std::unordered_set<MappedInputElement>> &InputContext::GetAllMappedInputs() const {
    return m_actionsReverse;
}

void InputContext::UnmapAction(Action action) {
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

void InputContext::SetButtonHandler(Action action, ButtonHandler handler) {
    m_buttonHandlers[action] = handler;
}

void InputContext::ClearButtonHandler(Action action) {
    m_buttonHandlers.erase(action);
}

void InputContext::SetAxis1DHandler(Action action, Axis1DHandler handler) {
    m_axis1DHandlers[action] = handler;
}

void InputContext::ClearAxis1DHandler(Action action) {
    m_axis1DHandlers.erase(action);
}

void InputContext::SetAxis2DHandler(Action action, Axis2DHandler handler) {
    m_axis2DHandlers[action] = handler;
}

void InputContext::ClearAxis2DHandler(Action action) {
    m_axis2DHandlers.erase(action);
}

} // namespace app::input
