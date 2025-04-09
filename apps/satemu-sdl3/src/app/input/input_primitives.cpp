#include "input_primitives.hpp"

#include <fmt/format.h>

#include <unordered_map>

namespace app::input {

std::string ToString(KeyModifier mod) {
    fmt::memory_buffer buf{};
    auto inserter = std::back_inserter(buf);

    auto appendPlus = [&, added = false]() mutable {
        if (!added) {
            added = true;
        } else {
            fmt::format_to(inserter, "+");
        }
    };
    auto bmMod = BitmaskEnum(mod);
    if (bmMod.AnyOf(KeyModifier::Control)) {
        appendPlus();
        fmt::format_to(inserter, "Control");
    }
    if (bmMod.AnyOf(KeyModifier::Alt)) {
        appendPlus();
        fmt::format_to(inserter, "Alt");
    }
    if (bmMod.AnyOf(KeyModifier::Shift)) {
        appendPlus();
        fmt::format_to(inserter, "Shift");
    }
    if (bmMod.AnyOf(KeyModifier::Super)) {
        appendPlus();
        fmt::format_to(inserter, "Super");
    }
    return fmt::to_string(buf);
}

std::string_view ToString(KeyboardKey key) {
    switch (key) {
    case KeyboardKey::A: return "A";
    case KeyboardKey::B: return "B";
    case KeyboardKey::C: return "C";
    case KeyboardKey::D: return "D";
    case KeyboardKey::E: return "E";
    case KeyboardKey::F: return "F";
    case KeyboardKey::G: return "G";
    case KeyboardKey::H: return "H";
    case KeyboardKey::I: return "I";
    case KeyboardKey::J: return "J";
    case KeyboardKey::K: return "K";
    case KeyboardKey::L: return "L";
    case KeyboardKey::M: return "M";
    case KeyboardKey::N: return "N";
    case KeyboardKey::O: return "O";
    case KeyboardKey::P: return "P";
    case KeyboardKey::Q: return "Q";
    case KeyboardKey::R: return "R";
    case KeyboardKey::S: return "S";
    case KeyboardKey::T: return "T";
    case KeyboardKey::U: return "U";
    case KeyboardKey::V: return "V";
    case KeyboardKey::W: return "W";
    case KeyboardKey::X: return "X";
    case KeyboardKey::Y: return "Y";
    case KeyboardKey::Z: return "Z";

    case KeyboardKey::Alpha1: return "Alpha1";
    case KeyboardKey::Alpha2: return "Alpha2";
    case KeyboardKey::Alpha3: return "Alpha3";
    case KeyboardKey::Alpha4: return "Alpha4";
    case KeyboardKey::Alpha5: return "Alpha5";
    case KeyboardKey::Alpha6: return "Alpha6";
    case KeyboardKey::Alpha7: return "Alpha7";
    case KeyboardKey::Alpha8: return "Alpha8";
    case KeyboardKey::Alpha9: return "Alpha9";
    case KeyboardKey::Alpha0: return "Alpha0";

    case KeyboardKey::Return: return "Return";
    case KeyboardKey::Escape: return "Escape";
    case KeyboardKey::Backspace: return "Backspace";
    case KeyboardKey::Tab: return "Tab";
    case KeyboardKey::Spacebar: return "Spacebar";
    case KeyboardKey::MinusUnderscore: return "MinusUnderscore";
    case KeyboardKey::EqualsPlus: return "EqualsPlus";
    case KeyboardKey::LeftBracket: return "LeftBracket";
    case KeyboardKey::RightBracket: return "RightBracket";
    case KeyboardKey::Backslash: return "Backslash";
    case KeyboardKey::PoundTilde: return "PoundTilde";
    case KeyboardKey::Semicolon: return "Semicolon";
    case KeyboardKey::Apostrophe: return "Apostrophe";
    case KeyboardKey::GraveAccent: return "GraveAccent";
    case KeyboardKey::Comma: return "Comma";
    case KeyboardKey::Period: return "Period";
    case KeyboardKey::Slash: return "Slash";
    case KeyboardKey::CapsLock: return "CapsLock";

    case KeyboardKey::F1: return "F1";
    case KeyboardKey::F2: return "F2";
    case KeyboardKey::F3: return "F3";
    case KeyboardKey::F4: return "F4";
    case KeyboardKey::F5: return "F5";
    case KeyboardKey::F6: return "F6";
    case KeyboardKey::F7: return "F7";
    case KeyboardKey::F8: return "F8";
    case KeyboardKey::F9: return "F9";
    case KeyboardKey::F10: return "F10";
    case KeyboardKey::F11: return "F11";
    case KeyboardKey::F12: return "F12";

    case KeyboardKey::PrintScreen: return "PrintScreen";
    case KeyboardKey::ScrollLock: return "ScrollLock";
    case KeyboardKey::Pause: return "Pause";

    case KeyboardKey::Insert: return "Insert";
    case KeyboardKey::Home: return "Home";
    case KeyboardKey::PageUp: return "PageUp";
    case KeyboardKey::Delete: return "Delete";
    case KeyboardKey::End: return "End";
    case KeyboardKey::PageDown: return "PageDown";

    case KeyboardKey::Right: return "Right";
    case KeyboardKey::Left: return "Left";
    case KeyboardKey::Down: return "Down";
    case KeyboardKey::Up: return "Up";

    case KeyboardKey::NumLock: return "NumLock";
    case KeyboardKey::KeyPadDivide: return "KeyPadDivide";
    case KeyboardKey::KeyPadMultiply: return "KeyPadMultiply";
    case KeyboardKey::KeyPadSubtract: return "KeyPadSubtract";
    case KeyboardKey::KeyPadAdd: return "KeyPadAdd";
    case KeyboardKey::KeyPadEnter: return "KeyPadEnter";
    case KeyboardKey::KeyPad1: return "KeyPad1";
    case KeyboardKey::KeyPad2: return "KeyPad2";
    case KeyboardKey::KeyPad3: return "KeyPad3";
    case KeyboardKey::KeyPad4: return "KeyPad4";
    case KeyboardKey::KeyPad5: return "KeyPad5";
    case KeyboardKey::KeyPad6: return "KeyPad6";
    case KeyboardKey::KeyPad7: return "KeyPad7";
    case KeyboardKey::KeyPad8: return "KeyPad8";
    case KeyboardKey::KeyPad9: return "KeyPad9";
    case KeyboardKey::KeyPad0: return "KeyPad0";
    case KeyboardKey::KeyPadPeriod: return "KeyPadPeriod";

    case KeyboardKey::NonUSBackslash: return "NonUSBackslash";

    case KeyboardKey::Application: return "Application";
    case KeyboardKey::Power: return "Power";

    case KeyboardKey::KeyPadEquals: return "KeyPadEquals";

    case KeyboardKey::F13: return "F13";
    case KeyboardKey::F14: return "F14";
    case KeyboardKey::F15: return "F15";
    case KeyboardKey::F16: return "F16";
    case KeyboardKey::F17: return "F17";
    case KeyboardKey::F18: return "F18";
    case KeyboardKey::F19: return "F19";
    case KeyboardKey::F20: return "F20";
    case KeyboardKey::F21: return "F21";
    case KeyboardKey::F22: return "F22";
    case KeyboardKey::F23: return "F23";
    case KeyboardKey::F24: return "F24";

    case KeyboardKey::Execute: return "Execute";
    case KeyboardKey::Help: return "Help";
    case KeyboardKey::Menu: return "Menu";
    case KeyboardKey::Select: return "Select";
    case KeyboardKey::Stop: return "Stop";
    case KeyboardKey::Again: return "Again";
    case KeyboardKey::Undo: return "Undo";
    case KeyboardKey::Cut: return "Cut";
    case KeyboardKey::Copy: return "Copy";
    case KeyboardKey::Paste: return "Paste";
    case KeyboardKey::Find: return "Find";
    case KeyboardKey::Mute: return "Mute";
    case KeyboardKey::VolumeUp: return "VolumeUp";
    case KeyboardKey::VolumeDown: return "VolumeDown";

    case KeyboardKey::LockingCapsLock: return "LockingCapsLock";
    case KeyboardKey::LockingNumLock: return "LockingNumLock";
    case KeyboardKey::LockingScrollLock: return "LockingScrollLock";

    case KeyboardKey::KeyPadComma: return "KeyPadComma";
    case KeyboardKey::KeyPadEqualSign: return "KeyPadEqualSign";

    case KeyboardKey::Intl1: return "Intl1";
    case KeyboardKey::Intl2: return "Intl2";
    case KeyboardKey::Intl3: return "Intl3";
    case KeyboardKey::Intl4: return "Intl4";
    case KeyboardKey::Intl5: return "Intl5";
    case KeyboardKey::Intl6: return "Intl6";
    case KeyboardKey::Intl7: return "Intl7";
    case KeyboardKey::Intl8: return "Intl8";
    case KeyboardKey::Intl9: return "Intl9";

    case KeyboardKey::LANG1: return "LANG1";
    case KeyboardKey::LANG2: return "LANG2";
    case KeyboardKey::LANG3: return "LANG3";
    case KeyboardKey::LANG4: return "LANG4";
    case KeyboardKey::LANG5: return "LANG5";
    case KeyboardKey::LANG6: return "LANG6";
    case KeyboardKey::LANG7: return "LANG7";
    case KeyboardKey::LANG8: return "LANG8";
    case KeyboardKey::LANG9: return "LANG9";

    case KeyboardKey::AltErase: return "AltErase";
    case KeyboardKey::SysReq: return "SysReq";
    case KeyboardKey::Cancel: return "Cancel";
    case KeyboardKey::Clear: return "Clear";
    case KeyboardKey::Prior: return "Prior";
    case KeyboardKey::Return2: return "Return2";
    case KeyboardKey::Separator: return "Separator";
    case KeyboardKey::Out: return "Out";
    case KeyboardKey::Oper: return "Oper";
    case KeyboardKey::ClearAgain: return "ClearAgain";
    case KeyboardKey::CrSelProps: return "CrSelProps";
    case KeyboardKey::ExSel: return "ExSel";

    case KeyboardKey::KeyPad00: return "KeyPad00";
    case KeyboardKey::KeyPad000: return "KeyPad000";
    case KeyboardKey::ThousandsSeparator: return "ThousandsSeparator";
    case KeyboardKey::DecimalSeparator: return "DecimalSeparator";
    case KeyboardKey::CurrencyUnit: return "CurrencyUnit";
    case KeyboardKey::CurrencySubUnit: return "CurrencySubUnit";
    case KeyboardKey::KeyPadLeftParenthesis: return "KeyPadLeftParenthesis";
    case KeyboardKey::KeyPadRightParenthesis: return "KeyPadRightParenthesis";
    case KeyboardKey::KeyPadLeftBrace: return "KeyPadLeftBrace";
    case KeyboardKey::KeyPadRightBrace: return "KeyPadRightBrace";
    case KeyboardKey::KeyPadTab: return "KeyPadTab";
    case KeyboardKey::KeyPadBackspace: return "KeyPadBackspace";
    case KeyboardKey::KeyPadA: return "KeyPadA";
    case KeyboardKey::KeyPadB: return "KeyPadB";
    case KeyboardKey::KeyPadC: return "KeyPadC";
    case KeyboardKey::KeyPadD: return "KeyPadD";
    case KeyboardKey::KeyPadE: return "KeyPadE";
    case KeyboardKey::KeyPadF: return "KeyPadF";
    case KeyboardKey::KeyPadXOR: return "KeyPadXOR";
    case KeyboardKey::KeyPadPower: return "KeyPadPower";
    case KeyboardKey::KeyPadPercent: return "KeyPadPercent";
    case KeyboardKey::KeyPadLess: return "KeyPadLess";
    case KeyboardKey::KeyPadGreater: return "KeyPadGreater";
    case KeyboardKey::KeyPadAmpersand: return "KeyPadAmpersand";
    case KeyboardKey::KeyPadDoubleAmpersand: return "KeyPadDoubleAmpersand";
    case KeyboardKey::KeyPadVerticalBar: return "KeyPadVerticalBar";
    case KeyboardKey::KeyPadDoubleVerticalBar: return "KeyPadDoubleVerticalBar";
    case KeyboardKey::KeyPadColon: return "KeyPadColon";
    case KeyboardKey::KeyPadHash: return "KeyPadHash";
    case KeyboardKey::KeyPadSpace: return "KeyPadSpace";
    case KeyboardKey::KeyPadAt: return "KeyPadAt";
    case KeyboardKey::KeyPadExclamation: return "KeyPadExclamation";
    case KeyboardKey::KeyPadMemoryStore: return "KeyPadMemoryStore";
    case KeyboardKey::KeyPadMemoryRecall: return "KeyPadMemoryRecall";
    case KeyboardKey::KeyPadMemoryClear: return "KeyPadMemoryClear";
    case KeyboardKey::KeyPadMemoryAdd: return "KeyPadMemoryAdd";
    case KeyboardKey::KeyPadMemorySubtract: return "KeyPadMemorySubtract";
    case KeyboardKey::KeyPadMemoryMultiply: return "KeyPadMemoryMultiply";
    case KeyboardKey::KeyPadMemoryDivide: return "KeyPadMemoryDivide";
    case KeyboardKey::KeyPadPlusMinus: return "KeyPadPlusMinus";
    case KeyboardKey::KeyPadClear: return "KeyPadClear";
    case KeyboardKey::KeyPadClearEntry: return "KeyPadClearEntry";
    case KeyboardKey::KeyPadBinary: return "KeyPadBinary";
    case KeyboardKey::KeyPadOctal: return "KeyPadOctal";
    case KeyboardKey::KeyPadDecimal: return "KeyPadDecimal";
    case KeyboardKey::KeyPadHexadecimal: return "KeyPadHexadecimal";

    case KeyboardKey::LeftControl: return "LeftControl";
    case KeyboardKey::LeftShift: return "LeftShift";
    case KeyboardKey::LeftAlt: return "LeftAlt";
    case KeyboardKey::LeftGui: return "LeftGui";
    case KeyboardKey::RightControl: return "RightControl";
    case KeyboardKey::RightShift: return "RightShift";
    case KeyboardKey::RightAlt: return "RightAlt";
    case KeyboardKey::RightGui: return "RightGui";
    default: return "Unknown";
    }
}

std::string_view ToString(MouseButton btn) {
    switch (btn) {
    case MouseButton::Left: return "MouseLeft";
    case MouseButton::Right: return "MouseRight";
    case MouseButton::Middle: return "MouseMiddle";
    case MouseButton::Extra1: return "MouseExtra1";
    case MouseButton::Extra2: return "MouseExtra2";

    default: return "Unknown";
    }
}

std::string_view ToString(MouseAxis1D axis) {
    switch (axis) {
    case MouseAxis1D::Vertical: return "MouseV";
    case MouseAxis1D::Horizontal: return "MouseH";
    case MouseAxis1D::WheelVertical: return "MouseWheelV";
    case MouseAxis1D::WheelHorizontal: return "MouseWheelH";
    default: return "Unknown";
    }
}

std::string_view ToString(MouseAxis2D axis) {
    switch (axis) {
    case MouseAxis2D::Mouse: return "Mouse";
    case MouseAxis2D::Wheel: return "MouseWheel";
    default: return "Unknown";
    }
}

std::string_view ToString(GamepadButton btn) {
    switch (btn) {
    case GamepadButton::A: return "GamepadA";
    case GamepadButton::B: return "GamepadB";
    case GamepadButton::X: return "GamepadX";
    case GamepadButton::Y: return "GamepadY";
    case GamepadButton::LeftBumper: return "GamepadLeftBumper";
    case GamepadButton::RightBumper: return "GamepadRightBumper";
    case GamepadButton::Back: return "GamepadBack";
    case GamepadButton::Start: return "GamepadStart";
    case GamepadButton::Guide: return "GamepadGuide";
    case GamepadButton::LeftThumb: return "GamepadLeftThumb";
    case GamepadButton::RightThumb: return "GamepadRightThumb";
    case GamepadButton::DpadUp: return "GamepadDpadUp";
    case GamepadButton::DpadDown: return "GamepadDpadDown";
    case GamepadButton::DpadLeft: return "GamepadDpadLeft";
    case GamepadButton::DpadRight: return "GamepadDpadRight";
    case GamepadButton::LeftPaddle1: return "GamepadLeftPaddle1";
    case GamepadButton::LeftPaddle2: return "GamepadLeftPaddle2";
    case GamepadButton::RightPaddle1: return "GamepadRightPaddle1";
    case GamepadButton::RightPaddle2: return "GamepadRightPaddle2";
    case GamepadButton::TouchPad: return "GamepadTouchPad";
    case GamepadButton::Misc1: return "GamepadMisc1";
    case GamepadButton::Misc2: return "GamepadMisc2";
    case GamepadButton::Misc3: return "GamepadMisc3";
    case GamepadButton::Misc4: return "GamepadMisc4";
    case GamepadButton::Misc5: return "GamepadMisc5";
    case GamepadButton::Misc6: return "GamepadMisc6";
    default: return "Unknown";
    }
}

std::string_view ToString(GamepadAxis1D axis) {
    switch (axis) {
    case GamepadAxis1D::LeftStickX: return "GamepadLeftStickX";
    case GamepadAxis1D::LeftStickY: return "GamepadLeftStickY";
    case GamepadAxis1D::RightStickX: return "GamepadRightStickX";
    case GamepadAxis1D::RightStickY: return "GamepadRightStickY";
    case GamepadAxis1D::LeftTrigger: return "GamepadLeftTrigger";
    case GamepadAxis1D::RightTrigger: return "GamepadRightTrigger";
    default: return "Unknown";
    }
}

std::string_view ToString(GamepadAxis2D axis) {
    switch (axis) {
    case GamepadAxis2D::LeftStick: return "GamepadLeftStick";
    case GamepadAxis2D::RightStick: return "GamepadRightStick";
    default: return "Unknown";
    }
}

std::string ToString(const KeyCombo &combo) {
    auto keyStr = ::app::input::ToString(combo.key);
    if (combo.modifiers != KeyModifier::None) {
        auto modStr = ::app::input::ToString(combo.modifiers);
        return fmt::format("{}+{}", modStr, keyStr);
    } else {
        return std::string(keyStr);
    }
}

std::string ToString(const MouseCombo &combo) {
    auto buttonStr = ::app::input::ToString(combo.button);
    if (combo.modifiers != KeyModifier::None) {
        auto modStr = ::app::input::ToString(combo.modifiers);
        return fmt::format("{}+{}", modStr, buttonStr);
    } else {
        return std::string(buttonStr);
    }
}

// ---------------------------------------------------------------------------------------------------------------------

static const std::unordered_map<std::string_view, KeyModifier> kKeyModifiers{
    {"Control", KeyModifier::Control},
    {"Alt", KeyModifier::Alt},
    {"Shift", KeyModifier::Shift},
    {"Super", KeyModifier::Super},
};

static const std::unordered_map<std::string_view, KeyboardKey> kKeyboardKeys{
    {"A", KeyboardKey::A},
    {"B", KeyboardKey::B},
    {"C", KeyboardKey::C},
    {"D", KeyboardKey::D},
    {"E", KeyboardKey::E},
    {"F", KeyboardKey::F},
    {"G", KeyboardKey::G},
    {"H", KeyboardKey::H},
    {"I", KeyboardKey::I},
    {"J", KeyboardKey::J},
    {"K", KeyboardKey::K},
    {"L", KeyboardKey::L},
    {"M", KeyboardKey::M},
    {"N", KeyboardKey::N},
    {"O", KeyboardKey::O},
    {"P", KeyboardKey::P},
    {"Q", KeyboardKey::Q},
    {"R", KeyboardKey::R},
    {"S", KeyboardKey::S},
    {"T", KeyboardKey::T},
    {"U", KeyboardKey::U},
    {"V", KeyboardKey::V},
    {"W", KeyboardKey::W},
    {"X", KeyboardKey::X},
    {"Y", KeyboardKey::Y},
    {"Z", KeyboardKey::Z},

    {"Alpha1", KeyboardKey::Alpha1},
    {"Alpha2", KeyboardKey::Alpha2},
    {"Alpha3", KeyboardKey::Alpha3},
    {"Alpha4", KeyboardKey::Alpha4},
    {"Alpha5", KeyboardKey::Alpha5},
    {"Alpha6", KeyboardKey::Alpha6},
    {"Alpha7", KeyboardKey::Alpha7},
    {"Alpha8", KeyboardKey::Alpha8},
    {"Alpha9", KeyboardKey::Alpha9},
    {"Alpha0", KeyboardKey::Alpha0},

    {"Return", KeyboardKey::Return},
    {"Escape", KeyboardKey::Escape},
    {"Backspace", KeyboardKey::Backspace},
    {"Tab", KeyboardKey::Tab},
    {"Spacebar", KeyboardKey::Spacebar},
    {"MinusUnderscore", KeyboardKey::MinusUnderscore},
    {"EqualsPlus", KeyboardKey::EqualsPlus},
    {"LeftBracket", KeyboardKey::LeftBracket},
    {"RightBracket", KeyboardKey::RightBracket},
    {"Backslash", KeyboardKey::Backslash},
    {"PoundTilde", KeyboardKey::PoundTilde},
    {"Semicolon", KeyboardKey::Semicolon},
    {"Apostrophe", KeyboardKey::Apostrophe},
    {"GraveAccent", KeyboardKey::GraveAccent},
    {"Comma", KeyboardKey::Comma},
    {"Period", KeyboardKey::Period},
    {"Slash", KeyboardKey::Slash},
    {"CapsLock", KeyboardKey::CapsLock},

    {"F1", KeyboardKey::F1},
    {"F2", KeyboardKey::F2},
    {"F3", KeyboardKey::F3},
    {"F4", KeyboardKey::F4},
    {"F5", KeyboardKey::F5},
    {"F6", KeyboardKey::F6},
    {"F7", KeyboardKey::F7},
    {"F8", KeyboardKey::F8},
    {"F9", KeyboardKey::F9},
    {"F10", KeyboardKey::F10},
    {"F11", KeyboardKey::F11},
    {"F12", KeyboardKey::F12},

    {"PrintScreen", KeyboardKey::PrintScreen},
    {"ScrollLock", KeyboardKey::ScrollLock},
    {"Pause", KeyboardKey::Pause},

    {"Insert", KeyboardKey::Insert},
    {"Home", KeyboardKey::Home},
    {"PageUp", KeyboardKey::PageUp},
    {"Delete", KeyboardKey::Delete},
    {"End", KeyboardKey::End},
    {"PageDown", KeyboardKey::PageDown},

    {"Right", KeyboardKey::Right},
    {"Left", KeyboardKey::Left},
    {"Down", KeyboardKey::Down},
    {"Up", KeyboardKey::Up},

    {"NumLock", KeyboardKey::NumLock},
    {"KeyPadDivide", KeyboardKey::KeyPadDivide},
    {"KeyPadMultiply", KeyboardKey::KeyPadMultiply},
    {"KeyPadSubtract", KeyboardKey::KeyPadSubtract},
    {"KeyPadAdd", KeyboardKey::KeyPadAdd},
    {"KeyPadEnter", KeyboardKey::KeyPadEnter},
    {"KeyPad1", KeyboardKey::KeyPad1},
    {"KeyPad2", KeyboardKey::KeyPad2},
    {"KeyPad3", KeyboardKey::KeyPad3},
    {"KeyPad4", KeyboardKey::KeyPad4},
    {"KeyPad5", KeyboardKey::KeyPad5},
    {"KeyPad6", KeyboardKey::KeyPad6},
    {"KeyPad7", KeyboardKey::KeyPad7},
    {"KeyPad8", KeyboardKey::KeyPad8},
    {"KeyPad9", KeyboardKey::KeyPad9},
    {"KeyPad0", KeyboardKey::KeyPad0},
    {"KeyPadPeriod", KeyboardKey::KeyPadPeriod},

    {"NonUSBackslash", KeyboardKey::NonUSBackslash},

    {"Application", KeyboardKey::Application},
    {"Power", KeyboardKey::Power},

    {"KeyPadEquals", KeyboardKey::KeyPadEquals},

    {"F13", KeyboardKey::F13},
    {"F14", KeyboardKey::F14},
    {"F15", KeyboardKey::F15},
    {"F16", KeyboardKey::F16},
    {"F17", KeyboardKey::F17},
    {"F18", KeyboardKey::F18},
    {"F19", KeyboardKey::F19},
    {"F20", KeyboardKey::F20},
    {"F21", KeyboardKey::F21},
    {"F22", KeyboardKey::F22},
    {"F23", KeyboardKey::F23},
    {"F24", KeyboardKey::F24},

    {"Execute", KeyboardKey::Execute},
    {"Help", KeyboardKey::Help},
    {"Menu", KeyboardKey::Menu},
    {"Select", KeyboardKey::Select},
    {"Stop", KeyboardKey::Stop},
    {"Again", KeyboardKey::Again},
    {"Undo", KeyboardKey::Undo},
    {"Cut", KeyboardKey::Cut},
    {"Copy", KeyboardKey::Copy},
    {"Paste", KeyboardKey::Paste},
    {"Find", KeyboardKey::Find},
    {"Mute", KeyboardKey::Mute},
    {"VolumeUp", KeyboardKey::VolumeUp},
    {"VolumeDown", KeyboardKey::VolumeDown},

    {"LockingCapsLock", KeyboardKey::LockingCapsLock},
    {"LockingNumLock", KeyboardKey::LockingNumLock},
    {"LockingScrollLock", KeyboardKey::LockingScrollLock},

    {"KeyPadComma", KeyboardKey::KeyPadComma},
    {"KeyPadEqualSign", KeyboardKey::KeyPadEqualSign},

    {"Intl1", KeyboardKey::Intl1},
    {"Intl2", KeyboardKey::Intl2},
    {"Intl3", KeyboardKey::Intl3},
    {"Intl4", KeyboardKey::Intl4},
    {"Intl5", KeyboardKey::Intl5},
    {"Intl6", KeyboardKey::Intl6},
    {"Intl7", KeyboardKey::Intl7},
    {"Intl8", KeyboardKey::Intl8},
    {"Intl9", KeyboardKey::Intl9},

    {"LANG1", KeyboardKey::LANG1},
    {"LANG2", KeyboardKey::LANG2},
    {"LANG3", KeyboardKey::LANG3},
    {"LANG4", KeyboardKey::LANG4},
    {"LANG5", KeyboardKey::LANG5},
    {"LANG6", KeyboardKey::LANG6},
    {"LANG7", KeyboardKey::LANG7},
    {"LANG8", KeyboardKey::LANG8},
    {"LANG9", KeyboardKey::LANG9},

    {"AltErase", KeyboardKey::AltErase},
    {"SysReq", KeyboardKey::SysReq},
    {"Cancel", KeyboardKey::Cancel},
    {"Clear", KeyboardKey::Clear},
    {"Prior", KeyboardKey::Prior},
    {"Return2", KeyboardKey::Return2},
    {"Separator", KeyboardKey::Separator},
    {"Out", KeyboardKey::Out},
    {"Oper", KeyboardKey::Oper},
    {"ClearAgain", KeyboardKey::ClearAgain},
    {"CrSelProps", KeyboardKey::CrSelProps},
    {"ExSel", KeyboardKey::ExSel},

    {"KeyPad00", KeyboardKey::KeyPad00},
    {"KeyPad000", KeyboardKey::KeyPad000},
    {"ThousandsSeparator", KeyboardKey::ThousandsSeparator},
    {"DecimalSeparator", KeyboardKey::DecimalSeparator},
    {"CurrencyUnit", KeyboardKey::CurrencyUnit},
    {"CurrencySubUnit", KeyboardKey::CurrencySubUnit},
    {"KeyPadLeftParenthesis", KeyboardKey::KeyPadLeftParenthesis},
    {"KeyPadRightParenthesis", KeyboardKey::KeyPadRightParenthesis},
    {"KeyPadLeftBrace", KeyboardKey::KeyPadLeftBrace},
    {"KeyPadRightBrace", KeyboardKey::KeyPadRightBrace},
    {"KeyPadTab", KeyboardKey::KeyPadTab},
    {"KeyPadBackspace", KeyboardKey::KeyPadBackspace},
    {"KeyPadA", KeyboardKey::KeyPadA},
    {"KeyPadB", KeyboardKey::KeyPadB},
    {"KeyPadC", KeyboardKey::KeyPadC},
    {"KeyPadD", KeyboardKey::KeyPadD},
    {"KeyPadE", KeyboardKey::KeyPadE},
    {"KeyPadF", KeyboardKey::KeyPadF},
    {"KeyPadXOR", KeyboardKey::KeyPadXOR},
    {"KeyPadPower", KeyboardKey::KeyPadPower},
    {"KeyPadPercent", KeyboardKey::KeyPadPercent},
    {"KeyPadLess", KeyboardKey::KeyPadLess},
    {"KeyPadGreater", KeyboardKey::KeyPadGreater},
    {"KeyPadAmpersand", KeyboardKey::KeyPadAmpersand},
    {"KeyPadDoubleAmpersand", KeyboardKey::KeyPadDoubleAmpersand},
    {"KeyPadVerticalBar", KeyboardKey::KeyPadVerticalBar},
    {"KeyPadDoubleVerticalBar", KeyboardKey::KeyPadDoubleVerticalBar},
    {"KeyPadColon", KeyboardKey::KeyPadColon},
    {"KeyPadHash", KeyboardKey::KeyPadHash},
    {"KeyPadSpace", KeyboardKey::KeyPadSpace},
    {"KeyPadAt", KeyboardKey::KeyPadAt},
    {"KeyPadExclamation", KeyboardKey::KeyPadExclamation},
    {"KeyPadMemoryStore", KeyboardKey::KeyPadMemoryStore},
    {"KeyPadMemoryRecall", KeyboardKey::KeyPadMemoryRecall},
    {"KeyPadMemoryClear", KeyboardKey::KeyPadMemoryClear},
    {"KeyPadMemoryAdd", KeyboardKey::KeyPadMemoryAdd},
    {"KeyPadMemorySubtract", KeyboardKey::KeyPadMemorySubtract},
    {"KeyPadMemoryMultiply", KeyboardKey::KeyPadMemoryMultiply},
    {"KeyPadMemoryDivide", KeyboardKey::KeyPadMemoryDivide},
    {"KeyPadPlusMinus", KeyboardKey::KeyPadPlusMinus},
    {"KeyPadClear", KeyboardKey::KeyPadClear},
    {"KeyPadClearEntry", KeyboardKey::KeyPadClearEntry},
    {"KeyPadBinary", KeyboardKey::KeyPadBinary},
    {"KeyPadOctal", KeyboardKey::KeyPadOctal},
    {"KeyPadDecimal", KeyboardKey::KeyPadDecimal},
    {"KeyPadHexadecimal", KeyboardKey::KeyPadHexadecimal},

    {"LeftControl", KeyboardKey::LeftControl},
    {"LeftShift", KeyboardKey::LeftShift},
    {"LeftAlt", KeyboardKey::LeftAlt},
    {"LeftGui", KeyboardKey::LeftGui},
    {"RightControl", KeyboardKey::RightControl},
    {"RightShift", KeyboardKey::RightShift},
    {"RightAlt", KeyboardKey::RightAlt},
    {"RightGui", KeyboardKey::RightGui},
};

static const std::unordered_map<std::string_view, MouseButton> kMouseButtons{
    {"MouseLeft", MouseButton::Left},     {"MouseRight", MouseButton::Right},   {"MouseMiddle", MouseButton::Middle},
    {"MouseExtra1", MouseButton::Extra1}, {"MouseExtra2", MouseButton::Extra2},
};

static const std::unordered_map<std::string_view, MouseAxis1D> kMouseAxes1D{
    {"MouseV", MouseAxis1D::Vertical},
    {"MouseH", MouseAxis1D::Horizontal},
    {"MouseWheelV", MouseAxis1D::WheelVertical},
    {"MouseWheelH", MouseAxis1D::WheelHorizontal},
};

static const std::unordered_map<std::string_view, MouseAxis2D> kMouseAxes2D{
    {"Mouse", MouseAxis2D::Mouse},
    {"MouseWheel", MouseAxis2D::Wheel},
};

static const std::unordered_map<std::string_view, GamepadButton> kGamepadButtons{
    {"GamepadA", GamepadButton::A},
    {"GamepadB", GamepadButton::B},
    {"GamepadX", GamepadButton::X},
    {"GamepadY", GamepadButton::Y},
    {"GamepadLeftBumper", GamepadButton::LeftBumper},
    {"GamepadRightBumper", GamepadButton::RightBumper},
    {"GamepadBack", GamepadButton::Back},
    {"GamepadStart", GamepadButton::Start},
    {"GamepadGuide", GamepadButton::Guide},
    {"GamepadLeftThumb", GamepadButton::LeftThumb},
    {"GamepadRightThumb", GamepadButton::RightThumb},
    {"GamepadDpadUp", GamepadButton::DpadUp},
    {"GamepadDpadDown", GamepadButton::DpadDown},
    {"GamepadDpadLeft", GamepadButton::DpadLeft},
    {"GamepadDpadRight", GamepadButton::DpadRight},
    {"GamepadLeftPaddle1", GamepadButton::LeftPaddle1},
    {"GamepadLeftPaddle2", GamepadButton::LeftPaddle2},
    {"GamepadRightPaddle1", GamepadButton::RightPaddle1},
    {"GamepadRightPaddle2", GamepadButton::RightPaddle2},
    {"GamepadTouchPad", GamepadButton::TouchPad},
    {"GamepadMisc1", GamepadButton::Misc1},
    {"GamepadMisc2", GamepadButton::Misc2},
    {"GamepadMisc3", GamepadButton::Misc3},
    {"GamepadMisc4", GamepadButton::Misc4},
    {"GamepadMisc5", GamepadButton::Misc5},
    {"GamepadMisc6", GamepadButton::Misc6},
};

static const std::unordered_map<std::string_view, GamepadAxis1D> kGamepadAxes1D{
    {"GamepadLeftStickX", GamepadAxis1D::LeftStickX},   {"GamepadLeftStickY", GamepadAxis1D::LeftStickY},
    {"GamepadRightStickX", GamepadAxis1D::RightStickX}, {"GamepadRightStickY", GamepadAxis1D::RightStickY},
    {"GamepadLeftTrigger", GamepadAxis1D::LeftTrigger}, {"GamepadRightTrigger", GamepadAxis1D::RightTrigger},
};

static const std::unordered_map<std::string_view, GamepadAxis2D> kGamepadAxes2D{
    {"GamepadLeftStick", GamepadAxis2D::LeftStick},
    {"GamepadRightStick", GamepadAxis2D::RightStick},
};

bool TryParse(std::string_view str, KeyModifier &mod) {
    KeyModifier out = KeyModifier::None;
    size_t offset = 0;
    size_t indexOfPlus;
    do {
        indexOfPlus = str.find_first_of('+', offset);
        auto modStr = str.substr(offset, indexOfPlus - offset);
        if (kKeyModifiers.contains(modStr)) {
            out |= kKeyModifiers.at(modStr);
        } else {
            return false;
        }
        offset = indexOfPlus + 1;
    } while (indexOfPlus != std::string::npos);
    mod = out;
    return true;
}

bool TryParse(std::string_view str, KeyboardKey &key) {
    if (kKeyboardKeys.contains(str)) {
        key = kKeyboardKeys.at(str);
        return true;
    }
    return false;
}

bool TryParse(std::string_view str, MouseButton &btn) {
    if (kMouseButtons.contains(str)) {
        btn = kMouseButtons.at(str);
        return true;
    }
    return false;
}

bool TryParse(std::string_view str, MouseAxis1D &axis) {
    if (kMouseAxes1D.contains(str)) {
        axis = kMouseAxes1D.at(str);
        return true;
    }
    return false;
}

bool TryParse(std::string_view str, MouseAxis2D &axis) {
    if (kMouseAxes2D.contains(str)) {
        axis = kMouseAxes2D.at(str);
        return true;
    }
    return false;
}

bool TryParse(std::string_view str, GamepadButton &btn) {
    if (kGamepadButtons.contains(str)) {
        btn = kGamepadButtons.at(str);
        return true;
    }
    return false;
}

bool TryParse(std::string_view str, GamepadAxis1D &axis) {
    if (kGamepadAxes1D.contains(str)) {
        axis = kGamepadAxes1D.at(str);
        return true;
    }
    return false;
}

bool TryParse(std::string_view str, GamepadAxis2D &axis) {
    if (kGamepadAxes2D.contains(str)) {
        axis = kGamepadAxes2D.at(str);
        return true;
    }
    return false;
}

bool TryParse(std::string_view str, KeyCombo &combo) {
    auto lastPlus = str.find_last_of('+');
    if (lastPlus == std::string::npos) {
        // No key modifiers; the entire string should contain a key
        if (!TryParse(str, combo.key)) {
            return false;
        }
        combo.modifiers = KeyModifier::None;
        return true;
    } else {
        // Modifiers are to the left of the last '+'
        // Key is to the right
        auto modStr = str.substr(0, lastPlus);
        auto keyStr = str.substr(lastPlus + 1);

        // Parse modifiers into a variable to avoid modifying the object before we know if the key is also valid
        KeyModifier mod{};
        if (!TryParse(modStr, mod)) {
            return false;
        }
        if (!TryParse(keyStr, combo.key)) {
            return false;
        }
        combo.modifiers = mod;
        return true;
    }
}

bool TryParse(std::string_view str, MouseCombo &combo) {
    auto lastPlus = str.find_last_of('+');
    if (lastPlus == std::string::npos) {
        // No key modifiers; the entire string should contain a button
        if (!TryParse(str, combo.button)) {
            return false;
        }
        combo.modifiers = KeyModifier::None;
        return true;
    } else {
        // Modifiers are to the left of the last '+'
        // Key is to the right
        auto modStr = str.substr(0, lastPlus);
        auto btnStr = str.substr(lastPlus + 1);

        // Parse modifiers into a variable to avoid modifying the object before we know if the key is also valid
        KeyModifier mod{};
        if (!TryParse(modStr, mod)) {
            return false;
        }
        if (!TryParse(btnStr, combo.button)) {
            return false;
        }
        combo.modifiers = mod;
        return true;
    }
}

} // namespace app::input
