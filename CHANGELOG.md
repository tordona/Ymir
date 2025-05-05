# Ymir changelog

## Version 0.1.3

### New features and improvements

- Cartridge: Added 16 Mbit ROM cartridge for Ultraman: Hikari no Kyojin Densetsu and The King of Fighters '95 (#71)

### Fixes

- VDP1: Preserve EWDR, EWLR and EWRR on reset. Fixes some graphics glitches on Capcom games (#67).
- VDP2: RBGs would render incorrectly when starting the emulator with threaded VDP rendering disabled (#77).
- SCSP: More accuracy improvement and bug fixes (thanks to @celeriyacon)


## Version 0.1.2

### New features and improvements

- Input: Improved support for gamepads. You can now configure triggers as buttons, map analog sticks to the D-Pad, adjust deadzones, and more. (#36, #37, #54)
- Input: Added one more slot for input binds

### Fixes

- Input: Keys no longer get stuck when focusing windows, menus, etc.
- Input: Keys bound to controller on port 2 (by default: arrow keys, numpad and navigation block) should no longer prevent keys bound to the controller on port 1 from working. (#50)
- Several stability improvements (@Wunkolo)


## Version 0.1.1

### New features and improvements

- App: New logo and icon (thanks to @windy3862 on Discord!)
- App: Added Welcome window with instructions for first-time users
- App: Set initial window size based on display resolution
- App: Scale GUI based on system DPI scaling (#45)

### Fixes

- VDP1: Truncate polygon coordinates to 13 bits, fixing a short freeze in Virtua Fighter 2
- SCSP: Various accuracy improvements and bug fixes (thanks to @celeriyacon)
- CD Block: Fix errors when loading homebrew discs containing a single file
- Input: Properly handle gamepad buttons when binding inputs
- ymdasm: Fix disassembly skipping the very last instruction in files


## Version 0.1.0

Initial release.
