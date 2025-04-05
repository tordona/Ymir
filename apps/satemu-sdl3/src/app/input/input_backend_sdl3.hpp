#pragma once

#include "input_defs.hpp"

#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_scancode.h>

namespace app::input {

KeyboardKey SDL3ScancodeToKeyboardKey(SDL_Scancode key);
SDL_Scancode KeyboardKeyToSDL3Scancode(KeyboardKey key);

KeyModifier SDL3ToKeyModifier(SDL_Keymod modifier);
SDL_Keymod KeyModifierToSDL3(KeyModifier modifier);

GamepadButton SDL3ToGamepadButton(SDL_GamepadButton button);
SDL_GamepadButton GamepadButtonToSDL3(GamepadButton button);

GamepadAxis SDL3ToGamepadAxis(SDL_GamepadAxis axis);
SDL_GamepadAxis GamepadAxisToSDL3(GamepadAxis axis);

} // namespace app::input
