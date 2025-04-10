#include "input_context.hpp"

namespace app::input {

// ---------------------------------------------------------------------------------------------------------------------
// Input primitive processing

void InputContext::ProcessPrimitive(KeyboardKey key, KeyModifier modifiers, bool pressed) {
    const InputEvent evt{{modifiers, key}};
    if (pressed) {
        ProcessSingleShotEvent(evt);
    }
    ProcessBinaryEvent(evt, pressed);
}

void InputContext::ProcessPrimitive(MouseButton button, KeyModifier modifiers, bool pressed) {
    const InputEvent evt{{modifiers, button}};
    if (pressed) {
        ProcessSingleShotEvent(evt);
    }
    ProcessBinaryEvent(evt, pressed);
}

void InputContext::ProcessPrimitive(uint32 id, GamepadButton button, bool pressed) {
    const InputEvent evt{id, button};
    ProcessBinaryEvent(evt, pressed);
}

void InputContext::ProcessSingleShotEvent(const InputEvent &event) {
    if (auto action = m_singleShotActions.find(event); action != m_singleShotActions.end()) {
        if (auto handler = m_singleShotActionHandlers.find(action->second.action.id);
            handler != m_singleShotActionHandlers.end()) {
            handler->second(action->second.context);
        }
    }
}

void InputContext::ProcessBinaryEvent(const InputEvent &event, bool actuated) {
    if (auto action = m_binaryActions.find(event); action != m_binaryActions.end()) {
        if (auto handler = m_binaryActionHandlers.find(action->second.action.id);
            handler != m_binaryActionHandlers.end()) {
            handler->second(action->second.context, actuated);
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Event-action mapping

void InputContext::MapAction(InputEvent event, SingleShotAction action, void *context) {
    if (event.type == InputEvent::Type::None) {
        return;
    }

    if (m_singleShotActions.contains(event)) {
        const auto &prev = m_singleShotActions.at(event);
        m_singleShotActionsReverse[prev.action].erase({event, context});
        if (m_singleShotActionsReverse[prev.action].empty()) {
            m_singleShotActionsReverse.erase(prev.action);
        }
    }
    m_singleShotActions[event] = {action, context};
    m_singleShotActionsReverse[action].insert({event, context});
}

void InputContext::MapAction(InputEvent event, BinaryAction action, void *context) {
    if (event.type == InputEvent::Type::None) {
        return;
    }

    if (m_binaryActions.contains(event)) {
        const auto &prev = m_binaryActions.at(event);
        m_binaryActionsReverse[prev.action].erase({event, context});
        if (m_binaryActionsReverse[prev.action].empty()) {
            m_binaryActionsReverse.erase(prev.action);
        }
    }
    m_binaryActions[event] = {action, context};
    m_binaryActionsReverse[action].insert({event, context});
}

std::optional<MappedSingleShotAction> InputContext::GetMappedSingleShotAction(InputEvent event) const {
    if (m_singleShotActions.contains(event)) {
        return m_singleShotActions.at(event);
    } else {
        return std::nullopt;
    }
}

std::optional<MappedBinaryAction> InputContext::GetMappedBinaryAction(InputEvent event) const {
    if (m_binaryActions.contains(event)) {
        return m_binaryActions.at(event);
    } else {
        return std::nullopt;
    }
}

const std::unordered_map<InputEvent, MappedSingleShotAction> &InputContext::GetMappedSingleShotActions() const {
    return m_singleShotActions;
}

const std::unordered_map<InputEvent, MappedBinaryAction> &InputContext::GetMappedBinaryActions() const {
    return m_binaryActions;
}

std::unordered_set<MappedInputEvent> InputContext::GetMappedInputs(SingleShotAction action) const {
    if (m_singleShotActionsReverse.contains(action)) {
        return m_singleShotActionsReverse.at(action);
    } else {
        return {};
    }
}

std::unordered_set<MappedInputEvent> InputContext::GetMappedInputs(BinaryAction action) const {
    if (m_binaryActionsReverse.contains(action)) {
        return m_binaryActionsReverse.at(action);
    } else {
        return {};
    }
}

const std::unordered_map<SingleShotAction, std::unordered_set<MappedInputEvent>> &
InputContext::GetAllMappedSingleShotInputs() const {
    return m_singleShotActionsReverse;
}

const std::unordered_map<BinaryAction, std::unordered_set<MappedInputEvent>> &
InputContext::GetAllMappedBinaryInputs() const {
    return m_binaryActionsReverse;
}

void InputContext::UnmapAction(SingleShotAction action) {
    if (m_singleShotActionsReverse.contains(action)) {
        for (auto &evt : m_singleShotActionsReverse.at(action)) {
            m_singleShotActions.erase(evt.event);
        }
        m_singleShotActionsReverse.erase(action);
    }
}

void InputContext::UnmapAction(BinaryAction action) {
    if (m_binaryActionsReverse.contains(action)) {
        for (auto &evt : m_binaryActionsReverse.at(action)) {
            m_binaryActions.erase(evt.event);
        }
        m_binaryActionsReverse.erase(action);
    }
}

void InputContext::UnmapAllActions() {
    m_singleShotActions.clear();
    m_singleShotActionsReverse.clear();

    m_binaryActions.clear();
    m_binaryActionsReverse.clear();
}

// ---------------------------------------------------------------------------------------------------------------------
// Action handler mapping

void InputContext::SetActionHandler(SingleShotAction action, SingleShotActionHandler handler) {
    m_singleShotActionHandlers[action.id] = handler;
}

void InputContext::SetActionHandler(BinaryAction action, BinaryActionHandler handler) {
    m_binaryActionHandlers[action.id] = handler;
}

void InputContext::ClearActionHandler(SingleShotAction action) {
    m_singleShotActionHandlers.erase(action.id);
}

void InputContext::ClearActionHandler(BinaryAction action) {
    m_binaryActionHandlers.erase(action.id);
}

} // namespace app::input
