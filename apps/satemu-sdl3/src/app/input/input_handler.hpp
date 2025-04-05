#pragma once

#include "input_context.hpp"

#include <functional>

namespace app::input {

class InputHandler {
public:
    using ActionHandler = std::function<void(const InputActionEvent &)>;

    InputContext &GetInputContext() {
        return m_inputContext;
    }

    // Registers an action handler to the specified action.
    void Register(ActionID action, ActionHandler handler);

    // Unregisters the action handler from the specified action.
    void Unregister(ActionID action);

    // Processes all pending input events.
    void ProcessInputEvents();

private:
    InputContext m_inputContext;

    std::unordered_map<ActionID, ActionHandler> m_actionHandlers;
};

} // namespace app::input
