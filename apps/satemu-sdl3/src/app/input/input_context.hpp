#pragma once

#include "input_defs.hpp"

#include <concurrentqueue.h>

#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace app::input {

class InputContext {
public:
    InputContext();

    // Maps an input event to an action and returns the previously mapped action.
    BoundAction MapAction(ActionID action, InputEvent event, bool pressed = true);
    // Maps an input event to an action with context and returns the previously mapped action.
    BoundAction MapAction(ActionID action, ActionContext context, InputEvent event, bool pressed = true);

    // Maps an input event to a toggleable action and returns the previously mapped action.
    std::pair<BoundAction, BoundAction> MapToggleableAction(ActionID action, InputEvent event);
    // Maps an input event to a toggleable action with context and returns the previously mapped action.
    std::pair<BoundAction, BoundAction> MapToggleableAction(ActionID action, ActionContext context, InputEvent event);

    // Gets the action mapped to the input event.
    BoundAction GetMappedAction(InputEvent event, bool pressed = true) const;

    // Gets all mapped actions.
    const std::unordered_map<BoundInputEvent, BoundAction> &GetMappedActions() const;

    // Gets the input events mapped to the action.
    std::unordered_set<BoundInputEvent> GetMappedInputs(ActionID action, ActionContext context = 0) const;
    // Gets all action to input event mappings.
    const std::unordered_map<BoundAction, std::unordered_set<BoundInputEvent>> &GetAllMappedInputs() const;

    // Unmaps the input events from the action and returns the previously mapped input events.
    std::unordered_set<BoundInputEvent> UnmapAction(ActionID action, ActionContext context = 0);

    // Clears all action mappings.
    void UnmapAllActions();

    // Processes a keyboard event.
    void ProcessKeyboardEvent(KeyboardKey key, KeyModifier modifiers, bool pressed);
    // Processes a gamepad event.
    void ProcessGamepadEvent(uint32 id, GamepadButton button, bool pressed);
    // Processes a joystick event.
    void ProcessJoystickEvent(int button, bool pressed);

    // Attempts to poll the next input action event.
    // Returns true if there is an event to process, false otherwise.
    bool TryPollNextEvent(InputActionEvent &event);

private:
    std::unordered_map<BoundInputEvent, BoundAction> m_actions;
    std::unordered_map<BoundAction, std::unordered_set<BoundInputEvent>> m_actionsReverse;

    moodycamel::ConcurrentQueue<InputActionEvent> m_actionQueue;
    moodycamel::ProducerToken m_actionQueueToken;

    BoundAction MapAction(BoundAction action, BoundInputEvent &&event);
    BoundAction GetMappedAction(BoundInputEvent &&event) const;

    void ProcessEvent(BoundInputEvent &&event);
};

} // namespace app::input
