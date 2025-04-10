#pragma once

#include <app/input/input_events.hpp>
#include <app/input/input_primitives.hpp>

#include <satemu/core/types.hpp>

#include <functional>

namespace app::input {

// Captures input primitives and sends the first generated event to a callback.
//
// Behavior for keyboard keys:
// - Escape cancels capture
// - Modifier keys (Ctrl, Alt, Shift, Super) trigger the callback when released
// - All other keys trigger when pressed
// - Modifier keys are included in the captured input event
//
// Behavior for mouse buttons:
// - All buttons trigger the callback when pressed
// - Keyboard modifier keys are included in the captured input event
//
// Behavior for gamepad buttons:
// - All buttons trigger the callback when pressed
// - Keyboard modifier keys are not captured
//
// Once captured, the callback is cleared.
class InputCapturer {
public:
    using Callback = std::function<void(const InputEvent &)>;

    // Processes a keyboard primitive.
    void ProcessPrimitive(KeyboardKey key, KeyModifier modifiers, bool pressed);
    // Processes a mouse button primitive.
    void ProcessPrimitive(MouseButton button, KeyModifier modifiers, bool pressed);
    // Processes a gamepad button primitive.
    void ProcessPrimitive(uint32 id, GamepadButton button, bool pressed);

    // Captures the next input event.
    void Capture(Callback &&callback);
    void CancelCapture();
    bool IsCapturing() const;

private:
    Callback m_callback;

    void InvokeCallback(InputEvent &&event);
};

} // namespace app::input
