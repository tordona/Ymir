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
        // The "None" key is used for key modifier-only events (e.g. Ctrl+Shift)
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
                    InvokeCaptureCallback(InputEvent{KeyCombo{modifiers, KeyboardKey::None}});
                }
                break;

            default:
                if (pressed) {
                    InvokeCaptureCallback(InputEvent{KeyCombo{modifiers, key}});
                }
                break;
            }
        } else {
            ProcessEvent(InputEvent{KeyCombo{modifiers, key}}, pressed);
        }
    }
}

void InputContext::ProcessPrimitive(MouseButton button, bool pressed) {
    const auto index = static_cast<size_t>(button);
    if (m_mouseButtonStates[index] != pressed) {
        m_mouseButtonStates[index] = pressed;
        if (m_captureCallback) [[unlikely]] {
            if (pressed) {
                InvokeCaptureCallback(InputEvent{MouseCombo{m_currModifiers, button}});
            }
        } else {
            ProcessEvent(InputEvent{MouseCombo{m_currModifiers, button}}, pressed);
        }
    }
}

void InputContext::ProcessPrimitive(uint32 id, GamepadButton button, bool pressed) {
    const auto index = static_cast<size_t>(button);
    if (m_gamepadButtonStates[index] != pressed) {
        m_gamepadButtonStates[index] = pressed;
        if (m_captureCallback) [[unlikely]] {
            if (pressed) {
                InvokeCaptureCallback(InputEvent{id, button});
            }
        } else {
            ProcessEvent(InputEvent{id, button}, pressed);
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
            m_mouseAxes2D[index2D].h = value;
        } else {
            m_mouseAxes2D[index2D].v = value;
        }
        m_mouseAxes2D[index2D].changed = true;
    }

    m_axesDirty = true;
}

void InputContext::ProcessPrimitive(uint32 id, GamepadAxis1D axis, float value) {
    const GamepadAxis2D axis2D = Get2DAxisFrom1DAxis(axis);
    const auto index1D = static_cast<size_t>(axis);

    m_gamepadAxes1D[id][index1D].value = value;
    m_gamepadAxes1D[id][index1D].changed = true;

    if (axis2D != GamepadAxis2D::None) {
        const auto index2D = static_cast<size_t>(axis2D);
        if (GetAxisDirection(axis) == AxisDirection::Horizontal) {
            m_gamepadAxes2D[id][index2D].h = value;
        } else {
            m_gamepadAxes2D[id][index2D].v = value;
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
        // TODO: map to action
        // axis, state.value
    }
    for (size_t i = 0; i < m_mouseAxes2D.size(); ++i) {
        const auto axis = static_cast<MouseAxis2D>(i);
        Axis2D &state = m_mouseAxes2D[i];
        if (!state.changed) {
            continue;
        }
        state.changed = false;
        // TODO: map to action
        // axis, state.h, state.v
    }

    for (auto &[id, axes] : m_gamepadAxes1D) {
        for (size_t i = 0; i < axes.size(); ++i) {
            const auto axis = static_cast<GamepadAxis1D>(i);
            Axis1D &state = axes[i];
            if (!state.changed) {
                continue;
            }
            state.changed = false;
            // TODO: map to action
            // axis, state.value
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
            // TODO: map to action
            // axis, state.h, state.v
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

void InputContext::ProcessEvent(const InputEvent &event, bool actuated) {
    if (auto action = m_actions.find(event); action != m_actions.end()) {
        if (auto handler = m_actionHandlers.find(action->second.action); handler != m_actionHandlers.end()) {
            handler->second(action->second.context, actuated);
        }
    }
}

void InputContext::InvokeCaptureCallback(InputEvent &&event) {
    if (m_captureCallback) {
        m_captureCallback(event);
        m_captureCallback = {};
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Event-action mapping

void InputContext::MapAction(InputEvent event, ActionID action, void *context) {
    if (event.type == InputEvent::Type::None) {
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

std::optional<MappedAction> InputContext::GetMappedAction(InputEvent event) const {
    if (m_actions.contains(event)) {
        return m_actions.at(event);
    } else {
        return std::nullopt;
    }
}

const std::unordered_map<InputEvent, MappedAction> &InputContext::GetMappedActions() const {
    return m_actions;
}

std::unordered_set<MappedInputEvent> InputContext::GetMappedInputs(ActionID action) const {
    if (m_actionsReverse.contains(action)) {
        return m_actionsReverse.at(action);
    } else {
        return {};
    }
}

const std::unordered_map<ActionID, std::unordered_set<MappedInputEvent>> &InputContext::GetAllMappedInputs() const {
    return m_actionsReverse;
}

void InputContext::UnmapAction(ActionID action) {
    if (m_actionsReverse.contains(action)) {
        for (auto &evt : m_actionsReverse.at(action)) {
            m_actions.erase(evt.event);
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

} // namespace app::input
