#pragma once

#include "input_action.hpp"
#include "input_events.hpp"

#include <satemu/core/types.hpp>

#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace app::input {

using SingleShotActionHandler = std::function<void(void *context)>;
using BinaryActionHandler = std::function<void(void *context, bool actuated)>;

struct MappedSingleShotAction {
    SingleShotAction action;
    void *context;

    constexpr bool operator==(const MappedSingleShotAction &rhs) const = default;
};

struct MappedBinaryAction {
    BinaryAction action;
    void *context;

    constexpr bool operator==(const MappedBinaryAction &rhs) const = default;
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
// TODO: use template magic to deduplicate functions based on action type
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
    void ProcessSingleShotEvent(const InputEvent &event);
    void ProcessBinaryEvent(const InputEvent &event, bool actuated);

public:
    // -----------------------------------------------------------------------------------------------------------------
    // Event-action mapping

    // Maps an input event to a single-shot action.
    void MapAction(InputEvent event, SingleShotAction action, void *context = nullptr);
    // Maps an input event to a bianry action.
    void MapAction(InputEvent event, BinaryAction action, void *context = nullptr);

    // TODO: consolidate both types of actions into one
    // Gets the single-shot action mapped to the input event, if any.
    std::optional<MappedSingleShotAction> GetMappedSingleShotAction(InputEvent event) const;
    // Gets the binary action mapped to the input event, if any.
    std::optional<MappedBinaryAction> GetMappedBinaryAction(InputEvent event) const;

    // Gets all mapped single-shot actions.
    const std::unordered_map<InputEvent, MappedSingleShotAction> &GetMappedSingleShotActions() const;
    // Gets all mapped binary actions.
    const std::unordered_map<InputEvent, MappedBinaryAction> &GetMappedBinaryActions() const;

    // Gets the input events mapped to the single-shot action.
    std::unordered_set<MappedInputEvent> GetMappedInputs(SingleShotAction action) const;
    // Gets the input events mapped to the binary action.
    std::unordered_set<MappedInputEvent> GetMappedInputs(BinaryAction action) const;

    // Gets all single-shot action to input event mappings.
    const std::unordered_map<SingleShotAction, std::unordered_set<MappedInputEvent>> &
    GetAllMappedSingleShotInputs() const;
    // Gets all binary action to input event mappings.
    const std::unordered_map<BinaryAction, std::unordered_set<MappedInputEvent>> &GetAllMappedBinaryInputs() const;

    // Unmaps the input events from the single-shot action.
    void UnmapAction(SingleShotAction action);
    // Unmaps the input events from the binary action.
    void UnmapAction(BinaryAction action);

    // Clears all action mappings.
    void UnmapAllActions();

public:
    // -----------------------------------------------------------------------------------------------------------------
    // Action handler mapping

    // Registers a single-shot action handler to handle the specified action.
    void SetActionHandler(SingleShotAction action, SingleShotActionHandler handler);
    // Registers a binary action handler to handle the specified action.
    void SetActionHandler(BinaryAction action, BinaryActionHandler handler);

    // Unregisters the single-shot action handler from the specified action.
    void ClearActionHandler(SingleShotAction action);
    // Unregisters the binary action handler from the specified action.
    void ClearActionHandler(BinaryAction action);

private:
    std::unordered_map<InputEvent, MappedSingleShotAction> m_singleShotActions;
    std::unordered_map<SingleShotAction, std::unordered_set<MappedInputEvent>> m_singleShotActionsReverse;

    std::unordered_map<InputEvent, MappedBinaryAction> m_binaryActions;
    std::unordered_map<BinaryAction, std::unordered_set<MappedInputEvent>> m_binaryActionsReverse;

    std::unordered_map<ActionID, SingleShotActionHandler> m_singleShotActionHandlers;
    std::unordered_map<ActionID, BinaryActionHandler> m_binaryActionHandlers;
};

} // namespace app::input

// ---------------------------------------------------------------------------------------------------------------------
// Hashing

template <>
struct std::hash<app::input::MappedSingleShotAction> {
    std::size_t operator()(const app::input::MappedSingleShotAction &e) const noexcept {
        return std::hash<app::input::SingleShotAction>{}(e.action) ^ std::hash<void *>{}(e.context);
    }
};

template <>
struct std::hash<app::input::MappedBinaryAction> {
    std::size_t operator()(const app::input::MappedBinaryAction &e) const noexcept {
        return std::hash<app::input::BinaryAction>{}(e.action) ^ std::hash<void *>{}(e.context);
    }
};

template <>
struct std::hash<app::input::MappedInputEvent> {
    std::size_t operator()(const app::input::MappedInputEvent &e) const noexcept {
        return std::hash<app::input::InputEvent>{}(e.event) ^ std::hash<void *>{}(e.context);
    }
};
