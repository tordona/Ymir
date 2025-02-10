#include <satemu/satemu.hpp>

#include <satemu/util/scope_guard.hpp>
#include <satemu/util/thread_name.hpp>

#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>
#include <fmt/format.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <span>
#include <thread>
#include <vector>

using namespace util;

std::vector<uint8> loadFile(std::filesystem::path romPath) {
    fmt::print("Loading file {}... ", romPath.string());

    std::vector<uint8> data;
    std::ifstream stream{romPath, std::ios::binary | std::ios::ate};
    if (stream.is_open()) {
        auto size = stream.tellg();
        stream.seekg(0, std::ios::beg);
        fmt::println("{} bytes", (std::size_t)size);

        data.resize(size);
        stream.read(reinterpret_cast<char *>(data.data()), size);
    } else {
        fmt::println("failed!");
    }
    return data;
}

void runEmulator(satemu::Saturn &saturn) {
    util::SetCurrentThreadName("Main thread");

    using clk = std::chrono::steady_clock;
    using namespace std::chrono_literals;
    using namespace satemu;

    // Screen parameters
    const uint32 scale = 4;
    struct ScreenParams {
        uint32 width = 320;
        uint32 height = 224;
        float scaleX = scale;
        float scaleY = scale;
        SDL_Window *window = nullptr;
    } screen;

    // ---------------------------------
    // Initialize SDL video subsystem

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS)) {
        SDL_Log("Unable to initialize SDL: %s", SDL_GetError());
        return;
    }
    ScopeGuard sgQuit{[] { SDL_Quit(); }};

    // ---------------------------------
    // Create window

    SDL_PropertiesID windowProps = SDL_CreateProperties();
    if (windowProps == 0) {
        SDL_Log("Unable to create window properties: %s", SDL_GetError());
        return;
    }
    ScopeGuard sgDestroyWindowProps{[&] { SDL_DestroyProperties(windowProps); }};

    // Assume the following calls succeed
    SDL_SetStringProperty(windowProps, SDL_PROP_WINDOW_CREATE_TITLE_STRING, "Unnamed Sega Saturn emulator");
    SDL_SetBooleanProperty(windowProps, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, false);
    SDL_SetNumberProperty(windowProps, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, screen.width * screen.scaleX);
    SDL_SetNumberProperty(windowProps, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, screen.height * screen.scaleY);
    SDL_SetNumberProperty(windowProps, SDL_PROP_WINDOW_CREATE_X_NUMBER, SDL_WINDOWPOS_CENTERED);
    SDL_SetNumberProperty(windowProps, SDL_PROP_WINDOW_CREATE_Y_NUMBER, SDL_WINDOWPOS_CENTERED);

    screen.window = SDL_CreateWindowWithProperties(windowProps);
    if (screen.window == nullptr) {
        SDL_Log("Unable to create window: %s", SDL_GetError());
        return;
    }
    ScopeGuard sgDestroyWindow{[&] { SDL_DestroyWindow(screen.window); }};

    // ---------------------------------
    // Create renderer

    SDL_PropertiesID rendererProps = SDL_CreateProperties();
    if (rendererProps == 0) {
        SDL_Log("Unable to create renderer properties: %s", SDL_GetError());
        return;
    }
    ScopeGuard sgDestroyRendererProps{[&] { SDL_DestroyProperties(rendererProps); }};

    // Assume the following calls succeed
    SDL_SetPointerProperty(rendererProps, SDL_PROP_RENDERER_CREATE_WINDOW_POINTER, screen.window);
    SDL_SetNumberProperty(rendererProps, SDL_PROP_RENDERER_CREATE_PRESENT_VSYNC_NUMBER, SDL_RENDERER_VSYNC_DISABLED);
    // SDL_SetNumberProperty(rendererProps, SDL_PROP_RENDERER_CREATE_PRESENT_VSYNC_NUMBER, SDL_RENDERER_VSYNC_ADAPTIVE);
    // SDL_SetNumberProperty(rendererProps, SDL_PROP_RENDERER_CREATE_PRESENT_VSYNC_NUMBER, 1);

    auto renderer = SDL_CreateRendererWithProperties(rendererProps);
    if (renderer == nullptr) {
        SDL_Log("Unable to create renderer: %s", SDL_GetError());
        return;
    }
    ScopeGuard sgDestroyRenderer{[&] { SDL_DestroyRenderer(renderer); }};

    // ---------------------------------
    // Create texture to render on

    auto texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_XBGR8888, SDL_TEXTUREACCESS_STREAMING, vdp::kMaxResH,
                                     vdp::kMaxResV);
    if (texture == nullptr) {
        SDL_Log("Unable to create texture: %s", SDL_GetError());
        return;
    }
    ScopeGuard sgDestroyTexture{[&] { SDL_DestroyTexture(texture); }};

    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);

    // ---------------------------------
    // Setup framebuffer and render callbacks

    std::vector<uint32> framebuffer(vdp::kMaxResH * vdp::kMaxResV);
    saturn.VDP.SetCallbacks({framebuffer.data(), [](uint32, uint32, void *ctx) { return (uint32 *)ctx; }},
                            {&screen, [](vdp::FramebufferColor *, uint32 width, uint32 height, void *ctx) {
                                 auto &screen = *static_cast<ScreenParams *>(ctx);
                                 if (width != screen.width || height != screen.height) {
                                     const bool doubleWidth = width >= 640;
                                     const bool doubleHeight = height >= 400;

                                     const float scaleX = doubleWidth ? scale * 0.5f : scale;
                                     const float scaleY = doubleHeight ? scale * 0.5f : scale;

                                     auto normalizeW = [](int width) { return (width >= 640) ? width / 2 : width; };
                                     auto normalizeH = [](int height) { return (height >= 400) ? height / 2 : height; };

                                     int wx, wy;
                                     SDL_GetWindowPosition(screen.window, &wx, &wy);
                                     const int dx = (int)normalizeW(width) - (int)normalizeW(screen.width);
                                     const int dy = (int)normalizeH(height) - (int)normalizeH(screen.height);
                                     screen.width = width;
                                     screen.height = height;

                                     // Adjust window size dynamically
                                     // TODO: add room for borders
                                     SDL_SetWindowSize(screen.window, screen.width * scaleX, screen.height * scaleY);
                                     SDL_SetWindowPosition(screen.window, wx - dx * scaleX / 2, wy - dy * scaleY / 2);
                                 }
                             }});

    // ---------------------------------
    // Create audio buffer and stream and set up callbacks

    // Use a smaller buffer to reduce audio latency
    SDL_SetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, "512");

    struct AudioBuffer {
        std::array<sint16, 4096> buffer{};
        uint32 readPos = 0;
        uint32 writePos = 0;
        bool sync = true;
    } audioBuffer{};

    SDL_AudioSpec audioSpec{};
    audioSpec.freq = 44100;
    audioSpec.format = SDL_AUDIO_S16;
    audioSpec.channels = 2;
    auto audioStream = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &audioSpec,
        [](void *userdata, SDL_AudioStream *stream, int additional_amount, int total_amount) {
            auto &buffer = *reinterpret_cast<AudioBuffer *>(userdata);
            int sampleCount = additional_amount / sizeof(sint16);
            int len1 = std::min<int>(sampleCount, buffer.buffer.size() - buffer.readPos);
            int len2 = std::min<int>(sampleCount - len1, buffer.readPos);
            SDL_PutAudioStreamData(stream, &buffer.buffer[buffer.readPos], len1 * sizeof(sint16));
            SDL_PutAudioStreamData(stream, &buffer.buffer[0], len2 * sizeof(sint16));

            buffer.readPos = (buffer.readPos + len1 + len2) % buffer.buffer.size();
        },
        &audioBuffer);
    if (audioStream == nullptr) {
        SDL_Log("Unable to create audio stream: %s", SDL_GetError());
        return;
    }
    ScopeGuard sgDestroyAudioStream{[&] { SDL_DestroyAudioStream(audioStream); }};

    // please don't burst my eardrums while I test audio
    SDL_SetAudioStreamGain(audioStream, 0.8f);

    if (!SDL_ResumeAudioStreamDevice(audioStream)) {
        SDL_Log("Unable to start audio stream: %s", SDL_GetError());
    }
    {
        SDL_AudioSpec srcSpec{};
        SDL_AudioSpec dstSpec{};
        SDL_GetAudioStreamFormat(audioStream, &srcSpec, &dstSpec);
        auto formatName = [&] {
            switch (srcSpec.format) {
            case SDL_AUDIO_U8: return "unsigned 8-bit PCM";
            case SDL_AUDIO_S8: return "signed 8-bit PCM";
            case SDL_AUDIO_S16LE: return "signed 16-bit little-endian integer PCM";
            case SDL_AUDIO_S16BE: return "signed 16-bit big-endian integer PCM";
            case SDL_AUDIO_S32LE: return "signed 32-bit little-endian integer PCM";
            case SDL_AUDIO_S32BE: return "signed 32-bit big-endian integer PCM";
            case SDL_AUDIO_F32LE: return "32-bit little-endian floating point PCM";
            case SDL_AUDIO_F32BE: return "32-bit big-endian floating point PCM";
            default: return "unknown";
            }
        };

        fmt::println("Audio stream opened: {} Hz, {} channel{}, {} format", srcSpec.freq, srcSpec.channels,
                     (srcSpec.channels == 1 ? "" : "s"), formatName());
        if (srcSpec.freq != audioSpec.freq || srcSpec.channels != audioSpec.channels ||
            srcSpec.format != audioSpec.format) {
            // Hopefully this never happens
            fmt::println("Audio format mismatch");
            return;
        }
    }

    saturn.SCSP.SetCallback({&audioBuffer, [](sint16 left, sint16 right, void *ctx) {
                                 auto &buffer = *reinterpret_cast<AudioBuffer *>(ctx);

                                 // TODO: these busy waits should go away
                                 while (buffer.sync && (buffer.writePos + 1) % buffer.buffer.size() == buffer.readPos) {
                                     std::this_thread::yield();
                                 }
                                 buffer.buffer[buffer.writePos] = left;
                                 buffer.writePos = (buffer.writePos + 1) % buffer.buffer.size();

                                 while (buffer.sync && (buffer.writePos + 1) % buffer.buffer.size() == buffer.readPos) {
                                     std::this_thread::yield();
                                 }
                                 buffer.buffer[buffer.writePos] = right;
                                 buffer.writePos = (buffer.writePos + 1) % buffer.buffer.size();
                             }});

    // ---------------------------------
    // Main emulator loop

    saturn.Reset(true);

    auto t = clk::now();
    uint64 frames = 0;
    bool running = true;
    uint16 &buttons = saturn.SMPC.Buttons();

    auto setClearButton = [&](uint16 bits, bool pressed) {
        if (pressed) {
            buttons &= ~bits;
        } else {
            buttons |= bits;
        }
    };

    auto updateButton = [&](SDL_Scancode scancode, SDL_Keymod mod, bool pressed) {
        switch (scancode) {
        case SDL_SCANCODE_W: setClearButton(smpc::kButtonUp, pressed); break;
        case SDL_SCANCODE_A: setClearButton(smpc::kButtonLeft, pressed); break;
        case SDL_SCANCODE_S: setClearButton(smpc::kButtonDown, pressed); break;
        case SDL_SCANCODE_D: setClearButton(smpc::kButtonRight, pressed); break;
        case SDL_SCANCODE_UP: setClearButton(smpc::kButtonUp, pressed); break;
        case SDL_SCANCODE_LEFT: setClearButton(smpc::kButtonLeft, pressed); break;
        case SDL_SCANCODE_DOWN: setClearButton(smpc::kButtonDown, pressed); break;
        case SDL_SCANCODE_RIGHT: setClearButton(smpc::kButtonRight, pressed); break;
        case SDL_SCANCODE_Q: setClearButton(smpc::kButtonL, pressed); break;
        case SDL_SCANCODE_E: setClearButton(smpc::kButtonR, pressed); break;
        case SDL_SCANCODE_J: setClearButton(smpc::kButtonA, pressed); break;
        case SDL_SCANCODE_K: setClearButton(smpc::kButtonB, pressed); break;
        case SDL_SCANCODE_L: setClearButton(smpc::kButtonC, pressed); break;
        case SDL_SCANCODE_U: setClearButton(smpc::kButtonX, pressed); break;
        case SDL_SCANCODE_I: setClearButton(smpc::kButtonY, pressed); break;
        case SDL_SCANCODE_O: setClearButton(smpc::kButtonZ, pressed); break;
        case SDL_SCANCODE_G: setClearButton(smpc::kButtonStart, pressed); break;
        case SDL_SCANCODE_H: setClearButton(smpc::kButtonStart, pressed); break;
        case SDL_SCANCODE_RETURN: setClearButton(smpc::kButtonStart, pressed); break;
        case SDL_SCANCODE_RETURN2: setClearButton(smpc::kButtonStart, pressed); break;
        case SDL_SCANCODE_R:
            if (pressed) {
                if (mod & SDL_KMOD_CTRL) {
                    saturn.Reset(true);
                }
            }
            if (mod & SDL_KMOD_SHIFT) {
                saturn.SMPC.SetResetButtonState(pressed);
            }
            break;
        case SDL_SCANCODE_TAB: audioBuffer.sync = !pressed; break;
        case SDL_SCANCODE_F3:
            if (pressed) {
                {
                    std::ofstream out{"wram-lo.bin", std::ios::binary};
                    saturn.SH2.bus.DumpWRAMLow(out);
                }
                {
                    std::ofstream out{"wram-hi.bin", std::ios::binary};
                    saturn.SH2.bus.DumpWRAMHigh(out);
                }
                {
                    std::ofstream out{"vdp1-vram.bin", std::ios::binary};
                    saturn.VDP.DumpVDP1VRAM(out);
                }
                {
                    std::ofstream out{"vdp1-fbs.bin", std::ios::binary};
                    saturn.VDP.DumpVDP1Framebuffers(out);
                }
                {
                    std::ofstream out{"vdp2-vram.bin", std::ios::binary};
                    saturn.VDP.DumpVDP2VRAM(out);
                }
                {
                    std::ofstream out{"vdp2-cram.bin", std::ios::binary};
                    saturn.VDP.DumpVDP2CRAM(out);
                }
                {
                    std::ofstream out{"scu-dsp-prog.bin", std::ios::binary};
                    saturn.SCU.DumpDSPProgramRAM(out);
                }
                {
                    std::ofstream out{"scu-dsp-data.bin", std::ios::binary};
                    saturn.SCU.DumpDSPDataRAM(out);
                }
                {
                    std::ofstream out{"scu-dsp-regs.bin", std::ios::binary};
                    saturn.SCU.DumpDSPRegs(out);
                }
                {
                    std::ofstream out{"scsp-wram.bin", std::ios::binary};
                    saturn.SCSP.DumpWRAM(out);
                }
                {
                    std::ofstream out{"scsp-dsp-mpro.bin", std::ios::binary};
                    saturn.SCSP.DumpDSP_MPRO(out);
                }
                {
                    std::ofstream out{"scsp-dsp-temp.bin", std::ios::binary};
                    saturn.SCSP.DumpDSP_TEMP(out);
                }
                {
                    std::ofstream out{"scsp-dsp-mems.bin", std::ios::binary};
                    saturn.SCSP.DumpDSP_MEMS(out);
                }
                {
                    std::ofstream out{"scsp-dsp-coef.bin", std::ios::binary};
                    saturn.SCSP.DumpDSP_COEF(out);
                }
                {
                    std::ofstream out{"scsp-dsp-madrs.bin", std::ios::binary};
                    saturn.SCSP.DumpDSP_MADRS(out);
                }
                {
                    std::ofstream out{"scsp-dsp-mixs.bin", std::ios::binary};
                    saturn.SCSP.DumpDSP_MIXS(out);
                }
                {
                    std::ofstream out{"scsp-dsp-efreg.bin", std::ios::binary};
                    saturn.SCSP.DumpDSP_EFREG(out);
                }
                {
                    std::ofstream out{"scsp-dsp-exts.bin", std::ios::binary};
                    saturn.SCSP.DumpDSP_EXTS(out);
                }
                {
                    std::ofstream out{"scsp-dsp-regs.bin", std::ios::binary};
                    saturn.SCSP.DumpDSPRegs(out);
                }
            }
            break;
        default: break;
        }
    };

    while (running) {
        SDL_Event evt{};
        while (SDL_PollEvent(&evt)) {
            switch (evt.type) {
            case SDL_EVENT_KEY_DOWN: updateButton(evt.key.scancode, evt.key.mod, true); break;
            case SDL_EVENT_KEY_UP: updateButton(evt.key.scancode, evt.key.mod, false); break;
            case SDL_EVENT_QUIT: running = false; break;
            }
        }

        // For the timings below, we introduce a counting factor in order to avoid rounding errors from the division.
        // At each step we increment the current cycle count by the factor, effectively enabling fractional cycle
        // counting with pure integer math.

        // NTSC timings at the default (fast) clock rate
        // static constexpr uint64 kClockRate = 28'636'360;
        // static constexpr uint64 kFramesPerSecond = 60;
        // static constexpr uint64 kCycleCountingFactor = 3;
        // static constexpr uint64 kCyclesPerFrame = kClockRate * kCycleCountingFactor / kFramesPerSecond;

        // PAL timings at the default (fast) clock rate
        // static constexpr uint64 kClockRate = 28'437'500;
        // static constexpr uint64 kFramesPerSecond = 50;
        // static constexpr uint64 kCycleCountingFactor = 1;
        // static constexpr uint64 kCyclesPerFrame = kClockRate * kCycleCountingFactor / kFramesPerSecond;

        /*for (int i = 0; i < kCyclesPerFrame; i += kCycleCountingFactor) {
            saturn.Step();
        }*/

        saturn.RunFrame();

        ++frames;
        auto t2 = clk::now();
        if (t2 - t >= 1s) {
            const media::Disc &disc = saturn.CDBlock.GetDisc();
            const media::SaturnHeader &header = disc.header;
            auto title = fmt::format("[{}] {} - {} fps", header.productNumber, header.gameTitle, frames);
            SDL_SetWindowTitle(screen.window, title.c_str());
            frames = 0;
            t = t2;
        }

        uint32 *pixels = nullptr;
        int pitch = 0;
        SDL_Rect area{.x = 0, .y = 0, .w = (int)screen.width, .h = (int)screen.height};
        if (SDL_LockTexture(texture, &area, (void **)&pixels, &pitch)) {
            for (uint32 y = 0; y < screen.height; y++) {
                std::copy_n(&framebuffer[y * screen.width], screen.width, &pixels[y * pitch / sizeof(uint32)]);
            }
            // std::copy_n(framebuffer.begin(), screen.width * screen.height, pixels);
            SDL_UnlockTexture(texture);
        }

        SDL_RenderClear(renderer);
        SDL_FRect srcRect{.x = 0.0f, .y = 0.0f, .w = (float)screen.width, .h = (float)screen.height};
        SDL_RenderTexture(renderer, texture, &srcRect, nullptr);
        SDL_RenderPresent(renderer);
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
}

int main(int argc, char **argv) {
    fmt::println("satemu {}", satemu::version::string);
    if (argc < 2) {
        fmt::println("missing argument: biospath [discpath]");
        fmt::println("    biospath   Path to Saturn BIOS ROM");
        fmt::println("    discpath   Path to Saturn disc image (.ccd, .cue, .iso, .mds)");
        return EXIT_FAILURE;
    }

    auto saturn = std::make_unique<satemu::Saturn>();
    {
        constexpr auto iplSize = satemu::sh2::kIPLSize;
        auto rom = loadFile(argv[1]);
        if (rom.size() != iplSize) {
            fmt::println("IPL ROM size mismatch: expected {} bytes, got {} bytes", iplSize, rom.size());
            return EXIT_FAILURE;
        }
        saturn->LoadIPL(std::span<uint8, iplSize>(rom));
        fmt::println("IPL ROM loaded");
    }
    if (argc > 2) {
        satemu::media::Disc disc{};
        if (!satemu::media::LoadDisc(argv[2], disc)) {
            fmt::println("Failed to load media from {}", argv[2]);
            return EXIT_FAILURE;
        }
        fmt::println("Loaded disc image from {}", argv[2]);
        saturn->LoadDisc(std::move(disc));
    }
    runEmulator(*saturn);

    return EXIT_SUCCESS;
}
