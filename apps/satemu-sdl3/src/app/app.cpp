#include "app.hpp"

#include <satemu/satemu.hpp>

#include <satemu/util/scope_guard.hpp>
#include <satemu/util/thread_name.hpp>

#include <util/rom_loader.hpp>

#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>

#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_sdlrenderer3.h>
#include <imgui.h>

#include <cmrc/cmrc.hpp>

#include <mutex>
#include <numbers>
#include <span>
#include <thread>
#include <vector>

CMRC_DECLARE(satemu_sdl3_rc);

namespace app {

App::App()
    : m_masterSH2Debugger(m_context, true)
    , m_slaveSH2Debugger(m_context, false)
    , m_scuDebugger(m_context) {}

int App::Run(const CommandLineOptions &options) {
    fmt::println("satemu {}", satemu::version::string);

    m_options = options;

    // ---------------------------------
    // Initialize SDL subsystems

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD | SDL_INIT_EVENTS)) {
        SDL_Log("Unable to initialize SDL: %s", SDL_GetError());
        return EXIT_FAILURE;
    }
    util::ScopeGuard sgQuit{[&] { SDL_Quit(); }};

    // Load IPL ROM
    {
        constexpr auto iplSize = satemu::sys::kIPLSize;
        auto rom = util::LoadFile(options.biosPath);
        if (rom.size() != iplSize) {
            fmt::println("IPL ROM size mismatch: expected {} bytes, got {} bytes", iplSize, rom.size());
            return EXIT_FAILURE;
        }
        m_context.saturn.LoadIPL(std::span<uint8, iplSize>(rom));
        fmt::println("IPL ROM loaded");
    }

    // Load disc image if provided
    if (!options.gameDiscPath.empty()) {
        if (!LoadDiscImage(options.gameDiscPath)) {
            return EXIT_FAILURE;
        }
    }

    RunEmulator();

    return EXIT_SUCCESS;
}

void App::RunEmulator() {
    util::SetCurrentThreadName("Main thread");

    using clk = std::chrono::steady_clock;
    using namespace std::chrono_literals;
    using namespace satemu;
    using namespace util;

    // Screen parameters
    const uint32 scale = 4;
    struct ScreenParams {
        ScreenParams()
            : framebuffer(vdp::kMaxResH * vdp::kMaxResV) {
            SetResolution(320, 224, scale);
        }

        SDL_Window *window = nullptr;

        uint32 width;
        uint32 height;
        float scaleX;
        float scaleY;
        float menuBarHeight = 0.0f;

        bool autoResizeWindow = true;

        void SetResolution(uint32 width, uint32 height, uint32 scale) {
            const bool doubleResH = width >= 640;
            const bool doubleResV = height >= 400;

            this->width = width;
            this->height = height;
            this->scaleX = doubleResH ? scale * 0.5f : scale;
            this->scaleY = doubleResV ? scale * 0.5f : scale;
        }

        void ResizeWindow() {
            SDL_RestoreWindow(window);
            SDL_SetWindowSize(window, width * scaleX, height * scaleY);
        }

        std::vector<uint32> framebuffer;
        std::mutex mtxFramebuffer;
        bool updated = false;
        bool reduceLatency = false; // false = more performance; true = update frames more often

        uint64 frames = 0;
        uint64 vdp1Frames = 0;
    } screen;

    // ---------------------------------
    // Determine ImGui menu bar height

    // Build temporary context
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        // Build atlas
        unsigned char *tex_pixels = nullptr;
        int tex_w, tex_h;
        ImGuiIO &io = ImGui::GetIO();
        io.Fonts->GetTexDataAsRGBA32(&tex_pixels, &tex_w, &tex_h);

        // Draw frame
        io.DisplaySize = {100, 100}; // Set a fake display size to satisfy ImGui
        ImGui::NewFrame();
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::BeginMainMenuBar();
        screen.menuBarHeight = ImGui::GetWindowHeight();
        ImGui::EndMainMenuBar();
        ImGui::PopStyleVar();
        ImGui::Render();

        ImGui::DestroyContext();
    }

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
    SDL_SetBooleanProperty(windowProps, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, true);
    SDL_SetNumberProperty(windowProps, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, screen.width * screen.scaleX);
    SDL_SetNumberProperty(windowProps, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER,
                          screen.height * screen.scaleY + screen.menuBarHeight);
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
    // SDL_SetNumberProperty(rendererProps, SDL_PROP_RENDERER_CREATE_PRESENT_VSYNC_NUMBER, SDL_RENDERER_VSYNC_DISABLED);
    // SDL_SetNumberProperty(rendererProps, SDL_PROP_RENDERER_CREATE_PRESENT_VSYNC_NUMBER, SDL_RENDERER_VSYNC_ADAPTIVE);
    SDL_SetNumberProperty(rendererProps, SDL_PROP_RENDERER_CREATE_PRESENT_VSYNC_NUMBER, 1);

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
    // Setup Dear ImGui context

    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     // Enable Docking

    // Setup Dear ImGui style
    ImGuiStyle &style = ImGui::GetStyle();
    style.WindowPadding = ImVec2(6, 6);
    style.FramePadding = ImVec2(4, 3);
    style.ItemSpacing = ImVec2(7, 4);
    style.ItemInnerSpacing = ImVec2(4, 4);
    style.TouchExtraPadding = ImVec2(0, 0);
    style.IndentSpacing = 21.0f;
    style.ScrollbarSize = 15.0f;
    style.GrabMinSize = 12.0f;
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.WindowRounding = 3.0f;
    style.ChildRounding = 0.0f;
    style.FrameRounding = 1.0f;
    style.PopupRounding = 1.0f;
    style.ScrollbarRounding = 1.0f;
    style.GrabRounding = 1.0f;
    style.TabBorderSize = 0.0f;
    style.TabBarBorderSize = 1.0f;
    style.TabBarOverlineSize = 2.0f;
    style.TabCloseButtonMinWidthSelected = -1.0f;
    style.TabCloseButtonMinWidthUnselected = 0.0f;
    style.TabRounding = 2.0f;
    style.CellPadding = ImVec2(3, 2);
    style.TableAngledHeadersAngle = -50.0f * (2.0f * std::numbers::pi / 360.0f);
    style.TableAngledHeadersTextAlign = ImVec2(0.50f, 0.00f);
    style.WindowTitleAlign = ImVec2(0.50f, 0.50f);
    style.WindowBorderHoverPadding = 5.0f;
    style.WindowMenuButtonPosition = ImGuiDir_Left;
    style.ColorButtonPosition = ImGuiDir_Right;
    style.ButtonTextAlign = ImVec2(0.50f, 0.50f);
    style.SelectableTextAlign = ImVec2(0.00f, 0.00f);
    style.SeparatorTextBorderSize = 2.0f;
    style.SeparatorTextPadding = ImVec2(21, 2);
    style.LogSliderDeadzone = 4.0f;
    style.ImageBorderSize = 0.0f;
    style.DockingSeparatorSize = 2.0f;
    style.DisplayWindowPadding = ImVec2(21, 21);
    style.DisplaySafeAreaPadding = ImVec2(3, 3);

    // Setup Dear ImGui colors
    ImVec4 *colors = ImGui::GetStyle().Colors;
    colors[ImGuiCol_Text] = ImVec4(0.91f, 0.92f, 0.94f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.38f, 0.39f, 0.41f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.05f, 0.06f, 0.08f, 0.95f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.14f, 0.18f, 0.26f, 0.18f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.07f, 0.06f, 0.09f, 0.94f);
    colors[ImGuiCol_Border] = ImVec4(0.60f, 0.65f, 0.77f, 0.31f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.10f, 0.22f, 0.51f, 0.66f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.25f, 0.36f, 0.62f, 0.80f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.63f, 0.71f, 0.92f, 0.84f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.10f, 0.13f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.23f, 0.36f, 0.72f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.10f, 0.11f, 0.13f, 0.59f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.05f, 0.06f, 0.09f, 0.95f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.04f, 0.05f, 0.05f, 0.69f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.29f, 0.31f, 0.35f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.36f, 0.39f, 0.45f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.46f, 0.52f, 0.64f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.20f, 0.42f, 0.94f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.43f, 0.57f, 0.91f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.74f, 0.82f, 1.00f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.26f, 0.46f, 0.98f, 0.40f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.46f, 0.98f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.51f, 0.64f, 0.99f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.26f, 0.46f, 0.98f, 0.40f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.46f, 0.98f, 0.80f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.26f, 0.48f, 0.98f, 1.00f);
    colors[ImGuiCol_Separator] = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.10f, 0.40f, 0.75f, 0.78f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.10f, 0.40f, 0.75f, 1.00f);
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.26f, 0.46f, 0.98f, 0.20f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.26f, 0.46f, 0.98f, 0.67f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.26f, 0.46f, 0.98f, 0.95f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.26f, 0.46f, 0.98f, 0.80f);
    colors[ImGuiCol_Tab] = ImVec4(0.18f, 0.29f, 0.58f, 0.86f);
    colors[ImGuiCol_TabSelected] = ImVec4(0.20f, 0.33f, 0.68f, 1.00f);
    colors[ImGuiCol_TabSelectedOverline] = ImVec4(0.26f, 0.46f, 0.98f, 1.00f);
    colors[ImGuiCol_TabDimmed] = ImVec4(0.07f, 0.09f, 0.15f, 0.97f);
    colors[ImGuiCol_TabDimmedSelected] = ImVec4(0.14f, 0.22f, 0.42f, 1.00f);
    colors[ImGuiCol_TabDimmedSelectedOverline] = ImVec4(0.50f, 0.50f, 0.50f, 0.00f);
    colors[ImGuiCol_DockingPreview] = ImVec4(0.26f, 0.46f, 0.98f, 0.70f);
    colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.53f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.67f, 0.25f, 1.00f);
    colors[ImGuiCol_TableHeaderBg] = ImVec4(0.19f, 0.19f, 0.20f, 1.00f);
    colors[ImGuiCol_TableBorderStrong] = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);
    colors[ImGuiCol_TableBorderLight] = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);
    colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
    colors[ImGuiCol_TextLink] = ImVec4(0.26f, 0.46f, 0.98f, 1.00f);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.43f, 0.59f, 0.98f, 0.43f);
    colors[ImGuiCol_DragDropTarget] = ImVec4(0.97f, 0.60f, 0.19f, 0.90f);
    colors[ImGuiCol_NavCursor] = ImVec4(0.26f, 0.46f, 0.98f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);

    // Setup Dear ImGui Platform/Renderer backends
    ImGui_ImplSDL3_InitForSDLRenderer(screen.window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use
    // ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return a nullptr. Please handle those errors in your
    // application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling
    // ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double
    // backslash \\ !
    // - Our Emscripten build process allows embedding fonts to be accessible at runtime from the "fonts/" folder. See
    // Makefile.emscripten for details.
    // io.Fonts->AddFontDefault();
    // io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf", 18.0f);
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    // ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, nullptr,
    // io.Fonts->GetGlyphRangesJapanese()); IM_ASSERT(font != nullptr);
    // io.Fonts->Build();
    {
        ImFontConfig config;
        config.FontDataOwnedByAtlas = false;

        ImVector<ImWchar> ranges;
        ImFontGlyphRangesBuilder builder;
        builder.AddRanges(io.Fonts->GetGlyphRangesDefault());
        // builder.AddRanges(io.Fonts->GetGlyphRangesChineseFull());
        // builder.AddRanges(io.Fonts->GetGlyphRangesCyrillic());
        // builder.AddRanges(io.Fonts->GetGlyphRangesGreek());
        // builder.AddRanges(io.Fonts->GetGlyphRangesJapanese());
        // builder.AddRanges(io.Fonts->GetGlyphRangesKorean());
        // builder.AddRanges(io.Fonts->GetGlyphRangesThai());
        // builder.AddRanges(io.Fonts->GetGlyphRangesVietnamese());
        builder.AddChar(0x2014); // Em-dash
        builder.AddChar(0x2190); // Left arrow
        builder.AddChar(0x2191); // Up arrow
        builder.AddChar(0x2192); // Right arrow
        builder.AddChar(0x2193); // Down arrow
        builder.BuildRanges(&ranges);

        auto embedfs = cmrc::satemu_sdl3_rc::get_filesystem();

        auto loadFont = [&](const char *path, float size) {
            cmrc::file file = embedfs.open(path);
            return io.Fonts->AddFontFromMemoryTTF((void *)file.begin(), file.size(), size, &config, ranges.Data);
        };

        m_context.fonts.sansSerifMedium = loadFont("fonts/SplineSans-Medium.ttf", 16);
        m_context.fonts.sansSerifBold = loadFont("fonts/SplineSans-Bold.ttf", 16);
        m_context.fonts.sansSerifMediumMedium = loadFont("fonts/SplineSans-Medium.ttf", 20);
        m_context.fonts.sansSerifMediumBold = loadFont("fonts/SplineSans-Bold.ttf", 20);
        m_context.fonts.sansSerifLargeBold = loadFont("fonts/SplineSans-Bold.ttf", 28);
        m_context.fonts.monospaceMedium = loadFont("fonts/SplineSansMono-Medium.ttf", 16);
        m_context.fonts.monospaceBold = loadFont("fonts/SplineSansMono-Bold.ttf", 16);
        m_context.fonts.monospaceMediumMedium = loadFont("fonts/SplineSansMono-Medium.ttf", 20);
        m_context.fonts.monospaceMediumBold = loadFont("fonts/SplineSansMono-Bold.ttf", 20);
        m_context.fonts.display = loadFont("fonts/ZenDots-Regular.ttf", 64);

        io.Fonts->Build();
    }

    // Our state
    bool showDemoWindow = false;
    ImVec4 clearColor = ImVec4(0.15f, 0.18f, 0.37f, 1.00f);

    // ---------------------------------
    // Setup framebuffer and render callbacks

    m_context.saturn.VDP.SetRenderCallback(
        {&screen, [](vdp::FramebufferColor *fb, uint32 width, uint32 height, void *ctx) {
             auto &screen = *static_cast<ScreenParams *>(ctx);
             if (width != screen.width || height != screen.height) {
                 const uint32 prevWidth = screen.width * screen.scaleX;
                 const uint32 prevHeight = screen.height * screen.scaleY;
                 screen.SetResolution(width, height, scale);

                 // TODO: this conflicts with window resizes from the user
                 // - add toggle: "Auto-fit window to screen"
                 if (screen.autoResizeWindow) {
                     int wx, wy;
                     SDL_GetWindowPosition(screen.window, &wx, &wy);
                     wy -= screen.menuBarHeight;
                     const int dx = (int)(width * screen.scaleX) - (int)prevWidth;
                     const int dy = (int)(height * screen.scaleY) - (int)prevHeight;

                     // Adjust window size dynamically
                     // TODO: add room for borders
                     SDL_SetWindowSize(screen.window, screen.width * screen.scaleX,
                                       screen.height * screen.scaleY + screen.menuBarHeight);
                     SDL_SetWindowPosition(screen.window, wx - dx / 2, wy - dy / 2 + screen.menuBarHeight);
                 }
             }
             ++screen.frames;

             // TODO: figure out frame pacing when sync to video is enabled
             if (screen.reduceLatency || !screen.updated) {
                 std::unique_lock lock{screen.mtxFramebuffer};
                 std::copy_n(fb, width * height, screen.framebuffer.data());
                 screen.updated = true;
             }
         }});

    m_context.saturn.VDP.SetVDP1Callback({&screen, [](void *ctx) {
                                              auto &screen = *static_cast<ScreenParams *>(ctx);
                                              ++screen.vdp1Frames;
                                          }});

    // ---------------------------------
    // Initialize audio system

    static constexpr int kSampleRate = 44100;
    static constexpr SDL_AudioFormat kSampleFormat = SDL_AUDIO_S16;
    static constexpr int kChannels = 2;
    static constexpr uint32 kBufferSize = 512; // TODO: make this configurable

    if (!m_audioSystem.Init(kSampleRate, kSampleFormat, kChannels, 512)) {
        SDL_Log("Unable to create audio stream: %s", SDL_GetError());
        return;
    }
    ScopeGuard sgDeinitAudio{[&] { m_audioSystem.Deinit(); }};

    // Set gain to a reasonable level
    m_audioSystem.SetGain(0.8f);

    if (m_audioSystem.Start()) {
        int sampleRate;
        SDL_AudioFormat audioFormat;
        int channels;
        if (!m_audioSystem.GetAudioStreamFormat(&sampleRate, &audioFormat, &channels)) {
            SDL_Log("Unable to get audio stream format: %s", SDL_GetError());
            return;
        }
        auto formatName = [&] {
            switch (audioFormat) {
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

        fmt::println("Audio stream opened: {} Hz, {} channel{}, {} format", sampleRate, channels,
                     (channels == 1 ? "" : "s"), formatName());
        if (sampleRate != kSampleRate || channels != kChannels || audioFormat != kSampleFormat) {
            // Hopefully this never happens
            fmt::println("Audio format mismatch");
            return;
        }
    } else {
        SDL_Log("Unable to start audio stream: %s", SDL_GetError());
    }

    m_context.saturn.SCSP.SetSampleCallback({&m_audioSystem, [](sint16 left, sint16 right, void *ctx) {
                                                 static_cast<AudioSystem *>(ctx)->ReceiveSample(left, right);
                                             }});

    // ---------------------------------
    // Main emulator loop

    m_fileDialogProps = SDL_CreateProperties();
    if (m_fileDialogProps == 0) {
        SDL_Log("Failed to create file dialog properties: %s\n", SDL_GetError());
        return;
    }
    ScopeGuard sgDestroyFileDialogProps{[&] { SDL_DestroyProperties(m_fileDialogProps); }};

    static constexpr SDL_DialogFileFilter kFileFilters[] = {
        {.name = "All supported formats", .pattern = "cue;mds;iso;ccd"}};

    SDL_SetPointerProperty(m_fileDialogProps, SDL_PROP_FILE_DIALOG_WINDOW_POINTER, screen.window);
    SDL_SetPointerProperty(m_fileDialogProps, SDL_PROP_FILE_DIALOG_FILTERS_POINTER, (void *)kFileFilters);
    SDL_SetNumberProperty(m_fileDialogProps, SDL_PROP_FILE_DIALOG_NFILTERS_NUMBER, (int)std::size(kFileFilters));
    SDL_SetBooleanProperty(m_fileDialogProps, SDL_PROP_FILE_DIALOG_MANY_BOOLEAN, false);
    SDL_SetStringProperty(m_fileDialogProps, SDL_PROP_FILE_DIALOG_TITLE_STRING, "Load Sega Saturn disc image");
    // const char *default_location = SDL_GetStringProperty(props, SDL_PROP_FILE_DIALOG_LOCATION_STRING, NULL);
    // const char *accept = SDL_GetStringProperty(props, SDL_PROP_FILE_DIALOG_ACCEPT_STRING, NULL);
    // const char *cancel = SDL_GetStringProperty(props, SDL_PROP_FILE_DIALOG_CANCEL_STRING, NULL);

    // TODO: pull from CommandLineOptions or configuration
    // m_context.saturn.SetVideoStandard(satemu::sys::VideoStandard::PAL);

    // TODO: pull from CommandLineOptions or configuration
    static constexpr std::string_view extBupPath = "bup-ext.bin";

    std::error_code error{};
    if (m_context.saturn.InsertCartridge<satemu::cart::BackupMemoryCartridge>(
            satemu::cart::BackupMemoryCartridge::Size::_32Mbit, extBupPath, error)) {
        fmt::println("External backup memory cartridge loaded from {}", extBupPath);
    } else if (error) {
        fmt::println("Failed to load external backup memory: {}", error.message());
    }

    /*if (m_context.saturn.InsertCartridge<satemu::cart::DRAM8MbitCartridge>()) {
        fmt::println("8 Mbit DRAM cartridge inserted");
    }*/

    /*if (m_context.saturn.InsertCartridge<satemu::cart::DRAM32MbitCartridge>()) {
        fmt::println("32 Mbit DRAM cartridge inserted");
    }*/

    m_context.saturn.Reset(true);

    auto t = clk::now();
    bool paused = false; // TODO: this should be updated by the emulator thread via events
    bool debugTrace = false;
    bool drawDebug = false;
    bool showVideoOutputDebugWindow = false;

    bool forceIntegerScaling = true;
    bool forceAspectRatio = false;
    float forcedAspect = 4.0f / 3.0f;

    auto &port1 = m_context.saturn.SMPC.GetPeripheralPort1();
    auto &port2 = m_context.saturn.SMPC.GetPeripheralPort2();
    auto *pad1 = port1.ConnectStandardPad();
    auto *pad2 = port2.ConnectStandardPad();

    auto setClearButton = [](peripheral::StandardPad &pad, peripheral::StandardPad::Button button, bool pressed) {
        if (pressed) {
            pad.PressButton(button);
        } else {
            pad.ReleaseButton(button);
        }
    };

    auto updateButton = [&](SDL_Scancode scancode, SDL_Keymod mod, bool pressed) {
        using enum peripheral::StandardPad::Button;
        switch (scancode) {
        case SDL_SCANCODE_W: setClearButton(*pad1, Up, pressed); break;
        case SDL_SCANCODE_A: setClearButton(*pad1, Left, pressed); break;
        case SDL_SCANCODE_S: setClearButton(*pad1, Down, pressed); break;
        case SDL_SCANCODE_D: setClearButton(*pad1, Right, pressed); break;
        case SDL_SCANCODE_Q: setClearButton(*pad1, L, pressed); break;
        case SDL_SCANCODE_E: setClearButton(*pad1, R, pressed); break;
        case SDL_SCANCODE_J: setClearButton(*pad1, A, pressed); break;
        case SDL_SCANCODE_K: setClearButton(*pad1, B, pressed); break;
        case SDL_SCANCODE_L: setClearButton(*pad1, C, pressed); break;
        case SDL_SCANCODE_U: setClearButton(*pad1, X, pressed); break;
        case SDL_SCANCODE_I: setClearButton(*pad1, Y, pressed); break;
        case SDL_SCANCODE_O:
            if ((mod & (SDL_KMOD_CTRL | SDL_KMOD_ALT | SDL_KMOD_SHIFT | SDL_KMOD_GUI)) == 0) {
                setClearButton(*pad1, Z, pressed);
            } else if (pressed && (mod & SDL_KMOD_CTRL)) {
                OpenLoadDiscDialog();
            }
            break;
        case SDL_SCANCODE_F: setClearButton(*pad1, Start, pressed); break;
        case SDL_SCANCODE_G: setClearButton(*pad1, Start, pressed); break;
        case SDL_SCANCODE_H: setClearButton(*pad1, Start, pressed); break;
        case SDL_SCANCODE_RETURN: setClearButton(*pad1, Start, pressed); break;

        case SDL_SCANCODE_UP: setClearButton(*pad2, Up, pressed); break;
        case SDL_SCANCODE_LEFT: setClearButton(*pad2, Left, pressed); break;
        case SDL_SCANCODE_DOWN: setClearButton(*pad2, Down, pressed); break;
        case SDL_SCANCODE_RIGHT: setClearButton(*pad2, Right, pressed); break;
        case SDL_SCANCODE_KP_7: setClearButton(*pad2, L, pressed); break;
        case SDL_SCANCODE_KP_9: setClearButton(*pad2, R, pressed); break;
        case SDL_SCANCODE_KP_1: setClearButton(*pad2, A, pressed); break;
        case SDL_SCANCODE_KP_2: setClearButton(*pad2, B, pressed); break;
        case SDL_SCANCODE_KP_3: setClearButton(*pad2, C, pressed); break;
        case SDL_SCANCODE_KP_4: setClearButton(*pad2, X, pressed); break;
        case SDL_SCANCODE_KP_5: setClearButton(*pad2, Y, pressed); break;
        case SDL_SCANCODE_KP_6: setClearButton(*pad2, Z, pressed); break;
        case SDL_SCANCODE_KP_ENTER: setClearButton(*pad2, Start, pressed); break;
        case SDL_SCANCODE_HOME: setClearButton(*pad2, Up, pressed); break;
        case SDL_SCANCODE_DELETE: setClearButton(*pad2, Left, pressed); break;
        case SDL_SCANCODE_END: setClearButton(*pad2, Down, pressed); break;
        case SDL_SCANCODE_PAGEDOWN: setClearButton(*pad2, Right, pressed); break;
        case SDL_SCANCODE_INSERT: setClearButton(*pad2, L, pressed); break;
        case SDL_SCANCODE_PAGEUP:
            setClearButton(*pad2, R, pressed);
            break;

            // ---- BEGIN TODO ----
            // TODO: find better keybindings for these
        case SDL_SCANCODE_F6:
            if (pressed) {
                m_emuEventQueue.enqueue(EmuEvent::OpenCloseTray());
            }
            break;
        case SDL_SCANCODE_F8:
            if (pressed) {
                m_emuEventQueue.enqueue(EmuEvent::EjectDisc());
            }
            break;
            // ---- END TODO ----

        case SDL_SCANCODE_EQUALS:
            if (pressed) {
                paused = true;
                m_audioSystem.SetSilent(false);
                m_emuEventQueue.enqueue(EmuEvent::FrameStep());
            }
            break;
        case SDL_SCANCODE_PAUSE:
            if (pressed) {
                paused = !paused;
                m_audioSystem.SetSilent(paused);
                m_emuEventQueue.enqueue(EmuEvent::SetPaused(paused));
            }

        case SDL_SCANCODE_R:
            if (pressed) {
                if ((mod & SDL_KMOD_CTRL) && (mod & SDL_KMOD_SHIFT)) {
                    m_emuEventQueue.enqueue(EmuEvent::FactoryReset());
                } else if (mod & SDL_KMOD_CTRL) {
                    m_emuEventQueue.enqueue(EmuEvent::HardReset());
                }
            }
            if (mod & SDL_KMOD_SHIFT) {
                m_emuEventQueue.enqueue(EmuEvent::SoftReset(pressed));
            }
            break;
        case SDL_SCANCODE_TAB: m_audioSystem.SetSync(!pressed); break;
        case SDL_SCANCODE_F3:
            if (pressed) {
                m_emuEventQueue.enqueue(EmuEvent::MemoryDump());
            }
            break;
        case SDL_SCANCODE_F9:
            if (pressed) {
                showVideoOutputDebugWindow = !showVideoOutputDebugWindow;
            }
            break;
        case SDL_SCANCODE_F10:
            if (pressed) {
                drawDebug = !drawDebug;
                screen.autoResizeWindow = !drawDebug;
                if (screen.autoResizeWindow) {
                    screen.ResizeWindow();
                }
                // SDL_SetWindowResizable(screen.window, drawDebug);
                fmt::println("Debug display {}", (drawDebug ? "enabled" : "disabled"));
            }
            break;
        case SDL_SCANCODE_F11:
            if (pressed) {
                debugTrace = !debugTrace;
                m_emuEventQueue.enqueue(EmuEvent::SetDebugTrace(debugTrace));
                fmt::println("Advanced debug tracing {}", (debugTrace ? "enabled" : "disabled"));
            }
            break;
        default: break;
        }
    };

    // Start emulator thread
    m_emuThread = std::thread([&] { EmulatorThread(); });
    ScopeGuard sgStopEmuThread{[&] {
        // TODO: fix this hacky mess
        // HACK: unpause and unsilence audio system in order to unlock the emulator thread if it is waiting for free
        // space in the audio buffer due to being paused
        paused = false;
        m_audioSystem.SetSilent(false);
        m_emuEventQueue.enqueue(EmuEvent::SetPaused(false));
        m_emuEventQueue.enqueue(EmuEvent::Shutdown());
        if (m_emuThread.joinable()) {
            m_emuThread.join();
        }
    }};

    while (true) {
        // Process SDL events
        SDL_Event evt{};
        while (SDL_PollEvent(&evt)) {
            ImGui_ImplSDL3_ProcessEvent(&evt);
            if (!io.WantCaptureKeyboard) {
                // TODO: clear out all key presses
            }

            switch (evt.type) {
            case SDL_EVENT_KEY_DOWN:
                if (!io.WantCaptureKeyboard) {
                    updateButton(evt.key.scancode, evt.key.mod, true);
                }
                break;
            case SDL_EVENT_KEY_UP:
                if (!io.WantCaptureKeyboard) {
                    updateButton(evt.key.scancode, evt.key.mod, false);
                }
                break;
            case SDL_EVENT_QUIT: goto end_loop; break;
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                if (evt.window.windowID == SDL_GetWindowID(screen.window)) {
                    goto end_loop;
                }
            }
        }

        // Update display
        // TODO: figure out frame pacing when sync to video is enabled
        if (screen.updated) {
            screen.updated = false;
            std::unique_lock lock{screen.mtxFramebuffer};
            uint32 *pixels = nullptr;
            int pitch = 0;
            SDL_Rect area{.x = 0, .y = 0, .w = (int)screen.width, .h = (int)screen.height};
            if (SDL_LockTexture(texture, &area, (void **)&pixels, &pitch)) {
                for (uint32 y = 0; y < screen.height; y++) {
                    std::copy_n(&screen.framebuffer[y * screen.width], screen.width,
                                &pixels[y * pitch / sizeof(uint32)]);
                }
                // std::copy_n(framebuffer.begin(), screen.width * screen.height, pixels);
                SDL_UnlockTexture(texture);
            }
        }

        // Calculate performance and update title bar
        auto t2 = clk::now();
        if (t2 - t >= 1s) {
            const media::Disc &disc = m_context.saturn.CDBlock.GetDisc();
            const media::SaturnHeader &header = disc.header;
            std::string title{};
            if (paused) {
                title =
                    fmt::format("[{}] {} - paused | GUI: {} fps", header.productNumber, header.gameTitle, io.Framerate);
            } else {
                title = fmt::format("[{}] {} | VDP2: {} fps | VDP1: {} fps | GUI: {} fps", header.productNumber,
                                    header.gameTitle, screen.frames, screen.vdp1Frames, io.Framerate);
            }
            SDL_SetWindowTitle(screen.window, title.c_str());
            screen.frames = 0;
            screen.vdp1Frames = 0;
            t = t2;
        }

        // ---------------------------------------------------------------------
        // Draw ImGui widgets

        // Start the Dear ImGui frame
        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        if (ImGui::BeginMainMenuBar()) {
            ImGui::PopStyleVar();
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Load disc image", "Ctrl+O")) {
                    OpenLoadDiscDialog();
                }
                if (ImGui::MenuItem("Open/close tray", "F6")) {
                    m_emuEventQueue.enqueue(EmuEvent::OpenCloseTray());
                }
                if (ImGui::MenuItem("Eject disc", "F8")) {
                    m_emuEventQueue.enqueue(EmuEvent::EjectDisc());
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Exit", "Alt+F4")) {
                    SDL_Event quitEvent{.type = SDL_EVENT_QUIT};
                    SDL_PushEvent(&quitEvent);
                }
                ImGui::End();
            }
            if (ImGui::BeginMenu("View")) {
                ImGui::MenuItem("Force integer scaling", nullptr, &forceIntegerScaling);
                ImGui::MenuItem("Force aspect ratio", nullptr, &forceAspectRatio);
                if (ImGui::SmallButton("4:3")) {
                    forcedAspect = 4.0f / 3.0f;
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("16:9")) {
                    forcedAspect = 16.0f / 9.0f;
                }
                ImGui::SameLine();
                ImGui::End();
            }
            if (ImGui::BeginMenu("Emulator")) {
                if (ImGui::MenuItem("Frame step", "=")) {
                    paused = true;
                    m_audioSystem.SetSilent(false);
                    m_emuEventQueue.enqueue(EmuEvent::FrameStep());
                }
                if (ImGui::MenuItem("Pause/resume", "Pause")) {
                    paused = !paused;
                    m_audioSystem.SetSilent(paused);
                    m_emuEventQueue.enqueue(EmuEvent::SetPaused(paused));
                }
                ImGui::Separator();
                /*if (ImGui::MenuItem("Soft reset", "Shift+R")) {
                    // TODO: send Soft Reset pulse for a short time
                    //m_emuEventQueue.enqueue(EmuEvent::SoftReset(pressed));
                }*/
                if (ImGui::MenuItem("Hard reset", "Ctrl+R")) {
                    m_emuEventQueue.enqueue(EmuEvent::HardReset());
                }
                if (ImGui::MenuItem("Factory reset", "Ctrl+Shift+R")) {
                    m_emuEventQueue.enqueue(EmuEvent::FactoryReset());
                }
                ImGui::End();
            }
            if (ImGui::BeginMenu("Settings")) {
                ImGui::TextUnformatted("(to be implemented)");
                ImGui::End();
            }
            if (ImGui::BeginMenu("Debug")) {
                ImGui::MenuItem("Enable debugger", "F10", &drawDebug);
                ImGui::MenuItem("Enable tracing", "F11", &debugTrace);
                ImGui::Separator();
                ImGui::MenuItem("Video output", "F9", &showVideoOutputDebugWindow);
                ImGui::Separator();
                if (ImGui::MenuItem("Dump all memory", "F3")) {
                    m_emuEventQueue.enqueue(EmuEvent::MemoryDump());
                }
                ImGui::End();
            }
            if (ImGui::BeginMenu("Help")) {
                ImGui::MenuItem("ImGui demo window", nullptr, &showDemoWindow);
                ImGui::End();
            }
            ImGui::EndMainMenuBar();
        } else {
            ImGui::PopStyleVar();
        }

        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

        // Show the big ImGui demo window if enabled
        if (showDemoWindow) {
            ImGui::ShowDemoWindow(&showDemoWindow);
        }

        // Draw debugger windows
        if (drawDebug) {
            // Draw video output as a window
            if (showVideoOutputDebugWindow) {
                std::string title = fmt::format("Video Output - {}x{}###Display", screen.width, screen.height);

                const float aspectRatio = (float)screen.height / screen.width;

                ImGui::SetNextWindowSizeConstraints(
                    ImVec2(320, 224), ImVec2(FLT_MAX, FLT_MAX),
                    [](ImGuiSizeCallbackData *data) {
                        float aspectRatio = *(float *)data->UserData;
                        data->DesiredSize.y =
                            (float)(int)(data->DesiredSize.x * aspectRatio) + ImGui::GetFrameHeightWithSpacing();
                    },
                    (void *)&aspectRatio);
                if (ImGui::Begin(title.c_str(), &showVideoOutputDebugWindow, ImGuiWindowFlags_NoNavInputs)) {
                    const ImVec2 avail = ImGui::GetContentRegionAvail();
                    const float scaleX = avail.x / screen.width;
                    const float scaleY = avail.y / screen.height;
                    const float scale = std::min(scaleX, scaleY);

                    ImGui::Image((ImTextureID)texture, ImVec2(screen.width * scale, screen.height * scale),
                                 ImVec2(0, 0),
                                 ImVec2((float)screen.width / vdp::kMaxResH, (float)screen.height / vdp::kMaxResV));
                }
                ImGui::End();
            }

            DrawDebug();
        }

        // ---------------------------------------------------------------------
        // Render window

        ImGui::Render();

        // Clear screen
        SDL_SetRenderDrawColorFloat(renderer, clearColor.x, clearColor.y, clearColor.z, clearColor.w);
        SDL_RenderClear(renderer);

        // Draw Saturn screen
        if (!drawDebug || !showVideoOutputDebugWindow) {
            // Get screen size
            const float baseWidth = forceAspectRatio ? screen.height * forcedAspect : screen.width;
            const float baseHeight = screen.height;

            // Get window size
            int ww, wh;
            SDL_GetWindowSize(screen.window, &ww, &wh);
            wh -= screen.menuBarHeight;

            // Compute maximum scale to fit the display given the constraints above
            const float scaleX = (float)ww / baseWidth;
            const float scaleY = (float)wh / baseHeight;
            float scale = std::min(scaleX, scaleY);
            if (forceIntegerScaling) {
                scale = floor(scale);
            }
            const float scaledWidth = baseWidth * scale;
            const float scaledHeight = baseHeight * scale;

            // Determine how much slack there is on each axis in order to center the image on the window
            const float slackX = ww - scaledWidth;
            const float slackY = wh - scaledHeight;

            // TODO: if not using integer scaling, render to a texture using nearest interpolation and scaled up to
            // ceil(current scale), then render that onto the window with linear interpolation, otherwise just render
            // the screen directly to the window with nearest interpolation
            SDL_FRect srcRect{.x = 0.0f, .y = 0.0f, .w = (float)screen.width, .h = (float)screen.height};
            SDL_FRect dstRect{
                .x = slackX * 0.5f, .y = slackY * 0.5f + screen.menuBarHeight, .w = scaledWidth, .h = scaledHeight};
            SDL_RenderTexture(renderer, texture, &srcRect, &dstRect);
        }

        // Render ImGui widgets
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);

        SDL_RenderPresent(renderer);
    }

end_loop:; // the semicolon is not a typo!

    // Everything is cleaned up automatically by ScopeGuards
}

void App::EmulatorThread() {
    util::SetCurrentThreadName("Emulator thread");

    std::array<EmuEvent, 64> cmds{};

    bool paused = false;
    bool frameStep = false;
    bool debugTrace = false;

    while (true) {
        // Process all pending commands
        const size_t cmdCount = paused ? m_emuEventQueue.wait_dequeue_bulk(cmds.begin(), cmds.size())
                                       : m_emuEventQueue.try_dequeue_bulk(cmds.begin(), cmds.size());
        for (size_t i = 0; i < cmdCount; i++) {
            const EmuEvent &cmd = cmds[i];
            using enum EmuEvent::Type;
            switch (cmd.type) {
            case FactoryReset: m_context.saturn.FactoryReset(); break;
            case HardReset: m_context.saturn.Reset(true); break;
            case SoftReset: m_context.saturn.SMPC.SetResetButtonState(std::get<bool>(cmd.value)); break;
            case FrameStep:
                frameStep = true;
                paused = false;
                break;
            case SetPaused: paused = std::get<bool>(cmd.value); break;
            case SetDebugTrace:
                debugTrace = std::get<bool>(cmd.value);
                if (debugTrace) {
                    m_context.saturn.masterSH2.UseTracer(&m_masterSH2Tracer);
                    m_context.saturn.slaveSH2.UseTracer(&m_slaveSH2Tracer);
                    m_context.saturn.SCU.UseTracer(&m_scuTracer);
                } else {
                    m_context.saturn.masterSH2.UseTracer(nullptr);
                    m_context.saturn.slaveSH2.UseTracer(nullptr);
                    m_context.saturn.SCU.UseTracer(nullptr);
                }
                break;
            case MemoryDump: //
            {
                {
                    std::ofstream out{"wram-lo.bin", std::ios::binary};
                    m_context.saturn.mem.DumpWRAMLow(out);
                }
                {
                    std::ofstream out{"wram-hi.bin", std::ios::binary};
                    m_context.saturn.mem.DumpWRAMHigh(out);
                }
                {
                    std::ofstream out{"vdp1-vram.bin", std::ios::binary};
                    m_context.saturn.VDP.DumpVDP1VRAM(out);
                }
                {
                    std::ofstream out{"vdp1-fbs.bin", std::ios::binary};
                    m_context.saturn.VDP.DumpVDP1Framebuffers(out);
                }
                {
                    std::ofstream out{"vdp2-vram.bin", std::ios::binary};
                    m_context.saturn.VDP.DumpVDP2VRAM(out);
                }
                {
                    std::ofstream out{"vdp2-cram.bin", std::ios::binary};
                    m_context.saturn.VDP.DumpVDP2CRAM(out);
                }
                {
                    std::ofstream out{"scu-dsp-prog.bin", std::ios::binary};
                    m_context.saturn.SCU.DumpDSPProgramRAM(out);
                }
                {
                    std::ofstream out{"scu-dsp-data.bin", std::ios::binary};
                    m_context.saturn.SCU.DumpDSPDataRAM(out);
                }
                {
                    std::ofstream out{"scu-dsp-regs.bin", std::ios::binary};
                    m_context.saturn.SCU.DumpDSPRegs(out);
                }
                {
                    std::ofstream out{"scsp-wram.bin", std::ios::binary};
                    m_context.saturn.SCSP.DumpWRAM(out);
                }
                {
                    std::ofstream out{"scsp-dsp-mpro.bin", std::ios::binary};
                    m_context.saturn.SCSP.DumpDSP_MPRO(out);
                }
                {
                    std::ofstream out{"scsp-dsp-temp.bin", std::ios::binary};
                    m_context.saturn.SCSP.DumpDSP_TEMP(out);
                }
                {
                    std::ofstream out{"scsp-dsp-mems.bin", std::ios::binary};
                    m_context.saturn.SCSP.DumpDSP_MEMS(out);
                }
                {
                    std::ofstream out{"scsp-dsp-coef.bin", std::ios::binary};
                    m_context.saturn.SCSP.DumpDSP_COEF(out);
                }
                {
                    std::ofstream out{"scsp-dsp-madrs.bin", std::ios::binary};
                    m_context.saturn.SCSP.DumpDSP_MADRS(out);
                }
                {
                    std::ofstream out{"scsp-dsp-mixs.bin", std::ios::binary};
                    m_context.saturn.SCSP.DumpDSP_MIXS(out);
                }
                {
                    std::ofstream out{"scsp-dsp-efreg.bin", std::ios::binary};
                    m_context.saturn.SCSP.DumpDSP_EFREG(out);
                }
                {
                    std::ofstream out{"scsp-dsp-exts.bin", std::ios::binary};
                    m_context.saturn.SCSP.DumpDSP_EXTS(out);
                }
                {
                    std::ofstream out{"scsp-dsp-regs.bin", std::ios::binary};
                    m_context.saturn.SCSP.DumpDSPRegs(out);
                }
                break;
            }
            case OpenCloseTray:
                if (m_context.saturn.IsTrayOpen()) {
                    m_context.saturn.CloseTray();
                } else {
                    m_context.saturn.OpenTray();
                }
                break;
            case LoadDisc: LoadDiscImage(std::get<std::string>(cmd.value)); break;
            case EjectDisc: m_context.saturn.EjectDisc(); break;
            case Shutdown: return;
            }
        }

        // Emulate one frame
        m_context.saturn.RunFrame(debugTrace);
        if (frameStep) {
            frameStep = false;
            paused = true;
            m_audioSystem.SetSilent(true);
        }
    }
}

void App::OpenLoadDiscDialog() {
    SDL_ShowFileDialogWithProperties(
        SDL_FILEDIALOG_OPENFILE,
        [](void *userdata, const char *const *filelist, int filter) {
            static_cast<App *>(userdata)->ProcessOpenDiscImageFileDialogSelection(filelist, filter);
        },
        this, m_fileDialogProps);
}

void App::ProcessOpenDiscImageFileDialogSelection(const char *const *filelist, int filter) {
    if (filelist == nullptr) {
        fmt::println("Failed to open file dialog: {}", SDL_GetError());
    } else if (*filelist == nullptr) {
        fmt::println("File dialog cancelled");
    } else {
        // Only one file should be selected
        const char *file = *filelist;
        m_emuEventQueue.enqueue(EmuEvent::LoadDisc(file));
    }
}

bool App::LoadDiscImage(std::filesystem::path path) {
    fmt::println("Loading disc image from {}", path.string());
    satemu::media::Disc disc{};
    if (!satemu::media::LoadDisc(path, disc)) {
        fmt::println("Failed to load disc image from {}", path.string());
        return false;
    }
    fmt::println("Loaded disc image from {}", path.string());
    m_context.saturn.LoadDisc(std::move(disc));
    return true;
}

void App::DrawDebug() {
    using namespace satemu;

    m_masterSH2Debugger.Display();
    m_slaveSH2Debugger.Display();
    m_scuDebugger.Display();

    /*int ww{};
    int wh{};
    SDL_GetWindowSize(screen.window, &ww, &wh);

    if (!debugTrace) {
        SDL_SetRenderDrawColor(renderer, 232, 117, 23, 255);
        drawText(5, wh - 5 - 8, "Advanced tracing disabled - some features are not available. Press F11 to enable");
    }

    SDL_SetRenderDrawColor(renderer, 23, 148, 232, 255);
    std::string res = fmt::format("{}x{}", screen.width, screen.height);
    drawText(ww - 5 - res.size() * 8, 5, res);*/
}

} // namespace app
