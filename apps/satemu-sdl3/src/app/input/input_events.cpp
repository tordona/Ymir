#include "input_events.hpp"

#include <fmt/format.h>

#include <sstream>

namespace app::input {

std::string ToString(const InputEvent &bind) {
    switch (bind.type) {
    default: // fallthrough
    case InputEvent::Type::None: return "None";
    case InputEvent::Type::KeyCombo: return ToString(bind.keyCombo);
    case InputEvent::Type::MouseCombo: return ToString(bind.mouseCombo);
    case InputEvent::Type::GamepadButton: return fmt::format("{}@{}", ToString(bind.gamepad.button), bind.gamepad.id);
    }
}

// ---------------------------------------------------------------------------------------------------------------------

bool TryParse(std::string_view str, InputEvent &event) {
    // Check for None
    if (str == "None") {
        event = InputEvent();
        return true;
    }

    // Check for KeyCombo
    if (TryParse(str, event.keyCombo)) {
        event.type = InputEvent::Type::KeyCombo;
        return true;
    }

    // Check for MouseCombo
    if (TryParse(str, event.mouseCombo)) {
        event.type = InputEvent::Type::MouseCombo;
        return true;
    }

    // Check for GamepadButton@<id>
    if (auto atPos = str.find_first_of('@'); atPos != std::string::npos) {
        auto btnStr = str.substr(0, atPos);
        auto idStr = str.substr(atPos + 1);
        if (TryParse(btnStr, event.gamepad.button)) {
            std::istringstream in{idStr.data()};
            uint32 id{};
            in >> id;
            if (in.eof()) {
                event.type = InputEvent::Type::GamepadButton;
                event.gamepad.id = id;
                return true;
            }
        }
    }

    // Nothing matched
    return false;
}

} // namespace app::input
