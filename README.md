# Ymir
A work-in-progress Sega Saturn emulator.

[![Release](https://github.com/StrikerX3/Ymir/actions/workflows/release.yaml/badge.svg)](https://github.com/StrikerX3/Ymir/actions/workflows/release.yaml)

<div class="grid" markdown>
  <img width="49%" src="https://github.com/StrikerX3/Ymir/blob/main/docs/images/cd-player.png"/>
  <img width="49%" src="https://github.com/StrikerX3/Ymir/blob/main/docs/images/sonic-r.png"/>
  <img width="49%" src="https://github.com/StrikerX3/Ymir/blob/main/docs/images/virtua-fighter-2.png"/>
  <img width="49%" src="https://github.com/StrikerX3/Ymir/blob/main/docs/images/radiant-silvergun.png"/>
  <img width="49%" src="https://github.com/StrikerX3/Ymir/blob/main/docs/images/panzer-dragoon-saga.png"/>
  <img width="49%" src="https://github.com/StrikerX3/Ymir/blob/main/docs/images/nights-into-dreams.png"/>
  <img width="100%" src="https://github.com/StrikerX3/Ymir/blob/main/docs/images/debugger.png"/>
</div>

## Usage

Grab the latest release [here](https://github.com/StrikerX3/Ymir/releases/latest).
Check the [Releases](https://github.com/StrikerX3/Ymir/releases) page for previous versions.

Ymir does not require installation. Simply download it to any directory and run the executable.

On Windows you might also need to install the latest Microsoft Visual C++ Redistributable package ([download here](https://aka.ms/vs/17/release/vc_redist.x64.exe)).

The program accepts command-line arguments. Invoke `ymir-sdl3 --help` to list the options:

```
Ymir - Sega Saturn emulator
Usage:
  Ymir [OPTION...] positional parameters

  -i, --ipl arg      Path to Saturn IPL ROM
  -p, --profile arg  Path to profile directory
  -h, --help         Display help text
```

Use `-p <profile-path>` to point to a separate set of configuration and state files, useful if you wish to have different user profiles (hence the name).

The `-i` option is deprecated and will be removed in a future release.

Note that the Win32 variant of Ymir does not output anything to the console, but it does honor the command line parameters.

Ymir requires an IPL (BIOS) ROM to work. You can place the ROMs under the `roms` directory created alongside the executable on the first run.
The emulator will scan and automatically select the IPL ROM matching the loaded disc. If no disc is loaded, it will use a ROM matching the first preferred region. Failing that, it will pick whatever is available.
You can override the selection on Settings > IPL.

Ymir can load game disc images from BIN+CUE, IMG+CCD, MDF+MDS or ISO files. It does not support loading .elf files directly at the moment.


## Compiling

See [COMPILING.md](COMPILING.md).
