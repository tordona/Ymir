# Ymir
A work-in-progress Sega Saturn emulator.

[![Release](https://github.com/StrikerX3/Ymir/actions/workflows/release.yaml/badge.svg)](https://github.com/StrikerX3/Ymir/actions/workflows/release.yaml)

![CD player](https://github.com/StrikerX3/Ymir/blob/main/docs/images/cd-player.png) ![Sonic R](https://github.com/StrikerX3/Ymir/blob/main/docs/images/sonic-r.png)  
![Virtua Fighter 2](https://github.com/StrikerX3/Ymir/blob/main/docs/images/virtua-fighter-2.png) ![Radiant Silvergun](https://github.com/StrikerX3/Ymir/blob/main/docs/images/radiant-silvergun.png)  
![Panzer Dragoon Saga](https://github.com/StrikerX3/Ymir/blob/main/docs/images/panzer-dragoon-saga.png) ![NiGHTS into Dreams...](https://github.com/StrikerX3/Ymir/blob/main/docs/images/nights-into-dreams.png)  
![Debugger](https://github.com/StrikerX3/Ymir/blob/main/docs/images/debugger.png)

## Usage

Grab the latest release [here](https://github.com/StrikerX3/Ymir/releases/latest).
Check the [Releases](https://github.com/StrikerX3/Ymir/releases) page for previous versions.

Ymir does not require installation. Simply download it to any directory and run the executable.

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


## Building

Ymir requires [CMake 3.28+](https://cmake.org/) and a C++20 compiler.

The repository includes several vendored dependencies as Git submodules. When cloning, make sure to include the `--recurse-submodules` parameter.
You can also initialize submodules with `git submodule update --init --recursive` after a `git clone` or when pulling changes.

Ymir has been successfully compiled with the following toolchains:
- Visual Studio 2022's Clang 19.1.1
- Visual Studio 2022's MSVC 19.43.34810.0
- Clang 15.0.7 on WSL Ubuntu 22.04.5 LTS
- Clang 19.1.1 on Ubuntu 24.04.2 LTS

The project has been compiled for x86_64 Windows and Linux platforms only. It may or may not compile for ARM64 or macOS.


### Build configuration

You can tune the build with following CMake options:

- `Ymir_AVX2` (`BOOL`): Set to `ON` to use AVX2 extensions. `OFF` uses the platform's default instruction set, typically SSE2. Disabled by default.
- `Ymir_ENABLE_TESTS` (`BOOL`): Includes the unit test project in the build. Enabled by default if this is the top level CMake project.
- `Ymir_ENABLE_SANDBOX` (`BOOL`): Includes the sandbox project in the build. Enabled by default if this is the top level CMake project.
- `Ymir_ENABLE_IPO` (`BOOL`): Enables interprocedural optimizations (also called link-time optimizations) on all projects. Enabled by default.
- `Ymir_ENABLE_DEVLOG` (`BOOL`): Enables logs meant to aid development. Enabled by default.
- `Ymir_ENABLE_IMGUI_DEMO` (`BOOL`): Enables the ImGui demo window, useful as a reference when developing new UI elements. Enabled by default.

For a Release build, you might want to disable the devlog and ImGui demo window to maximize performance and reduce the binary size.

It is highly recommended to use [Ninja](https://ninja-build.org/) as it greatly accelerates the build process, especially on machines with high CPU core counts.


### Building on Windows

To build Ymir on Windows, you will need [Visual Studio 2022 Community](https://visualstudio.microsoft.com/vs/community/) and [CMake 3.28+](https://cmake.org/).
Clang is highly recommended over MSVC as it produces much higher quality code, outperforming MSVC by 50-80%. However, MSVC tends to provide a better debugging experience.

All dependencies are included in the `vendor` directory and are built together with the emulator. No external dependencies are needed.

You can choose to generate a .sln file with CMake or open the directory directly with Visual Studio.
Both methods work, but opening the directory allows Visual Studio to use Ninja for significantly faster build times.


### Building on Linux

To build Ymir on Linux, first you will need to install SDL3's required dependencies. Follow the instructions on [this page](https://wiki.libsdl.org/SDL3/README/linux) to install them.

The compiler of choice for this platform is Clang. GCC is not currently supported as there seems to be a bug which causes it to take an extremely long time to compile `sh2.cpp`.

Use CMake to generate a Makefile or (preferably) a Ninja build script:

```sh
cmake -S . -B build -G Ninja
```

Pass additional `-D<option>=<value>` parameters to tune the build. See the [Build configuration](#build-configuration) section above for details.

You can use CMake to build the project, regardless of generator:

```sh
cmake --build build --parallel
```
