#include "input_handler.hpp"

namespace app::input {

void InputHandler::Register(ActionID action, ActionHandler handler) {
    m_actionHandlers[action] = handler;
}

void InputHandler::Unregister(ActionID action) {
    m_actionHandlers.erase(action);
}

void InputHandler::ProcessInputEvents() {
    // Process all pending input events
    InputActionEvent inputActionEvent;
    while (m_inputContext.TryPollNextEvent(inputActionEvent)) {
        if (auto handler = m_actionHandlers.find(inputActionEvent.action.id); handler != m_actionHandlers.end()) {
            handler->second(inputActionEvent);
        }
    }
}

} // namespace app::input
