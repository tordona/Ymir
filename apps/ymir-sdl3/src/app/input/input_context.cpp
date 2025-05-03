#include "input_context.hpp"

namespace app::input {

InputContext::InputContext() {
    m_keyStates.fill(false);
    m_mouseButtonStates.fill(false);
    m_gamepadButtonStates.fill(false);
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
        ProcessEvent(InputEvent{KeyCombo{modifiers, key}}, pressed);
    }
}

void InputContext::ProcessPrimitive(MouseButton button, bool pressed) {
    const auto index = static_cast<size_t>(button);
    if (m_mouseButtonStates[index] != pressed) {
        m_mouseButtonStates[index] = pressed;
        ProcessEvent(InputEvent{MouseCombo{m_currModifiers, button}}, pressed);
    }
}

void InputContext::ProcessPrimitive(uint32 id, GamepadButton button, bool pressed) {
    const auto index = static_cast<size_t>(button);
    if (m_gamepadButtonStates[index] != pressed) {
        m_gamepadButtonStates[index] = pressed;
        ProcessEvent(InputEvent{id, button}, pressed);
    }
}

void InputContext::ProcessEvent(const InputEvent &event, bool actuated) {
    if (auto action = m_actions.find(event); action != m_actions.end()) {
        if (auto handler = m_actionHandlers.find(action->second.action); handler != m_actionHandlers.end()) {
            handler->second(action->second.context, actuated);
        }
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
