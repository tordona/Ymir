#include <satemu/util/scope_guard.hpp>

#include "../../../libs/satemu-core/src/satemu/hw/vdp/slope.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>
#include <fmt/format.h>

#include <chrono>
#include <filesystem>
#include <vector>

using namespace util;

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
        if (keys[SDL_SCANCODE_C] && !prevKeys[SDL_SCANCODE_C]) {
            uvGradient = !uvGradient;
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
                uint32 color;
                if (uvGradient) {
                    color = ((line.FracPos() >> 8ll) & 0xFF) | (((edge.FracPos() >> 8ll) & 0xFF) << 8u) |
                            (firstPixel * 0xFF0000) | (first * 0x7F0000);
                } else {
                    color = firstPixel ? 0xc7997c : first ? 0x96674a : 0x75492e;
                }

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
    bool uvGradient = false;

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
    SDL_SetStringProperty(windowProps, SDL_PROP_WINDOW_CREATE_TITLE_STRING, "Sandbox");
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
    runSandbox();

    return EXIT_SUCCESS;
}
