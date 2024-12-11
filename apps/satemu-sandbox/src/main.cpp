#include <satemu/satemu.hpp>

#include <SDL3/SDL.h>
#include <fmt/format.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <span>
#include <vector>

std::vector<uint8> loadFile(std::filesystem::path romPath) {
    fmt::print("Loading file {}... ", romPath.string());

    std::vector<uint8> data;
    std::ifstream stream{romPath, std::ios::binary | std::ios::ate};
    if (stream.is_open()) {
        auto size = stream.tellg();
        stream.seekg(0, std::ios::beg);
        fmt::println("{} bytes", (size_t)size);

        data.resize(size);
        stream.read(reinterpret_cast<char *>(data.data()), size);
    } else {
        fmt::println("failed!");
    }
    return data;
}

template <typename Fn>
struct ScopeGuard {
    ScopeGuard(const Fn &fn)
        : fn(fn) {}

    ScopeGuard(Fn &&fn)
        : fn(std::move(fn)) {}

    ~ScopeGuard() {
        if (active) {
            fn();
        }
    }

    void Cancel() {
        active = false;
    }

private:
    Fn fn;
    bool active = true;
};

void runEmulator(satemu::Saturn &saturn) {
    using clk = std::chrono::steady_clock;
    using namespace std::chrono_literals;

    // Screen parameters
    // TODO: adjust dynamically
    // NOTE: double-horizontal res should halve horizontal scale
    // NOTE: add room for borders
    const uint32 screenWidth = 320;
    const uint32 screenHeight = 224;
    const uint32 scale = 3;

    // ---------------------------------
    // Initialize SDL video subsystem

    if (!SDL_Init(SDL_INIT_VIDEO)) {
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
    // Main emulator loop

    saturn.Reset(true);

    // Configure single framebuffer
    std::vector<uint32> framebuffer(704 * 480);
    saturn.VDP2.SetCallbacks({framebuffer.data(), [](uint32, uint32, void *ctx) { return (uint32 *)ctx; }}, {});

    auto t = clk::now();
    uint64 frames = 0;
    bool running = true;
    while (running) {
        // TODO: saturn.RunFrame();

        // For the timings below, we introduce a counting factor in order to avoid rounding errors from the division.
        // At each step we increment the current cycle count by the factor, effectively enabling fractional cycle
        // counting with pure integer math.

        // NTSC timings at the default (fast) clock rate
        static constexpr uint64 kClockRate = 28'636'360;
        static constexpr uint64 kFramesPerSecond = 60;
        static constexpr uint64 kCycleCountingFactor = 3;
        static constexpr uint64 kCyclesPerFrame = kClockRate * kCycleCountingFactor / kFramesPerSecond;

        // PAL timings at the default (fast) clock rate)
        // static constexpr uint64 kClockRate = 28'437'500;
        // static constexpr uint64 kFramesPerSecond = 50;
        // static constexpr uint64 kCycleCountingFactor = 1;
        // static constexpr uint64 kCyclesPerFrame = kClockRate * kCycleCountingFactor / kFramesPerSecond;

        for (int i = 0; i < kCyclesPerFrame; i += kCycleCountingFactor) {
            saturn.Step();
        }

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
            std::copy_n(framebuffer.begin(), screenWidth * screenHeight, pixels);
            SDL_UnlockTexture(texture);
        }

        SDL_RenderClear(renderer);
        SDL_RenderTexture(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);

        auto evt = SDL_Event{};
        while (SDL_PollEvent(&evt)) {
            if (evt.type == SDL_EVENT_QUIT) {
                running = false;
                break;
            }
        }
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
        media::Disc disc{};
        if (!media::LoadDisc(argv[2], disc)) {
            fmt::println("Failed to load media from {}", argv[2]);
            return EXIT_FAILURE;
        }
        fmt::println("Loaded disc image from {}", argv[2]);
        saturn->LoadDisc(std::move(disc));
    }
    runEmulator(*saturn);

    return EXIT_SUCCESS;
}
