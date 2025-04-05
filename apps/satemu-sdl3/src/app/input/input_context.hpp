#pragma once

#include "input_defs.hpp"

#include <concurrentqueue.h>

#include <unordered_map>
#include <utility>

namespace app::input {

class InputContext {
public:
    // Maps a keyboard key to an action and returns the previously mapped action.
    Action MapAction(Action action, KeyboardKey key, bool pressed = true);
    // Maps a key combo to an action and returns the previously mapped action.
    Action MapAction(Action action, KeyCombo keyCombo, bool pressed = true);
    // Maps a gamepad button to an action and returns the previously mapped action.
    Action MapAction(Action action, int index, GamepadButton button, bool pressed = true);
    // Maps a joystick button to an action and returns the previously mapped action.
    Action MapAction(Action action, int joystickButton, bool pressed = true);

    // Maps a keyboard key press and release to an action and returns the previously mapped action.
    std::pair<Action, Action> MapPressAndReleaseAction(Action action, KeyboardKey key);
    // Maps a key combo press and release to an action and returns the previously mapped action.
    std::pair<Action, Action> MapPressAndReleaseAction(Action action, KeyCombo keyCombo);
    // Maps a gamepad button press and release to an action and returns the previously mapped action.
    std::pair<Action, Action> MapPressAndReleaseAction(Action action, int index, GamepadButton button);
    // Maps a joystick button press and release to an action and returns the previously mapped action.
    std::pair<Action, Action> MapPressAndReleaseAction(Action action, int joystickButton);

    // Gets the action mapped to the keyboard key.
    Action GetMappedAction(KeyboardKey key, bool pressed = true) const;
    // Gets the action mapped to the key combo.
    Action GetMappedAction(KeyCombo keyCombo, bool pressed = true) const;
    // Gets the action mapped to the gamepad button.
    Action GetMappedAction(int index, GamepadButton button, bool pressed = true) const;
    // Gets the action mapped to the joystick button.
    Action GetMappedAction(int joystickButton, bool pressed = true) const;
    const std::unordered_map<InputEvent, Action, typename InputEvent::Hash> &GetMappedActions() const;

    // Gets the input event mapped to the action.
    InputEvent GetMappedInput(Action action) const;
    // Gets all action to input event mappings.
    const std::unordered_map<Action, InputEvent> &GetMappedInputs() const;

    // Unmaps the input event from the action and returns the previously mapped input event.
    InputEvent UnmapAction(Action action);
    // Clears all action mappings.
    void UnmapAllActions();

    // Processes a keyboard event.
    void ProcessKeyboardEvent(KeyboardKey key, KeyModifier modifiers, bool pressed);
    // Processes a gamepad event.
    void ProcessGamepadEvent(int index, GamepadButton button, bool pressed);
    // Processes a joystick event.
    void ProcessJoystickEvent(int button, bool pressed);

    // Attempts to poll the next input action event.
    // Returns true if there is an event to process, false otherwise.
    bool TryPollNextEvent(InputActionEvent &event);

private:
    std::unordered_map<InputEvent, Action, typename InputEvent::Hash> m_actions;
    std::unordered_map<Action, InputEvent> m_actionsReverse;

    moodycamel::ConcurrentQueue<InputActionEvent> m_actionQueue;

    Action MapAction(Action action, InputEvent &&event);
    Action GetMappedAction(InputEvent &&event) const;

    void ProcessEvent(InputEvent &&event);
};

} // namespace app::input
