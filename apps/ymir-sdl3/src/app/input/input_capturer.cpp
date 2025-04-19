#include "input_capturer.hpp"

namespace app::input {

void InputCapturer::ProcessPrimitive(KeyboardKey key, KeyModifier modifiers, bool pressed) {
    switch (key) {
    case KeyboardKey::Escape: CancelCapture(); break;

    case KeyboardKey::LeftControl:
    case KeyboardKey::RightControl:
    case KeyboardKey::LeftAlt:
    case KeyboardKey::RightAlt:
    case KeyboardKey::LeftShift:
    case KeyboardKey::RightShift:
    case KeyboardKey::LeftGui:
    case KeyboardKey::RightGui:
        if (!pressed) {
            switch (key) {
            case KeyboardKey::LeftControl:
            case KeyboardKey::RightControl: modifiers |= KeyModifier::Control; break;
            case KeyboardKey::LeftAlt:
            case KeyboardKey::RightAlt: modifiers |= KeyModifier::Alt; break;
            case KeyboardKey::LeftShift:
            case KeyboardKey::RightShift: modifiers |= KeyModifier::Shift; break;
            case KeyboardKey::LeftGui:
            case KeyboardKey::RightGui: modifiers |= KeyModifier::Super; break;
            default: break;
            }
            InvokeCallback(InputEvent{KeyCombo{modifiers, KeyboardKey::None}});
        }
        break;

    default:
        if (pressed) {
            InvokeCallback(InputEvent{KeyCombo{modifiers, key}});
        }
        break;
    }
}

void InputCapturer::ProcessPrimitive(MouseButton button, KeyModifier modifiers, bool pressed) {
    if (pressed) {
        InvokeCallback(InputEvent{MouseCombo{modifiers, button}});
    }
}

void InputCapturer::ProcessPrimitive(uint32 id, GamepadButton button, bool pressed) {
    if (pressed) {
        InvokeCallback(InputEvent{id, button});
    }
}

void InputCapturer::Capture(Callback &&callback) {
    m_callback.swap(callback);
}

void InputCapturer::CancelCapture() {
    m_callback = {};
}

bool InputCapturer::IsCapturing() const {
    return (bool)m_callback;
}

void InputCapturer::InvokeCallback(InputEvent &&event) {
    if (m_callback) {
        m_callback(event);
        m_callback = {};
    }
}

} // namespace app::input
