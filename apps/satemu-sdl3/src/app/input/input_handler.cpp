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
    InputActionEvent inputEvent;
    while (m_inputContext.TryPollNextEvent(inputEvent)) {
        if (auto handler = m_actionHandlers.find(inputEvent.action); handler != m_actionHandlers.end()) {
            handler->second(inputEvent);
        }
    }
}

} // namespace app::input
