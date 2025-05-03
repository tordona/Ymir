#pragma once

#include "input_action.hpp"
#include "input_events.hpp"

#include <ymir/core/types.hpp>

#include <array>
#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace app::input {

using ActionHandler = std::function<void(void *context, bool actuated)>;

struct MappedAction {
    ActionID action;
    void *context;

    constexpr bool operator==(const MappedAction &rhs) const = default;
};

struct MappedInputEvent {
    InputEvent event;
    void *context;

    constexpr bool operator==(const MappedInputEvent &rhs) const = default;
};

// An input context encompasses a set of action mappings for a particular context in the application.
// The application can use as many input contexts as needed.
//
// Input handling begins by processing input primitives with the ProcessPrimitive() methods which translate them into
// input events. These are mapped to actions based on mappings configured with the MapAction() methods. Finally, the
// corresponding action handlers set up with SetActionHandler() are invoked.
//
// TODO: implement primitive processing in input_backend_sdl3
class InputContext {
public:
    InputContext();

    // -----------------------------------------------------------------------------------------------------------------
    // Input primitive processing

    // Processes a keyboard primitive.
    void ProcessPrimitive(KeyboardKey key, KeyModifier modifiers, bool pressed);
    // Processes a mouse button primitive.
    void ProcessPrimitive(MouseButton button, bool pressed);
    // Processes a gamepad button primitive.
    void ProcessPrimitive(uint32 id, GamepadButton button, bool pressed);

    // Processes an 1D mouse axis primitive.
    void ProcessPrimitive(MouseAxis1D axis, float value);
    // Processes an 1D gamepad axis primitive.
    void ProcessPrimitive(GamepadAxis1D axis, float value);

    // Processes a 2D mouse axis primitive.
    void ProcessPrimitive(MouseAxis2D axis, float x, float y);
    // Processes a 2D gamepad axis primitive.
    void ProcessPrimitive(GamepadAxis2D axis, float x, float y);

    // -----------------------------------------------------------------------------------------------------------------
    // Input capture

    using CaptureCallback = std::function<void(const InputEvent &)>;

    // Captures the next input event.
    void Capture(CaptureCallback &&callback);
    void CancelCapture();
    bool IsCapturing() const;

private:
    void ProcessEvent(const InputEvent &event, bool actuated);

    CaptureCallback m_captureCallback;

    void InvokeCaptureCallback(InputEvent &&event);

public:
    // -----------------------------------------------------------------------------------------------------------------
    // Event-action mapping

    // Maps an input event to an action.
    void MapAction(InputEvent event, ActionID action, void *context = nullptr);

    // Gets the action mapped to the input event, if any.
    std::optional<MappedAction> GetMappedAction(InputEvent event) const;

    // Gets all mapped actions.
    const std::unordered_map<InputEvent, MappedAction> &GetMappedActions() const;

    // Gets the input events mapped to the action.
    std::unordered_set<MappedInputEvent> GetMappedInputs(ActionID action) const;

    // Gets all action to input event mappings.
    const std::unordered_map<ActionID, std::unordered_set<MappedInputEvent>> &GetAllMappedInputs() const;

    // Unmaps the input events from the action.
    void UnmapAction(ActionID action);

    // Clears all action mappings.
    void UnmapAllActions();

public:
    // -----------------------------------------------------------------------------------------------------------------
    // Action handler mapping

    // Registers an action handler to handle the specified action.
    void SetActionHandler(ActionID action, ActionHandler handler);

    // Unregisters the action handler from the specified action.
    void ClearActionHandler(ActionID action);

private:
    struct Axis2D {
        float h, v;
    };

    KeyModifier m_currModifiers = KeyModifier::None;

    std::array<bool, static_cast<size_t>(KeyboardKey::_Count)> m_keyStates;
    std::array<bool, static_cast<size_t>(MouseButton::_Count)> m_mouseButtonStates;
    std::array<bool, static_cast<size_t>(GamepadButton::_Count)> m_gamepadButtonStates;

    std::array<float, static_cast<size_t>(MouseAxis1D::_Count)> m_mouseAxes1D;
    std::array<float, static_cast<size_t>(GamepadAxis1D::_Count)> m_gamepadAxes1D;

    std::array<Axis2D, static_cast<size_t>(MouseAxis2D::_Count)> m_mouseAxes2D;
    std::array<Axis2D, static_cast<size_t>(GamepadAxis2D::_Count)> m_gamepadAxes2D;

    float m_gamepadDeadzoneX;
    float m_gamepadDeadzoneY;

    std::unordered_map<InputEvent, MappedAction> m_actions;
    std::unordered_map<ActionID, std::unordered_set<MappedInputEvent>> m_actionsReverse;

    std::unordered_map<ActionID, ActionHandler> m_actionHandlers;
};

} // namespace app::input

// ---------------------------------------------------------------------------------------------------------------------
// Hashing

template <>
struct std::hash<app::input::MappedAction> {
    std::size_t operator()(const app::input::MappedAction &e) const noexcept {
        return std::hash<app::input::ActionID>{}(e.action) ^ std::hash<void *>{}(e.context);
    }
};

template <>
struct std::hash<app::input::MappedInputEvent> {
    std::size_t operator()(const app::input::MappedInputEvent &e) const noexcept {
        return std::hash<app::input::InputEvent>{}(e.event) ^ std::hash<void *>{}(e.context);
    }
};
