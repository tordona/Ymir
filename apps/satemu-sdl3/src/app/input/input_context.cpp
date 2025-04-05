#include "input_context.hpp"

namespace app::input {

inline constexpr BoundAction kNoBoundAction{kNoAction, 0};

InputContext::InputContext()
    : m_actionQueueToken(m_actionQueue) {}

BoundAction InputContext::MapAction(ActionID action, KeyboardKey key, bool pressed) {
    return MapAction({action, 0}, InputEvent{key, pressed});
}

BoundAction InputContext::MapAction(ActionID action, KeyCombo keyCombo, bool pressed) {
    return MapAction({action, 0}, InputEvent{keyCombo, pressed});
}

BoundAction InputContext::MapAction(ActionID action, uint32 id, GamepadButton button, bool pressed) {
    return MapAction({action, 0}, InputEvent{id, button, pressed});
}

BoundAction InputContext::MapAction(ActionID action, int joystickButton, bool pressed) {
    return MapAction({action, 0}, InputEvent{joystickButton, pressed});
}

std::pair<BoundAction, BoundAction> InputContext::MapToggleableAction(ActionID action, KeyboardKey key) {
    return {MapAction(action, key, true), MapAction(action, key, false)};
}

std::pair<BoundAction, BoundAction> InputContext::MapToggleableAction(ActionID action, KeyCombo keyCombo) {
    return {MapAction(action, keyCombo, true), MapAction(action, keyCombo, false)};
}

std::pair<BoundAction, BoundAction> InputContext::MapToggleableAction(ActionID action, uint32 id,
                                                                      GamepadButton button) {
    return {MapAction(action, id, button, true), MapAction(action, id, button, false)};
}

std::pair<BoundAction, BoundAction> InputContext::MapToggleableAction(ActionID action, int joystickButton) {
    return {MapAction(action, joystickButton, true), MapAction(action, joystickButton, false)};
}

BoundAction InputContext::GetMappedAction(KeyboardKey key, bool pressed) const {
    return GetMappedAction(InputEvent{key, pressed});
}

BoundAction InputContext::GetMappedAction(KeyCombo keyCombo, bool pressed) const {
    return GetMappedAction(InputEvent{keyCombo, pressed});
}

BoundAction InputContext::GetMappedAction(uint32 id, GamepadButton button, bool pressed) const {
    return GetMappedAction(InputEvent{id, button, pressed});
}

BoundAction InputContext::GetMappedAction(int joystickButton, bool pressed) const {
    return GetMappedAction(InputEvent{joystickButton, pressed});
}

const std::unordered_map<InputEvent, BoundAction> &InputContext::GetMappedActions() const {
    return m_actions;
}

std::unordered_set<InputEvent> InputContext::GetMappedInputs(ActionID action, ActionContext context) const {
    const BoundAction boundAction{action, context};
    if (m_actionsReverse.contains(boundAction)) {
        return m_actionsReverse.at(boundAction);
    }
    return {};
}

const std::unordered_map<BoundAction, std::unordered_set<InputEvent>> &InputContext::GetAllMappedInputs() const {
    return m_actionsReverse;
}

std::unordered_set<InputEvent> InputContext::UnmapAction(ActionID action, ActionContext context) {
    const BoundAction boundAction{action, context};
    if (m_actionsReverse.contains(boundAction)) {
        std::unordered_set<InputEvent> evts = m_actionsReverse[boundAction];
        for (auto &evt : evts) {
            m_actions.erase(evt);
        }
        m_actionsReverse.erase(boundAction);
        return evts;
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

BoundAction InputContext::MapAction(BoundAction action, InputEvent &&event) {
    BoundAction prev;
    if (m_actions.contains(event)) {
        prev = m_actions.at(event);
        m_actionsReverse[prev].erase(event);
        if (m_actionsReverse[prev].empty()) {
            m_actionsReverse.erase(prev);
        }
    } else {
        prev = kNoBoundAction;
    }
    m_actions[event] = action;
    m_actionsReverse[action].insert(event);
    return prev;
}

BoundAction InputContext::GetMappedAction(InputEvent &&event) const {
    return m_actions.contains(event) ? m_actions.at(event) : kNoBoundAction;
}

void InputContext::ProcessEvent(InputEvent &&event) {
    if (m_actions.contains(event)) {
        auto action = m_actions.at(event);
        m_actionQueue.enqueue(m_actionQueueToken, InputActionEvent{.input = event, .action = action});
    }
}

} // namespace app::input
