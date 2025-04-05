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

    // Maps a keyboard key to an action and returns the previously mapped action.
    BoundAction MapAction(ActionID action, KeyboardKey key, bool pressed = true);
    // Maps a key combo to an action and returns the previously mapped action.
    BoundAction MapAction(ActionID action, KeyCombo keyCombo, bool pressed = true);
    // Maps a gamepad button to an action and returns the previously mapped action.
    BoundAction MapAction(ActionID action, uint32 id, GamepadButton button, bool pressed = true);
    // Maps a joystick button to an action and returns the previously mapped action.
    BoundAction MapAction(ActionID action, int joystickButton, bool pressed = true);

    // Maps a keyboard key press and release to an action and returns the previously mapped action.
    std::pair<BoundAction, BoundAction> MapToggleableAction(ActionID action, KeyboardKey key);
    // Maps a key combo press and release to an action and returns the previously mapped action.
    std::pair<BoundAction, BoundAction> MapToggleableAction(ActionID action, KeyCombo keyCombo);
    // Maps a gamepad button press and release to an action and returns the previously mapped action.
    std::pair<BoundAction, BoundAction> MapToggleableAction(ActionID action, uint32 id, GamepadButton button);
    // Maps a joystick button press and release to an action and returns the previously mapped action.
    std::pair<BoundAction, BoundAction> MapToggleableAction(ActionID action, int joystickButton);

    // Gets the action mapped to the keyboard key.
    BoundAction GetMappedAction(KeyboardKey key, bool pressed = true) const;
    // Gets the action mapped to the key combo.
    BoundAction GetMappedAction(KeyCombo keyCombo, bool pressed = true) const;
    // Gets the action mapped to the gamepad button.
    BoundAction GetMappedAction(uint32 id, GamepadButton button, bool pressed = true) const;
    // Gets the action mapped to the joystick button.
    BoundAction GetMappedAction(int joystickButton, bool pressed = true) const;
    // Gets all mapped actions.
    const std::unordered_map<InputEvent, BoundAction> &GetMappedActions() const;

    // Gets the input events mapped to the action.
    std::unordered_set<InputEvent> GetMappedInputs(ActionID action, ActionContext context = 0) const;
    // Gets all action to input event mappings.
    const std::unordered_map<BoundAction, std::unordered_set<InputEvent>> &GetAllMappedInputs() const;

    // Unmaps the input events from the action and returns the previously mapped input events.
    std::unordered_set<InputEvent> UnmapAction(ActionID action, ActionContext context = 0);
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
    std::unordered_map<InputEvent, BoundAction> m_actions;
    std::unordered_map<BoundAction, std::unordered_set<InputEvent>> m_actionsReverse;

    moodycamel::ConcurrentQueue<InputActionEvent> m_actionQueue;
    moodycamel::ProducerToken m_actionQueueToken;

    BoundAction MapAction(BoundAction action, InputEvent &&event);
    BoundAction GetMappedAction(InputEvent &&event) const;

    void ProcessEvent(InputEvent &&event);
};

} // namespace app::input
