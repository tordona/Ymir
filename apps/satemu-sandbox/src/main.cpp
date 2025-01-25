#include <satemu/satemu.hpp>

#include <satemu/util/scope_guard.hpp>

#include "../../../libs/satemu-core/src/satemu/hw/vdp/slope.hpp"

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
    // Create audio buffer and stream and set up callbacks

    // Use a smaller buffer to reduce audio latency
    SDL_SetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, "512");

    struct AudioBuffer {
        std::array<sint16, 4096> buffer{};
        uint32 readPos = 0;
        uint32 writePos = 0;
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
    SDL_SetAudioStreamGain(audioStream, 0.15f);

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
                                 while ((buffer.writePos + 1) % buffer.buffer.size() == buffer.readPos) {
                                     std::this_thread::yield();
                                 }
                                 buffer.buffer[buffer.writePos] = left;
                                 buffer.writePos = (buffer.writePos + 1) % buffer.buffer.size();

                                 while ((buffer.writePos + 1) % buffer.buffer.size() == buffer.readPos) {
                                     std::this_thread::yield();
                                 }
                                 buffer.buffer[buffer.writePos] = right;
                                 buffer.writePos = (buffer.writePos + 1) % buffer.buffer.size();
                             }});

    // ---------------------------------
    // Main emulator loop

    saturn.Reset(true);

    // Configure single framebuffer
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
            if (pressed && (mod & SDL_KMOD_CTRL)) {
                saturn.Reset(true);
            }
            break;
        case SDL_SCANCODE_F3: //
        {
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
                std::ofstream out{"scsp-wram.bin", std::ios::binary};
                saturn.SCSP.DumpWRAM(out);
            }
            // TODO: dump SCU DSP program and data RAM
            // TODO: saturn.SCSP.DumpDSPProgram(std::ofstream{"scsp-dspprog.bin", std::ios::binary});
            // TODO: dump whatever else is needed for SCSP DSP analysis
            break;
        }
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
            auto title = fmt::format("{} fps", frames);
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

struct Sandbox {
    Sandbox(uint32 width, uint32 height)
        : framebuffer(width * height)
        , width(width)
        , height(height)
        // A = 32x38  B = 225x52  C = 431x254  D = 59x273
        // A = 260x272  B = 135x195  C = 240x129  D = 346x192
        // A = 181x241  B = 373x29  C = 95x37  D = 52x103
        , ax(181)
        , ay(241)
        , bx(373)
        , by(29)
        , cx(95)
        , cy(37)
        , dx(52)
        , dy(103)
        , lastTicks(SDL_GetTicks()) {
        keys.fill(false);
        prevKeys = keys;
    }

    void KeyDown(const SDL_Event &evt) {
        keys[evt.key.scancode] = true;
    }

    void KeyUp(const SDL_Event &evt) {
        keys[evt.key.scancode] = false;
    }

    void Frame() {
        using namespace satemu::vdp;

        const double dt = DeltaTime();
        const double speed = 100.0;

        double factor = 1.0;
        if (keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT]) {
            factor = 5.0;
        }

        const double inc = dt * speed * factor;

        if (keys[SDL_SCANCODE_Z] && !prevKeys[SDL_SCANCODE_Z]) {
            antialias = !antialias;
        }
        if (keys[SDL_SCANCODE_X] && !prevKeys[SDL_SCANCODE_X]) {
            edgesOnTop = !edgesOnTop;
        }
        if (keys[SDL_SCANCODE_1] && !prevKeys[SDL_SCANCODE_1]) {
            ax = 32;
            ay = 38;
            bx = 225;
            by = 52;
            cx = 431;
            cy = 254;
            dx = 59;
            dy = 273;
        }
        if (keys[SDL_SCANCODE_2] && !prevKeys[SDL_SCANCODE_2]) {
            ax = 260;
            ay = 218;
            bx = 135;
            by = 141;
            cx = 240;
            cy = 75;
            dx = 346;
            dy = 138;
        }
        if (keys[SDL_SCANCODE_3] && !prevKeys[SDL_SCANCODE_3]) {
            ax = 181;
            ay = 241;
            bx = 373;
            by = 29;
            cx = 95;
            cy = 37;
            dx = 52;
            dy = 103;
        }
        if (keys[SDL_SCANCODE_4] && !prevKeys[SDL_SCANCODE_4]) {
            ax = 200;
            ay = 100;
            bx = 300;
            by = 100;
            cx = 300;
            cy = 200;
            dx = 200;
            dy = 200;
        }

        if (keys[SDL_SCANCODE_5] && !prevKeys[SDL_SCANCODE_5]) {
            ax = 250;
            ay = 150;
            bx = 251;
            by = 150;
            cx = 251;
            cy = 151;
            dx = 250;
            dy = 151;
        }

        if (keys[SDL_SCANCODE_W]) {
            ay -= inc;
        }
        if (keys[SDL_SCANCODE_S]) {
            ay += inc;
        }
        if (keys[SDL_SCANCODE_A]) {
            ax -= inc;
        }
        if (keys[SDL_SCANCODE_D]) {
            ax += inc;
        }

        if (keys[SDL_SCANCODE_HOME]) {
            by -= inc;
        }
        if (keys[SDL_SCANCODE_END]) {
            by += inc;
        }
        if (keys[SDL_SCANCODE_DELETE]) {
            bx -= inc;
        }
        if (keys[SDL_SCANCODE_PAGEDOWN]) {
            bx += inc;
        }

        if (keys[SDL_SCANCODE_UP]) {
            cy -= inc;
        }
        if (keys[SDL_SCANCODE_DOWN]) {
            cy += inc;
        }
        if (keys[SDL_SCANCODE_LEFT]) {
            cx -= inc;
        }
        if (keys[SDL_SCANCODE_RIGHT]) {
            cx += inc;
        }

        if (keys[SDL_SCANCODE_I]) {
            dy -= inc;
        }
        if (keys[SDL_SCANCODE_K]) {
            dy += inc;
        }
        if (keys[SDL_SCANCODE_J]) {
            dx -= inc;
        }
        if (keys[SDL_SCANCODE_L]) {
            dx += inc;
        }

        if (keys[SDL_SCANCODE_T]) {
            ay -= inc;
            by -= inc;
            cy -= inc;
            dy -= inc;
        }
        if (keys[SDL_SCANCODE_G]) {
            ay += inc;
            by += inc;
            cy += inc;
            dy += inc;
        }
        if (keys[SDL_SCANCODE_F]) {
            ax -= inc;
            bx -= inc;
            cx -= inc;
            dx -= inc;
        }
        if (keys[SDL_SCANCODE_H]) {
            ax += inc;
            bx += inc;
            cx += inc;
            dx += inc;
        }

        if (keys[SDL_SCANCODE_SPACE] && !prevKeys[SDL_SCANCODE_SPACE]) {
            fmt::println("A = {}x{}  B = {}x{}  C = {}x{}  D = {}x{}", (int)ax, (int)ay, (int)bx, (int)by, (int)cx,
                         (int)cy, (int)dx, (int)dy);
        }

        prevKeys = keys;

        std::fill(framebuffer.begin(), framebuffer.end(), 0xFF000000);

        const CoordS32 coordA{(int)ax, (int)ay};
        const CoordS32 coordB{(int)bx, (int)by};
        const CoordS32 coordC{(int)cx, (int)cy};
        const CoordS32 coordD{(int)dx, (int)dy};

        if (!edgesOnTop) {
            for (LineStepper line{coordA, coordD}; line.CanStep(); line.Step()) {
                auto [x, y] = line.Coord();
                DrawPixel(x, y, 0x51b7c4);
            }

            for (LineStepper line{coordB, coordC}; line.CanStep(); line.Step()) {
                auto [x, y] = line.Coord();
                DrawPixel(x, y, 0xc45183);
            }

            DrawPixel(ax, ay, 0x4f52ff);
            DrawPixel(bx, by, 0x4fff98);
            DrawPixel(cx, cy, 0xffa74f);
            DrawPixel(dx, dy, 0xff4fb6);
        }

        // bool swapped;
        bool first = true;
        for (QuadEdgesStepper edge{coordA, coordB, coordC, coordD}; edge.CanStep(); edge.Step()) {
            const CoordS32 coordL{edge.LX(), edge.LY()};
            const CoordS32 coordR{edge.RX(), edge.RY()};

            bool firstPixel = true;
            for (LineStepper line{coordL, coordR}; line.CanStep(); line.Step()) {
                auto [x, y] = line.Coord();
                // const uint32 color = firstPixel ? 0xc7997c : first ? 0x96674a : 0x75492e;
                const uint32 color = ((line.FracPos() >> 8ll) & 0xFF) | (((edge.FracPos() >> 8ll) & 0xFF) << 8u) |
                                     (firstPixel * 0xFF0000) | (first * 0x7F0000);

                DrawPixel(x, y, color);
                if (antialias && line.NeedsAntiAliasing()) {
                    auto [aax, aay] = line.AACoord();
                    DrawPixel(aax, aay, color);
                }
                firstPixel = false;
            }
            if (first) {
                // swapped = edge.Swapped();
                first = false;
            }
        }

        if (edgesOnTop) {
            for (LineStepper line{coordA, coordD}; line.CanStep(); line.Step()) {
                auto [x, y] = line.Coord();
                DrawPixel(x, y, 0x51b7c4);
            }

            for (LineStepper line{coordB, coordC}; line.CanStep(); line.Step()) {
                auto [x, y] = line.Coord();
                DrawPixel(x, y, 0xc45183);
            }

            DrawPixel(ax, ay, 0x4f52ff);
            DrawPixel(bx, by, 0x4fff98);
            DrawPixel(cx, cy, 0xffa74f);
            DrawPixel(dx, dy, 0xff4fb6);
        }

        /*if (swapped) {
            DrawPixel((ax + bx + cx + dx) / 4, (ay + by + cy + dy) / 4, 0xFFFFFF);
        }*/

        lastTicks = SDL_GetTicks();
    }

    void DrawPixel(sint32 x, sint32 y, uint32 color) {
        if (x >= 0 && x < width && y >= 0 && y < height) {
            framebuffer[x + y * width] = color | 0xFF000000;
        }
    }

    double DeltaTime() const {
        return (SDL_GetTicks() - lastTicks) / 1000.0;
    }

    std::vector<uint32> framebuffer;
    uint32 width, height;
    double ax, ay;
    double bx, by;
    double cx, cy;
    double dx, dy;

    bool edgesOnTop = true;
    bool antialias = true;

    uint64 lastTicks;

    std::array<bool, SDL_SCANCODE_COUNT> keys;
    std::array<bool, SDL_SCANCODE_COUNT> prevKeys;
};

void runSandbox() {
    using clk = std::chrono::steady_clock;
    using namespace std::chrono_literals;

    // Screen parameters
    const uint32 screenWidth = 500;
    const uint32 screenHeight = 300;
    const uint32 scale = 3;

    // ---------------------------------
    // Initialize SDL video subsystem

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
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
    SDL_SetNumberProperty(windowProps, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, screenWidth * scale);
    SDL_SetNumberProperty(windowProps, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, screenHeight * scale);
    SDL_SetNumberProperty(windowProps, SDL_PROP_WINDOW_CREATE_X_NUMBER, SDL_WINDOWPOS_CENTERED);
    SDL_SetNumberProperty(windowProps, SDL_PROP_WINDOW_CREATE_Y_NUMBER, SDL_WINDOWPOS_CENTERED);

    auto window = SDL_CreateWindowWithProperties(windowProps);
    if (window == nullptr) {
        SDL_Log("Unable to create window: %s", SDL_GetError());
        return;
    }
    ScopeGuard sgDestroyWindow{[&] { SDL_DestroyWindow(window); }};

    // ---------------------------------
    // Create renderer

    SDL_PropertiesID rendererProps = SDL_CreateProperties();
    if (rendererProps == 0) {
        SDL_Log("Unable to create renderer properties: %s", SDL_GetError());
        return;
    }
    ScopeGuard sgDestroyRendererProps{[&] { SDL_DestroyProperties(rendererProps); }};

    // Assume the following calls succeed
    SDL_SetPointerProperty(rendererProps, SDL_PROP_RENDERER_CREATE_WINDOW_POINTER, window);
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

    auto texture =
        SDL_CreateTexture(renderer, SDL_PIXELFORMAT_XBGR8888, SDL_TEXTUREACCESS_STREAMING, screenWidth, screenHeight);
    if (texture == nullptr) {
        SDL_Log("Unable to create texture: %s", SDL_GetError());
        return;
    }
    ScopeGuard sgDestroyTexture{[&] { SDL_DestroyTexture(texture); }};

    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);

    // ---------------------------------
    // Main loop

    auto t = clk::now();
    uint64 frames = 0;
    bool running = true;

    Sandbox sandbox{screenWidth, screenHeight};

    while (running) {
        SDL_Event evt{};
        while (SDL_PollEvent(&evt)) {
            switch (evt.type) {
            case SDL_EVENT_KEY_DOWN: sandbox.KeyDown(evt); break;
            case SDL_EVENT_KEY_UP: sandbox.KeyUp(evt); break;
            case SDL_EVENT_QUIT: running = false; break;
            }
        }

        sandbox.Frame();

        ++frames;
        auto t2 = clk::now();
        if (t2 - t >= 1s) {
            auto title = fmt::format("{} fps", frames);
            SDL_SetWindowTitle(window, title.c_str());
            frames = 0;
            t = t2;
        }

        uint32 *pixels = nullptr;
        int pitch = 0;
        if (SDL_LockTexture(texture, nullptr, (void **)&pixels, &pitch)) {
            std::copy_n(sandbox.framebuffer.begin(), screenWidth * screenHeight, pixels);
            SDL_UnlockTexture(texture);
        }

        SDL_RenderClear(renderer);
        SDL_RenderTexture(renderer, texture, nullptr, nullptr);
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

    // runSandbox();

    return EXIT_SUCCESS;
}
