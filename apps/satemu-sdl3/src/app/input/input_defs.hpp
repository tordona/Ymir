#pragma once

#include <satemu/core/types.hpp>

#include <satemu/util/bitmask_enum.hpp>

namespace app::input {

// Action identifier. Apps can use any mapping scheme they wish for actions.
// 4 billion different actions should be more than enough for any kind of app.
//
// Action 0 is reserved to represent "no action" or "unmapped binding".
using ActionID = uint32;

// The "no action" or "unmapped binding" action identifier.
inline constexpr ActionID kNoAction = 0;

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

// Xbox (or compatible gamepad) axes
enum class GamepadAxis {
    None,

    LeftX,
    LeftY,
    RightX,
    RightY,
    LeftTrigger,
    RightTrigger,
};

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

enum MouseButton {
    Left,
    Right,
    Middle,
    Extra1,
    Extra2,
};

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

    union Event {
        KeyboardKey keyboardKey;
        KeyCombo keyCombo;
        MouseButton mouseButton;
        MouseCombo mouseCombo;
        struct {
            uint32 id;
            GamepadButton button;
        } gamepad;
        int joystickButton;
    } event;

    // Corresponds to button pressed/released state
    bool activated;

    InputEvent()
        : type(Type::None)
        , activated(false) {}

    explicit InputEvent(KeyboardKey key, bool activated)
        : type(Type::KeyboardKey)
        , event({.keyboardKey = key})
        , activated(activated) {}

    explicit InputEvent(KeyCombo keyCombo, bool activated)
        : type(Type::KeyCombo)
        , event({.keyCombo = keyCombo})
        , activated(activated) {}

    explicit InputEvent(MouseButton mouseButton, bool activated)
        : type(Type::MouseButton)
        , event({.mouseButton = mouseButton})
        , activated(activated) {}

    explicit InputEvent(MouseCombo mouseCombo, bool activated)
        : type(Type::MouseCombo)
        , event({.mouseCombo = mouseCombo})
        , activated(activated) {}

    explicit InputEvent(uint32 id, GamepadButton button, bool activated)
        : type(Type::GamepadButton)
        , event({.gamepad{.id = id, .button = button}})
        , activated(activated) {}

    explicit InputEvent(int joystickButton, bool activated)
        : type(Type::JoystickButton)
        , event({.joystickButton = joystickButton})
        , activated(activated) {}

    struct Hash {
        std::size_t operator()(const InputEvent &e) const {
            std::size_t base = 0;
            switch (e.type) {
            case Type::None: base = 0; break;
            case Type::KeyboardKey: base = static_cast<std::size_t>(e.event.keyboardKey); break;
            case Type::KeyCombo:
                base = static_cast<std::size_t>(e.event.keyCombo.key) |
                       (static_cast<std::size_t>(e.event.keyCombo.modifiers) << 24ull);
                break;
            case Type::MouseButton: base = static_cast<std::size_t>(e.event.mouseButton); break;
            case Type::MouseCombo:
                base = static_cast<std::size_t>(e.event.mouseCombo.button) |
                       (static_cast<std::size_t>(e.event.mouseCombo.modifiers) << 24ull);
                break;
            case Type::GamepadButton:
                base = static_cast<std::size_t>(e.event.gamepad.button) |
                       (static_cast<std::size_t>(e.event.gamepad.id) << 24ull);
                break;
            case Type::JoystickButton: base = static_cast<std::size_t>(e.event.joystickButton); break;
            }
            return base | (static_cast<std::size_t>(e.type) << 32ull) |
                   (static_cast<std::size_t>(e.activated) << 63ull);
        }
    };

    bool operator==(const InputEvent &rhs) const {
        if (type != rhs.type) {
            return false;
        }
        if (activated != rhs.activated) {
            return false;
        }
        switch (type) {
        case Type::None: return true;
        case Type::KeyboardKey: return event.keyboardKey == rhs.event.keyboardKey;
        case Type::KeyCombo: return event.keyCombo == rhs.event.keyCombo;
        case Type::MouseButton: return event.mouseButton == rhs.event.mouseButton;
        case Type::MouseCombo: return event.mouseCombo == rhs.event.mouseCombo;
        case Type::GamepadButton:
            return event.gamepad.id == rhs.event.gamepad.id && event.gamepad.button == rhs.event.gamepad.button;
        case Type::JoystickButton: return event.joystickButton == rhs.event.joystickButton;
        default: return false;
        }
    }
};

struct InputActionEvent {
    InputEvent input;
    ActionID action;
};

} // namespace app::input

ENABLE_BITMASK_OPERATORS(app::input::KeyModifier);
