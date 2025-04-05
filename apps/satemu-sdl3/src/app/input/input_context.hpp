#pragma once

#include "input_defs.hpp"

#include <concurrentqueue.h>

#include <unordered_map>
#include <utility>

namespace app::input {

class InputContext {
public:
    InputContext();

    // Maps a keyboard key to an action and returns the previously mapped action.
    ActionID MapAction(ActionID action, KeyboardKey key, bool pressed = true);
    // Maps a key combo to an action and returns the previously mapped action.
    ActionID MapAction(ActionID action, KeyCombo keyCombo, bool pressed = true);
    // Maps a gamepad button to an action and returns the previously mapped action.
    ActionID MapAction(ActionID action, uint32 id, GamepadButton button, bool pressed = true);
    // Maps a joystick button to an action and returns the previously mapped action.
    ActionID MapAction(ActionID action, int joystickButton, bool pressed = true);

    // Maps a keyboard key press and release to an action and returns the previously mapped action.
    std::pair<ActionID, ActionID> MapPressAndReleaseAction(ActionID action, KeyboardKey key);
    // Maps a key combo press and release to an action and returns the previously mapped action.
    std::pair<ActionID, ActionID> MapPressAndReleaseAction(ActionID action, KeyCombo keyCombo);
    // Maps a gamepad button press and release to an action and returns the previously mapped action.
    std::pair<ActionID, ActionID> MapPressAndReleaseAction(ActionID action, uint32 id, GamepadButton button);
    // Maps a joystick button press and release to an action and returns the previously mapped action.
    std::pair<ActionID, ActionID> MapPressAndReleaseAction(ActionID action, int joystickButton);

    // Gets the action mapped to the keyboard key.
    ActionID GetMappedAction(KeyboardKey key, bool pressed = true) const;
    // Gets the action mapped to the key combo.
    ActionID GetMappedAction(KeyCombo keyCombo, bool pressed = true) const;
    // Gets the action mapped to the gamepad button.
    ActionID GetMappedAction(uint32 id, GamepadButton button, bool pressed = true) const;
    // Gets the action mapped to the joystick button.
    ActionID GetMappedAction(int joystickButton, bool pressed = true) const;
    const std::unordered_map<InputEvent, ActionID, typename InputEvent::Hash> &GetMappedActions() const;

    // Gets the input event mapped to the action.
    InputEvent GetMappedInput(ActionID action) const;
    // Gets all action to input event mappings.
    const std::unordered_map<ActionID, InputEvent> &GetMappedInputs() const;

    // Unmaps the input event from the action and returns the previously mapped input event.
    InputEvent UnmapAction(ActionID action);
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
    std::unordered_map<InputEvent, ActionID, typename InputEvent::Hash> m_actions;
    std::unordered_map<ActionID, InputEvent> m_actionsReverse;

    moodycamel::ConcurrentQueue<InputActionEvent> m_actionQueue;
    moodycamel::ProducerToken m_actionQueueToken;

    ActionID MapAction(ActionID action, InputEvent &&event);
    ActionID GetMappedAction(InputEvent &&event) const;

    void ProcessEvent(InputEvent &&event);
};

} // namespace app::input
