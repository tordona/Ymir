#pragma once

#include "input_action.hpp"
#include "input_events.hpp"

#include <satemu/core/types.hpp>

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
    // -----------------------------------------------------------------------------------------------------------------
    // Input primitive processing

    // Processes a keyboard primitive.
    void ProcessPrimitive(KeyboardKey key, KeyModifier modifiers, bool pressed);
    // Processes a mouse button primitive.
    void ProcessPrimitive(MouseButton button, KeyModifier modifiers, bool pressed);
    // Processes a gamepad button primitive.
    void ProcessPrimitive(uint32 id, GamepadButton button, bool pressed);

private:
    void ProcessEvent(const InputEvent &event, bool actuated);

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
