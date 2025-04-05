#include "input_context.hpp"

namespace app::input {

inline constexpr BoundAction kNoBoundAction{kNoAction, 0};

InputContext::InputContext()
    : m_actionQueueToken(m_actionQueue) {}

BoundAction InputContext::MapAction(ActionID action, InputEvent event, bool pressed) {
    return MapAction({action, 0}, BoundInputEvent{event, pressed});
}

BoundAction InputContext::MapAction(ActionID action, ActionContext context, InputEvent event, bool pressed) {
    return MapAction({action, context}, BoundInputEvent{event, pressed});
}

std::pair<BoundAction, BoundAction> InputContext::MapToggleableAction(ActionID action, InputEvent event) {
    return {MapAction(action, event, true), MapAction(action, event, false)};
}

std::pair<BoundAction, BoundAction> InputContext::MapToggleableAction(ActionID action, ActionContext context,
                                                                      InputEvent event) {
    return {MapAction(action, context, event, true), MapAction(action, context, event, false)};
}

BoundAction InputContext::GetMappedAction(InputEvent event, bool pressed) const {
    return GetMappedAction(BoundInputEvent{event, pressed});
}

const std::unordered_map<BoundInputEvent, BoundAction> &InputContext::GetMappedActions() const {
    return m_actions;
}

std::unordered_set<BoundInputEvent> InputContext::GetMappedInputs(ActionID action, ActionContext context) const {
    const BoundAction boundAction{action, context};
    if (m_actionsReverse.contains(boundAction)) {
        return m_actionsReverse.at(boundAction);
    }
    return {};
}

const std::unordered_map<BoundAction, std::unordered_set<BoundInputEvent>> &InputContext::GetAllMappedInputs() const {
    return m_actionsReverse;
}

std::unordered_set<BoundInputEvent> InputContext::UnmapAction(ActionID action, ActionContext context) {
    const BoundAction boundAction{action, context};
    if (m_actionsReverse.contains(boundAction)) {
        std::unordered_set<BoundInputEvent> evts = m_actionsReverse[boundAction];
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
    ProcessEvent(BoundInputEvent{{key}, pressed});
    ProcessEvent(BoundInputEvent{{KeyCombo{modifiers, key}}, pressed});
}

void InputContext::ProcessGamepadEvent(uint32 id, GamepadButton button, bool pressed) {
    ProcessEvent(BoundInputEvent{{id, button}, pressed});
}

void InputContext::ProcessJoystickEvent(int button, bool pressed) {
    ProcessEvent(BoundInputEvent{{button}, pressed});
}

bool InputContext::TryPollNextEvent(InputActionEvent &event) {
    return m_actionQueue.try_dequeue_from_producer(m_actionQueueToken, event);
}

BoundAction InputContext::MapAction(BoundAction action, BoundInputEvent &&event) {
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

BoundAction InputContext::GetMappedAction(BoundInputEvent &&event) const {
    return m_actions.contains(event) ? m_actions.at(event) : kNoBoundAction;
}

void InputContext::ProcessEvent(BoundInputEvent &&event) {
    if (m_actions.contains(event)) {
        auto action = m_actions.at(event);
        m_actionQueue.enqueue(m_actionQueueToken, InputActionEvent{.input = event, .action = action});
    }
}

} // namespace app::input
