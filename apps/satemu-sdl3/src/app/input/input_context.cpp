#include "input_context.hpp"

namespace app::input {

InputContext::InputContext()
    : m_actionQueueToken(m_actionQueue) {}

ActionID InputContext::MapAction(ActionID action, KeyboardKey key, bool pressed) {
    return MapAction(action, InputEvent{key, pressed});
}

ActionID InputContext::MapAction(ActionID action, KeyCombo keyCombo, bool pressed) {
    return MapAction(action, InputEvent{keyCombo, pressed});
}

ActionID InputContext::MapAction(ActionID action, uint32 id, GamepadButton button, bool pressed) {
    return MapAction(action, InputEvent{id, button, pressed});
}

ActionID InputContext::MapAction(ActionID action, int joystickButton, bool pressed) {
    return MapAction(action, InputEvent{joystickButton, pressed});
}

std::pair<ActionID, ActionID> InputContext::MapPressAndReleaseAction(ActionID action, KeyboardKey key) {
    return {MapAction(action, key, true), MapAction(action, key, false)};
}

std::pair<ActionID, ActionID> InputContext::MapPressAndReleaseAction(ActionID action, KeyCombo keyCombo) {
    return {MapAction(action, keyCombo, true), MapAction(action, keyCombo, false)};
}

std::pair<ActionID, ActionID> InputContext::MapPressAndReleaseAction(ActionID action, uint32 id, GamepadButton button) {
    return {MapAction(action, id, button, true), MapAction(action, id, button, false)};
}

std::pair<ActionID, ActionID> InputContext::MapPressAndReleaseAction(ActionID action, int joystickButton) {
    return {MapAction(action, joystickButton, true), MapAction(action, joystickButton, false)};
}

ActionID InputContext::GetMappedAction(KeyboardKey key, bool pressed) const {
    return GetMappedAction(InputEvent{key, pressed});
}

ActionID InputContext::GetMappedAction(KeyCombo keyCombo, bool pressed) const {
    return GetMappedAction(InputEvent{keyCombo, pressed});
}

ActionID InputContext::GetMappedAction(uint32 id, GamepadButton button, bool pressed) const {
    return GetMappedAction(InputEvent{id, button, pressed});
}

ActionID InputContext::GetMappedAction(int joystickButton, bool pressed) const {
    return GetMappedAction(InputEvent{joystickButton, pressed});
}

const std::unordered_map<InputEvent, ActionID> &InputContext::GetMappedActions() const {
    return m_actions;
}

InputEvent InputContext::GetMappedInput(ActionID action) const {
    if (m_actionsReverse.contains(action)) {
        return m_actionsReverse.at(action);
    }
    return {};
}

const std::unordered_map<ActionID, InputEvent> &InputContext::GetMappedInputs() const {
    return m_actionsReverse;
}

InputEvent InputContext::UnmapAction(ActionID action) {
    if (m_actionsReverse.contains(action)) {
        auto &evt = m_actionsReverse[action];
        m_actions.erase(evt);
        m_actionsReverse.erase(action);
        return evt;
    }
    return {};
}

void InputContext::UnmapAllActions() {
    m_actions.clear();
    m_actionsReverse.clear();
}

void InputContext::ProcessKeyboardEvent(KeyboardKey key, KeyModifier modifiers, bool pressed) {
    ProcessEvent(InputEvent{key, pressed});
    ProcessEvent(InputEvent{KeyCombo{modifiers, key}, pressed});
}

void InputContext::ProcessGamepadEvent(uint32 id, GamepadButton button, bool pressed) {
    ProcessEvent(InputEvent{id, button, pressed});
}

void InputContext::ProcessJoystickEvent(int button, bool pressed) {
    ProcessEvent(InputEvent{button, pressed});
}

bool InputContext::TryPollNextEvent(InputActionEvent &event) {
    return m_actionQueue.try_dequeue_from_producer(m_actionQueueToken, event);
}

ActionID InputContext::MapAction(ActionID action, InputEvent &&event) {
    ActionID prev;
    if (m_actions.contains(event)) {
        prev = m_actions.at(event);
        m_actionsReverse.erase(prev);
    } else {
        prev = kNoAction;
    }
    m_actions[event] = action;
    m_actionsReverse[action] = event;
    return prev;
}

ActionID InputContext::GetMappedAction(InputEvent &&event) const {
    return m_actions.contains(event) ? m_actions.at(event) : kNoAction;
}

void InputContext::ProcessEvent(InputEvent &&event) {
    if (m_actions.contains(event)) {
        auto action = m_actions.at(event);
        m_actionQueue.enqueue(m_actionQueueToken, InputActionEvent{.input = event, .action = action});
    }
}

} // namespace app::input
