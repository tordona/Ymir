#pragma once

#include <satemu/core/types.hpp>

#include <satemu/util/bitmask_enum.hpp>

#include <string>
#include <string_view>

namespace app::input {

// Action identifier. Apps can use any mapping scheme they wish for actions.
// 4 billion different actions should be more than enough for any kind of app.
//
// Action 0 is reserved to represent "no action" or "unmapped binding".
using ActionID = uint32;

// The "no action" or "unmapped binding" action identifier.
inline constexpr ActionID kNoAction = 0;

// Action context. Enables the same action to be handled in multiple different contexts (such as handling inputs
// for multiple players).
//
// The context is large enough to accomodate a pointer; feel free to cast it to void* should you need to refer to
// more complex or structured data.
using ActionContext = uintptr_t;

// ---------------------------------------------------------------------------------------------------------------------
// Input elements

// Keyboard key modifiers.
enum class KeyModifier {
    None = 0,

    Shift = 1 << 0,
    Control = 1 << 1,
    Alt = 1 << 2,
    Super = 1 << 3, // Windows/Command key
};

enum class KeyboardKey {
    None,

    // Codes are from USB Keyboard/Keypad Page (0x07)
    // See https://www.usb.org/sites/default/files/documents/hut1_12v2.pdf

    A = 4,  // a A
    B = 5,  // b B
    C = 6,  // c C
    D = 7,  // d D
    E = 8,  // e E
    F = 9,  // f F
    G = 10, // g G
    H = 11, // h H
    I = 12, // i I
    J = 13, // j J
    K = 14, // k K
    L = 15, // l L
    M = 16, // m M
    N = 17, // n N
    O = 18, // o O
    P = 19, // p P
    Q = 20, // q Q
    R = 21, // r R
    S = 22, // s S
    T = 23, // t T
    U = 24, // u U
    V = 25, // v V
    W = 26, // w W
    X = 27, // x X
    Y = 28, // y Y
    Z = 29, // z Z

    Alpha1 = 30, // 1 !
    Alpha2 = 31, // 2 @
    Alpha3 = 32, // 3 #
    Alpha4 = 33, // 4 $
    Alpha5 = 34, // 5 %
    Alpha6 = 35, // 6 ^
    Alpha7 = 36, // 7 &
    Alpha8 = 37, // 8 *
    Alpha9 = 38, // 9 (
    Alpha0 = 39, // 0 )

    Return = 40,
    Escape = 41,
    Backspace = 42,
    Tab = 43,
    Spacebar = 44,
    MinusUnderscore = 45, // - _
    EqualsPlus = 46,      // = +
    LeftBracket = 47,     // [ {
    RightBracket = 48,    // ] }
    Backslash = 49,       // \ |
    PoundTilde = 50,      // # ~
    Semicolon = 51,       // ; :
    Apostrophe = 52,      // ' "
    GraveAccent = 53,     // ` ~
    Comma = 54,           // , <
    Period = 55,          // . >
    Slash = 56,           // / ?
    CapsLock = 57,

    F1 = 58,
    F2 = 59,
    F3 = 60,
    F4 = 61,
    F5 = 62,
    F6 = 63,
    F7 = 64,
    F8 = 65,
    F9 = 66,
    F10 = 67,
    F11 = 68,
    F12 = 69,

    PrintScreen = 70,
    ScrollLock = 71,
    Pause = 72,

    Insert = 73,
    Home = 74,
    PageUp = 75,
    Delete = 76,
    End = 77,
    PageDown = 78,

    Right = 79,
    Left = 80,
    Down = 81,
    Up = 82,

    NumLock = 83,
    KeyPadDivide = 84,
    KeyPadMultiply = 85,
    KeyPadSubtract = 86,
    KeyPadAdd = 87,
    KeyPadEnter = 88,
    KeyPad1 = 89,
    KeyPad2 = 90,
    KeyPad3 = 91,
    KeyPad4 = 92,
    KeyPad5 = 93,
    KeyPad6 = 94,
    KeyPad7 = 95,
    KeyPad8 = 96,
    KeyPad9 = 97,
    KeyPad0 = 98,
    KeyPadPeriod = 99,

    NonUSBackslash = 100,

    Application = 101, // Context menu/Compose key
    Power = 102,

    KeyPadEquals = 103,

    F13 = 104,
    F14 = 105,
    F15 = 106,
    F16 = 107,
    F17 = 108,
    F18 = 109,
    F19 = 110,
    F20 = 111,
    F21 = 112,
    F22 = 113,
    F23 = 114,
    F24 = 115,

    Execute = 116,
    Help = 117,
    Menu = 118,
    Select = 119,
    Stop = 120,
    Again = 121,
    Undo = 122,
    Cut = 123,
    Copy = 124,
    Paste = 125,
    Find = 126,
    Mute = 127,
    VolumeUp = 128,
    VolumeDown = 129,

    LockingCapsLock = 130,
    LockingNumLock = 131,
    LockingScrollLock = 132,

    KeyPadComma = 133,
    KeyPadEqualSign = 134,

    Intl1 = 135,
    Intl2 = 136,
    Intl3 = 137,
    Intl4 = 138,
    Intl5 = 139,
    Intl6 = 140,
    Intl7 = 141,
    Intl8 = 142,
    Intl9 = 143,

    LANG1 = 144,
    LANG2 = 145,
    LANG3 = 146,
    LANG4 = 147,
    LANG5 = 148,
    LANG6 = 149,
    LANG7 = 150,
    LANG8 = 151,
    LANG9 = 152,

    AltErase = 153,
    SysReq = 154,
    Cancel = 155,
    Clear = 156,
    Prior = 157,
    Return2 = 158,
    Separator = 159,
    Out = 160,
    Oper = 161,
    ClearAgain = 162,
    CrSelProps = 163,
    ExSel = 164,

    KeyPad00 = 176,
    KeyPad000 = 177,
    ThousandsSeparator = 178,
    DecimalSeparator = 179,
    CurrencyUnit = 180,
    CurrencySubUnit = 181,
    KeyPadLeftParenthesis = 182,
    KeyPadRightParenthesis = 183,
    KeyPadLeftBrace = 184,
    KeyPadRightBrace = 185,
    KeyPadTab = 186,
    KeyPadBackspace = 187,
    KeyPadA = 188,
    KeyPadB = 189,
    KeyPadC = 190,
    KeyPadD = 191,
    KeyPadE = 192,
    KeyPadF = 193,
    KeyPadXOR = 194,
    KeyPadPower = 195,
    KeyPadPercent = 196,
    KeyPadLess = 197,
    KeyPadGreater = 198,
    KeyPadAmpersand = 199,
    KeyPadDoubleAmpersand = 200,
    KeyPadVerticalBar = 201,
    KeyPadDoubleVerticalBar = 202,
    KeyPadColon = 203,
    KeyPadHash = 204,
    KeyPadSpace = 205,
    KeyPadAt = 206,
    KeyPadExclamation = 207,
    KeyPadMemoryStore = 208,
    KeyPadMemoryRecall = 209,
    KeyPadMemoryClear = 210,
    KeyPadMemoryAdd = 211,
    KeyPadMemorySubtract = 212,
    KeyPadMemoryMultiply = 213,
    KeyPadMemoryDivide = 214,
    KeyPadPlusMinus = 215,
    KeyPadClear = 216,
    KeyPadClearEntry = 217,
    KeyPadBinary = 218,
    KeyPadOctal = 219,
    KeyPadDecimal = 220,
    KeyPadHexadecimal = 221,

    LeftControl = 224,
    LeftShift = 225,
    LeftAlt = 226,
    LeftGui = 227, // Windows/Command key
    RightControl = 228,
    RightShift = 229,
    RightAlt = 230,
    RightGui = 231, // Windows/Command key
};

// Mouse buttons.
enum MouseButton {
    Left,
    Right,
    Middle,
    Extra1,
    Extra2,
};

// 1D mouse axes.
enum MouseAxis1D {
    Vertical,
    Horizontal,
    WheelVertical,
    WheelHorizontal,
};

// 2D mouse axes.
enum MouseAxis2D {
    Mouse,
    Wheel,
};

// Xbox (or compatible gamepad) buttons
enum class GamepadButton {
    None,

    A,
    B,
    X,
    Y,
    LeftBumper,
    RightBumper,
    Back,
    Start,
    Guide,
    LeftThumb,
    RightThumb,
    DpadUp,
    DpadDown,
    DpadLeft,
    DpadRight,
    LeftPaddle1,
    LeftPaddle2,
    RightPaddle1,
    RightPaddle2,
    TouchPad,
    Misc1,
    Misc2,
    Misc3,
    Misc4,
    Misc5,
    Misc6,
};

// Xbox (or compatible gamepad) 1D axes.
enum class GamepadAxis1D {
    None,

    LeftStickX,
    LeftStickY,
    RightStickX,
    RightStickY,
    LeftTrigger,
    RightTrigger,
};

// Xbox (or compatible gamepad) 2D axes.
enum class GamepadAxis2D {
    None,

    LeftStick,
    RightStick,
};

// Combination of a keyboard key and key modifiers.
struct KeyCombo {
    KeyModifier modifiers;
    KeyboardKey key;

    KeyCombo() = default;
    KeyCombo(const KeyCombo &) = default;
    KeyCombo(KeyCombo &&) = default;

    KeyCombo(KeyboardKey key)
        : modifiers(KeyModifier::None)
        , key(key) {}

    KeyCombo(KeyModifier modifiers, KeyboardKey key)
        : modifiers(modifiers)
        , key(key) {}

    KeyCombo &operator=(const KeyCombo &) = default;
    KeyCombo &operator=(KeyCombo &&) = default;

    bool operator==(const KeyCombo &) const = default;
};

// Combination of a mouse button and key modifiers.
struct MouseCombo {
    KeyModifier modifiers;
    MouseButton button;

    MouseCombo() = default;
    MouseCombo(const MouseCombo &) = default;
    MouseCombo(MouseCombo &&) = default;

    MouseCombo(MouseButton button)
        : modifiers(KeyModifier::None)
        , button(button) {}

    MouseCombo(KeyModifier modifiers, MouseButton button)
        : modifiers(modifiers)
        , button(button) {}

    MouseCombo &operator=(const MouseCombo &) = default;
    MouseCombo &operator=(MouseCombo &&) = default;

    bool operator==(const MouseCombo &) const = default;
};

// TODO: rename this; perhaps "BinaryInputEvent"?
// An input event comprises any discrete event generated by an input device.
// Includes keyboard keys, mouse buttons, gamepad or joystick buttons and key combos.
struct InputEvent {
    enum class Type {
        None,
        KeyboardKey,
        KeyCombo,
        MouseButton,
        MouseCombo,
        GamepadButton,
        JoystickButton,
    };
    Type type = Type::None;

    union {
        KeyboardKey keyboardKey;
        KeyCombo keyCombo;
        MouseButton mouseButton;
        MouseCombo mouseCombo;
        struct {
            uint32 id;
            GamepadButton button;
        } gamepad;
        int joystickButton;
    };

    InputEvent()
        : type(Type::None) {}

    InputEvent(KeyboardKey key)
        : type(Type::KeyboardKey)
        , keyboardKey(key) {}

    InputEvent(KeyCombo keyCombo)
        : type(Type::KeyCombo)
        , keyCombo(keyCombo) {}

    InputEvent(MouseButton mouseButton)
        : type(Type::MouseButton)
        , mouseButton(mouseButton) {}

    InputEvent(MouseCombo mouseCombo)
        : type(Type::MouseCombo)
        , mouseCombo(mouseCombo) {}

    InputEvent(uint32 id, GamepadButton button)
        : type(Type::GamepadButton)
        , gamepad{.id = id, .button = button} {}

    InputEvent(int joystickButton)
        : type(Type::JoystickButton)
        , joystickButton(joystickButton) {}

    constexpr bool operator==(const InputEvent &rhs) const {
        if (type != rhs.type) {
            return false;
        }
        switch (type) {
        case Type::None: return true;
        case Type::KeyboardKey: return keyboardKey == rhs.keyboardKey;
        case Type::KeyCombo: return keyCombo == rhs.keyCombo;
        case Type::MouseButton: return mouseButton == rhs.mouseButton;
        case Type::MouseCombo: return mouseCombo == rhs.mouseCombo;
        case Type::GamepadButton: return gamepad.id == rhs.gamepad.id && gamepad.button == rhs.gamepad.button;
        case Type::JoystickButton: return joystickButton == rhs.joystickButton;
        default: return false;
        }
    }
};

// TODO: rename this
// An input event with an activation state.
struct BoundInputEvent {
    InputEvent event;
    bool activated; // Corresponds to button pressed/released state

    BoundInputEvent()
        : activated(false) {}

    BoundInputEvent(InputEvent event, bool activated)
        : event(event)
        , activated(activated) {}

    constexpr bool operator==(const BoundInputEvent &rhs) const = default;
};

// TODO: rename this
// Groups together an action ID and the context value.
struct BoundAction {
    ActionID id;
    ActionContext context;

    bool operator==(const BoundAction &) const = default;
};

// TODO: rename this
// Groups together an input event with the activation state, the action ID and its context.
struct InputActionEvent {
    BoundInputEvent input;
    BoundAction action;
};

// ---------------------------------------------------------------------------------------------------------------------
// String converters

std::string ToString(KeyModifier mod);
std::string_view ToString(KeyboardKey key);
std::string_view ToString(MouseButton btn);
std::string_view ToString(MouseAxis1D axis);
std::string_view ToString(MouseAxis2D axis);
std::string_view ToString(GamepadButton btn);
std::string_view ToString(GamepadAxis1D axis);
std::string_view ToString(GamepadAxis2D axis);
std::string ToString(const KeyCombo &combo);
std::string ToString(const MouseCombo &combo);

// ---------------------------------------------------------------------------------------------------------------------
// String parsers

bool TryParse(std::string_view str, KeyModifier &mod);
bool TryParse(std::string_view str, KeyboardKey &key);
bool TryParse(std::string_view str, MouseButton &btn);
bool TryParse(std::string_view str, MouseAxis1D &axis);
bool TryParse(std::string_view str, MouseAxis2D &axis);
bool TryParse(std::string_view str, GamepadButton &btn);
bool TryParse(std::string_view str, GamepadAxis1D &axis);
bool TryParse(std::string_view str, GamepadAxis2D &axis);
bool TryParse(std::string_view str, KeyCombo &combo);
bool TryParse(std::string_view str, MouseCombo &combo);

} // namespace app::input

// ---------------------------------------------------------------------------------------------------------------------
// Utility and hashing

ENABLE_BITMASK_OPERATORS(app::input::KeyModifier);

template <>
struct std::hash<app::input::InputEvent> {
    std::size_t operator()(const app::input::InputEvent &e) const noexcept {
        using Type = app::input::InputEvent::Type;

        std::size_t base = 0;
        switch (e.type) {
        case Type::None: base = 0; break;
        case Type::KeyboardKey: base = static_cast<std::size_t>(e.keyboardKey); break;
        case Type::KeyCombo:
            base = static_cast<std::size_t>(e.keyCombo.key) | (static_cast<std::size_t>(e.keyCombo.modifiers) << 24ull);
            break;
        case Type::MouseButton: base = static_cast<std::size_t>(e.mouseButton); break;
        case Type::MouseCombo:
            base = static_cast<std::size_t>(e.mouseCombo.button) |
                   (static_cast<std::size_t>(e.mouseCombo.modifiers) << 24ull);
            break;
        case Type::GamepadButton:
            base = static_cast<std::size_t>(e.gamepad.button) | (static_cast<std::size_t>(e.gamepad.id) << 24ull);
            break;
        case Type::JoystickButton: base = static_cast<std::size_t>(e.joystickButton); break;
        }
        return base | (static_cast<std::size_t>(e.type) << 32ull);
    }
};

template <>
struct std::hash<app::input::BoundInputEvent> {
    std::size_t operator()(const app::input::BoundInputEvent &e) const noexcept {
        return std::hash<app::input::InputEvent>{}(e.event) | (static_cast<std::size_t>(e.activated) << 63ull);
    }
};

template <>
struct std::hash<app::input::BoundAction> {
    std::size_t operator()(const app::input::BoundAction &e) const noexcept {
        return (std::hash<app::input::ActionID>{}(e.id) << 1ull) ^ std::hash<app::input::ActionContext>{}(e.context);
    }
};
