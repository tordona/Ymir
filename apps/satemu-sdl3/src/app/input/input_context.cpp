#include "input_context.hpp"

namespace app::input {

Action InputContext::MapAction(Action action, KeyboardKey key, bool pressed) {
    return MapAction(action, InputEvent{key, pressed});
}

Action InputContext::MapAction(Action action, KeyCombo keyCombo, bool pressed) {
    return MapAction(action, InputEvent{keyCombo, pressed});
}

Action InputContext::MapAction(Action action, int index, GamepadButton button, bool pressed) {
    return MapAction(action, InputEvent{index, button, pressed});
}

Action InputContext::MapAction(Action action, int joystickButton, bool pressed) {
    return MapAction(action, InputEvent{joystickButton, pressed});
}

std::pair<Action, Action> InputContext::MapPressAndReleaseAction(Action action, KeyboardKey key) {
    return {MapAction(action, key, true), MapAction(action, key, false)};
}

std::pair<Action, Action> InputContext::MapPressAndReleaseAction(Action action, KeyCombo keyCombo) {
    return {MapAction(action, keyCombo, true), MapAction(action, keyCombo, false)};
}

std::pair<Action, Action> InputContext::MapPressAndReleaseAction(Action action, int index, GamepadButton button) {
    return {MapAction(action, index, button, true), MapAction(action, index, button, false)};
}

std::pair<Action, Action> InputContext::MapPressAndReleaseAction(Action action, int joystickButton) {
    return {MapAction(action, joystickButton, true), MapAction(action, joystickButton, false)};
}

Action InputContext::GetMappedAction(KeyboardKey key, bool pressed) const {
    return GetMappedAction(InputEvent{key, pressed});
}

Action InputContext::GetMappedAction(KeyCombo keyCombo, bool pressed) const {
    return GetMappedAction(InputEvent{keyCombo, pressed});
}

Action InputContext::GetMappedAction(int index, GamepadButton button, bool pressed) const {
    return GetMappedAction(InputEvent{index, button, pressed});
}

Action InputContext::GetMappedAction(int joystickButton, bool pressed) const {
    return GetMappedAction(InputEvent{joystickButton, pressed});
}

const std::unordered_map<InputEvent, Action, typename InputEvent::Hash> &InputContext::GetMappedActions() const {
    return m_actions;
}

InputEvent InputContext::GetMappedInput(Action action) const {
    if (m_actionsReverse.contains(action)) {
        return m_actionsReverse.at(action);
    }
    return {};
}

const std::unordered_map<Action, InputEvent> &InputContext::GetMappedInputs() const {
    return m_actionsReverse;
}

InputEvent InputContext::UnmapAction(Action action) {
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

void InputContext::ProcessGamepadEvent(int index, GamepadButton button, bool pressed) {
    ProcessEvent(InputEvent{index, button, pressed});
}

void InputContext::ProcessJoystickEvent(int button, bool pressed) {
    ProcessEvent(InputEvent{button, pressed});
}

bool InputContext::TryPollNextEvent(InputActionEvent &event) {
    return m_actionQueue.try_dequeue(event);
}

Action InputContext::MapAction(Action action, InputEvent &&event) {
    Action prev;
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

Action InputContext::GetMappedAction(InputEvent &&event) const {
    return m_actions.contains(event) ? m_actions.at(event) : kNoAction;
}

void InputContext::ProcessEvent(InputEvent &&event) {
    if (m_actions.contains(event)) {
        auto action = m_actions.at(event);
        m_actionQueue.enqueue(InputActionEvent{.input = event, .action = action});
    }
}

} // namespace app::input
