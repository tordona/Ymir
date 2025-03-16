#include "app.hpp"

#include <satemu/satemu.hpp>

#include <satemu/util/event.hpp>
#include <satemu/util/scope_guard.hpp>
#include <satemu/util/thread_name.hpp>

#include <util/rom_loader.hpp>

#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>

#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_sdlrenderer3.h>
#include <imgui.h>

#include <atomic>
#include <span>
#include <thread>
#include <vector>

namespace app {

int App::Run(const CommandLineOptions &options) {
    fmt::println("satemu {}", satemu::version::string);

    m_options = options;

    // Load IPL ROM
    {
        constexpr auto iplSize = satemu::sys::kIPLSize;
        auto rom = util::LoadFile(options.biosPath);
        if (rom.size() != iplSize) {
            fmt::println("IPL ROM size mismatch: expected {} bytes, got {} bytes", iplSize, rom.size());
            return EXIT_FAILURE;
        }
        m_saturn.LoadIPL(std::span<uint8, iplSize>(rom));
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
        uint32 width = 320;
        uint32 height = 224;
        float scaleX = scale;
        float scaleY = scale;
        SDL_Window *window = nullptr;
        uint64 frames = 0;
        uint64 vdp1Frames = 0;
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
    // SDL_SetNumberProperty(rendererProps, SDL_PROP_RENDERER_CREATE_PRESENT_VSYNC_NUMBER,
    // SDL_RENDERER_VSYNC_ADAPTIVE); SDL_SetNumberProperty(rendererProps,
    // SDL_PROP_RENDERER_CREATE_PRESENT_VSYNC_NUMBER, 1);

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
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     // Enable Docking

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    // ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
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

    // Our state
    bool showDemoWindow = true;
    ImVec4 clearColor = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // ---------------------------------
    // Setup framebuffer and render callbacks

    std::vector<uint32> framebuffer(vdp::kMaxResH * vdp::kMaxResV);
    m_saturn.VDP.SetRenderCallbacks(
        {framebuffer.data(), [](uint32, uint32, void *ctx) { return (uint32 *)ctx; }},
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
             ++screen.frames;
         }});

    m_saturn.VDP.SetVDP1Callbacks({&screen, [](void *ctx) {
                                       auto &screen = *static_cast<ScreenParams *>(ctx);
                                       ++screen.vdp1Frames;
                                   }});

    // ---------------------------------
    // Create audio buffer and stream and set up callbacks

    // Use a smaller buffer to reduce audio latency
    SDL_SetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, "512");

    struct Sample {
        sint16 left, right;
    };

    struct AudioState {
        std::array<Sample, 2048> buffer{};
        std::atomic_uint32_t readPos = 0;
        uint32 writePos = 0;
        util::Event bufferNotFullEvent{true};
        bool sync = true;
        bool silent = false;
    } audioState{};

    SDL_AudioSpec audioSpec{};
    audioSpec.freq = 44100;
    audioSpec.format = SDL_AUDIO_S16;
    audioSpec.channels = 2;
    auto audioStream = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &audioSpec,
        [](void *userdata, SDL_AudioStream *stream, int additional_amount, int total_amount) {
            auto &state = *reinterpret_cast<AudioState *>(userdata);

            int sampleCount = additional_amount / sizeof(Sample);
            if (state.silent) {
                const sint16 zero = 0;
                for (int i = 0; i < sampleCount; i++) {
                    SDL_PutAudioStreamData(stream, &zero, sizeof(zero));
                }
            } else {
                const uint32 readPos = state.readPos.load(std::memory_order_acquire);
                int len1 = std::min<int>(sampleCount, state.buffer.size() - readPos);
                int len2 = std::min<int>(sampleCount - len1, readPos);
                SDL_PutAudioStreamData(stream, &state.buffer[readPos], len1 * sizeof(Sample));
                SDL_PutAudioStreamData(stream, &state.buffer[0], len2 * sizeof(Sample));

                state.readPos.store((state.readPos + len1 + len2) % state.buffer.size(), std::memory_order_release);
                state.bufferNotFullEvent.Set();
            }
        },
        &audioState);
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

    m_saturn.SCSP.SetSampleCallback({&audioState, [](sint16 left, sint16 right, void *ctx) {
                                         auto &state = *reinterpret_cast<AudioState *>(ctx);

                                         if (state.sync) {
                                             state.bufferNotFullEvent.Wait(false);
                                         }
                                         state.buffer[state.writePos] = {left, right};
                                         state.writePos = (state.writePos + 1) % state.buffer.size();
                                         if (state.writePos == state.readPos) {
                                             state.bufferNotFullEvent.Reset();
                                         }
                                     }});

    // ---------------------------------
    // Main emulator loop

    SDL_PropertiesID fileDialogProps = SDL_CreateProperties();
    if (fileDialogProps == 0) {
        SDL_Log("Failed to create file dialog properties: %s\n", SDL_GetError());
        return;
    }
    ScopeGuard sgDestroyFileDialogProps{[&] { SDL_DestroyProperties(fileDialogProps); }};

    static constexpr SDL_DialogFileFilter kFileFilters[] = {
        {.name = "All supported formats", .pattern = "cue;mds;iso;ccd"}};

    SDL_SetPointerProperty(fileDialogProps, SDL_PROP_FILE_DIALOG_WINDOW_POINTER, screen.window);
    SDL_SetPointerProperty(fileDialogProps, SDL_PROP_FILE_DIALOG_FILTERS_POINTER, (void *)kFileFilters);
    SDL_SetNumberProperty(fileDialogProps, SDL_PROP_FILE_DIALOG_NFILTERS_NUMBER, (int)std::size(kFileFilters));
    SDL_SetBooleanProperty(fileDialogProps, SDL_PROP_FILE_DIALOG_MANY_BOOLEAN, false);
    SDL_SetStringProperty(fileDialogProps, SDL_PROP_FILE_DIALOG_TITLE_STRING, "Load Sega Saturn disc image");
    // const char *default_location = SDL_GetStringProperty(props, SDL_PROP_FILE_DIALOG_LOCATION_STRING, NULL);
    // const char *accept = SDL_GetStringProperty(props, SDL_PROP_FILE_DIALOG_ACCEPT_STRING, NULL);
    // const char *cancel = SDL_GetStringProperty(props, SDL_PROP_FILE_DIALOG_CANCEL_STRING, NULL);

    // TODO: pull from CommandLineOptions or configuration
    // m_saturn.SetVideoStandard(satemu::sys::VideoStandard::PAL);

    // TODO: pull from CommandLineOptions or configuration
    static constexpr std::string_view extBupPath = "bup-ext.bin";

    std::error_code error{};
    if (m_saturn.InsertCartridge<satemu::cart::BackupMemoryCartridge>(
            satemu::cart::BackupMemoryCartridge::Size::_32Mbit, extBupPath, error)) {
        fmt::println("External backup memory cartridge loaded from {}", extBupPath);
    } else if (error) {
        fmt::println("Failed to load external backup memory: {}", error.message());
    }

    /*if (m_saturn.InsertCartridge<satemu::cart::DRAM8MbitCartridge>()) {
        fmt::println("8 Mbit DRAM cartridge inserted");
    }*/

    /*if (m_saturn.InsertCartridge<satemu::cart::DRAM32MbitCartridge>()) {
        fmt::println("32 Mbit DRAM cartridge inserted");
    }*/

    m_saturn.Reset(true);

    auto t = clk::now();
    bool paused = false;
    bool frameStep = false;
    bool debugTrace = false;
    bool drawDebug = false;
    auto &port1 = m_saturn.SMPC.GetPeripheralPort1();
    auto &port2 = m_saturn.SMPC.GetPeripheralPort2();
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
            if (mod == SDL_KMOD_NONE) {
                setClearButton(*pad1, Z, pressed);
            } else if (pressed && (mod & SDL_KMOD_CTRL)) {
                SDL_ShowFileDialogWithProperties(
                    SDL_FILEDIALOG_OPENFILE,
                    [](void *userdata, const char *const *filelist, int filter) {
                        static_cast<App *>(userdata)->OpenDiscImageFileDialog(filelist, filter);
                    },
                    this, fileDialogProps);
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
                if (m_saturn.IsTrayOpen()) {
                    m_saturn.CloseTray();
                } else {
                    m_saturn.OpenTray();
                }
            }
            break;
        case SDL_SCANCODE_F8:
            if (pressed) {
                m_saturn.EjectDisc();
            }
            break;
            // ---- END TODO ----

        case SDL_SCANCODE_EQUALS:
            if (pressed) {
                frameStep = true;
                paused = false;
                audioState.silent = false;
            }
            break;
        case SDL_SCANCODE_PAUSE:
            if (pressed) {
                paused = !paused;
                audioState.silent = paused;
            }

        case SDL_SCANCODE_R:
            if (pressed) {
                if ((mod & SDL_KMOD_CTRL) && (mod & SDL_KMOD_SHIFT)) {
                    m_saturn.FactoryReset();
                } else if (mod & SDL_KMOD_CTRL) {
                    m_saturn.Reset(true);
                }
            }
            if (mod & SDL_KMOD_SHIFT) {
                m_saturn.SMPC.SetResetButtonState(pressed);
            }
            break;
        case SDL_SCANCODE_TAB: audioState.sync = !pressed; break;
        case SDL_SCANCODE_F3:
            if (pressed) {
                {
                    std::ofstream out{"wram-lo.bin", std::ios::binary};
                    m_saturn.mem.DumpWRAMLow(out);
                }
                {
                    std::ofstream out{"wram-hi.bin", std::ios::binary};
                    m_saturn.mem.DumpWRAMHigh(out);
                }
                {
                    std::ofstream out{"vdp1-vram.bin", std::ios::binary};
                    m_saturn.VDP.DumpVDP1VRAM(out);
                }
                {
                    std::ofstream out{"vdp1-fbs.bin", std::ios::binary};
                    m_saturn.VDP.DumpVDP1Framebuffers(out);
                }
                {
                    std::ofstream out{"vdp2-vram.bin", std::ios::binary};
                    m_saturn.VDP.DumpVDP2VRAM(out);
                }
                {
                    std::ofstream out{"vdp2-cram.bin", std::ios::binary};
                    m_saturn.VDP.DumpVDP2CRAM(out);
                }
                {
                    std::ofstream out{"scu-dsp-prog.bin", std::ios::binary};
                    m_saturn.SCU.DumpDSPProgramRAM(out);
                }
                {
                    std::ofstream out{"scu-dsp-data.bin", std::ios::binary};
                    m_saturn.SCU.DumpDSPDataRAM(out);
                }
                {
                    std::ofstream out{"scu-dsp-regs.bin", std::ios::binary};
                    m_saturn.SCU.DumpDSPRegs(out);
                }
                {
                    std::ofstream out{"scsp-wram.bin", std::ios::binary};
                    m_saturn.SCSP.DumpWRAM(out);
                }
                {
                    std::ofstream out{"scsp-dsp-mpro.bin", std::ios::binary};
                    m_saturn.SCSP.DumpDSP_MPRO(out);
                }
                {
                    std::ofstream out{"scsp-dsp-temp.bin", std::ios::binary};
                    m_saturn.SCSP.DumpDSP_TEMP(out);
                }
                {
                    std::ofstream out{"scsp-dsp-mems.bin", std::ios::binary};
                    m_saturn.SCSP.DumpDSP_MEMS(out);
                }
                {
                    std::ofstream out{"scsp-dsp-coef.bin", std::ios::binary};
                    m_saturn.SCSP.DumpDSP_COEF(out);
                }
                {
                    std::ofstream out{"scsp-dsp-madrs.bin", std::ios::binary};
                    m_saturn.SCSP.DumpDSP_MADRS(out);
                }
                {
                    std::ofstream out{"scsp-dsp-mixs.bin", std::ios::binary};
                    m_saturn.SCSP.DumpDSP_MIXS(out);
                }
                {
                    std::ofstream out{"scsp-dsp-efreg.bin", std::ios::binary};
                    m_saturn.SCSP.DumpDSP_EFREG(out);
                }
                {
                    std::ofstream out{"scsp-dsp-exts.bin", std::ios::binary};
                    m_saturn.SCSP.DumpDSP_EXTS(out);
                }
                {
                    std::ofstream out{"scsp-dsp-regs.bin", std::ios::binary};
                    m_saturn.SCSP.DumpDSPRegs(out);
                }
            }
            break;
        case SDL_SCANCODE_F10:
            if (pressed) {
                drawDebug = !drawDebug;
                fmt::println("Debug display {}", (drawDebug ? "enabled" : "disabled"));
            }
            break;
        case SDL_SCANCODE_F11:
            if (pressed) {
                debugTrace = !debugTrace;
                if (debugTrace) {
                    m_saturn.masterSH2.UseTracer(&m_masterSH2Tracer);
                    m_saturn.slaveSH2.UseTracer(&m_slaveSH2Tracer);
                    m_saturn.SCU.UseTracer(&m_scuTracer);
                } else {
                    m_saturn.masterSH2.UseTracer(nullptr);
                    m_saturn.slaveSH2.UseTracer(nullptr);
                    m_saturn.SCU.UseTracer(nullptr);
                }
                fmt::println("Advanced debug tracing {}", (debugTrace ? "enabled" : "disabled"));
            }
            break;
        default: break;
        }
    };

    while (true) {
        // ---------------------------------------------------------------------
        // Process events

        SDL_Event evt{};
        while (paused ? SDL_WaitEvent(&evt) : SDL_PollEvent(&evt)) {
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

        // ---------------------------------------------------------------------
        // Emulate one frame

        m_saturn.RunFrame(debugTrace);
        if (frameStep) {
            frameStep = false;
            paused = true;
            audioState.silent = true;
        }

        // ---------------------------------------------------------------------
        // Calculate performance and update title bar

        auto t2 = clk::now();
        if (t2 - t >= 1s) {
            const media::Disc &disc = m_saturn.CDBlock.GetDisc();
            const media::SaturnHeader &header = disc.header;
            std::string title{};
            if (paused) {
                title = fmt::format("[{}] {} - paused", header.productNumber, header.gameTitle);
            } else {
                title = fmt::format("[{}] {} - VDP2: {} fps - VDP1: {} fps", header.productNumber, header.gameTitle,
                                    screen.frames, screen.vdp1Frames);
            }
            SDL_SetWindowTitle(screen.window, title.c_str());
            screen.frames = 0;
            screen.vdp1Frames = 0;
            t = t2;
        }

        // ---------------------------------------------------------------------
        // Update display

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

        // ---------------------------------------------------------------------
        // Draw ImGui widgets

        // Start the Dear ImGui frame
        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code
        // to learn more about Dear ImGui!).
        if (showDemoWindow) {
            ImGui::ShowDemoWindow(&showDemoWindow);
        }

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
        {
            static float f = 0.0f;
            static int counter = 0;

            ImGui::Begin("Hello, world!"); // Create a window called "Hello, world!" and append into it.

            ImGui::Text("This is some useful text.");        // Display some text (you can use a format strings too)
            ImGui::Checkbox("Demo Window", &showDemoWindow); // Edit bools storing our window open/close state

            ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
            ImGui::ColorEdit3("clear color", (float *)&clearColor); // Edit 3 floats representing a color

            if (ImGui::Button(
                    "Button")) // Buttons return true when clicked (most widgets return true when edited/activated)
                counter++;
            ImGui::SameLine();
            ImGui::Text("counter = %d", counter);

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
            ImGui::End();
        }

        // ---------------------------------------------------------------------
        // Render window

        ImGui::Render();

        // Clear screen
        SDL_SetRenderDrawColorFloat(renderer, clearColor.x, clearColor.y, clearColor.z, clearColor.w);
        SDL_RenderClear(renderer);

        // Render Saturn display covering the entire window
        SDL_FRect srcRect{.x = 0.0f, .y = 0.0f, .w = (float)screen.width, .h = (float)screen.height};
        SDL_RenderTexture(renderer, texture, &srcRect, nullptr);

        // Render ImGui widgets
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);

        // (OBSOLETE; TO BE REMOVED) Render debug text
        SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);

        auto drawText = [&](int x, int y, std::string text) {
            uint8 r, g, b, a;
            SDL_GetRenderDrawColor(renderer, &r, &g, &b, &a);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            // FIXME: this is a horribly inefficient way to draw contours!
            for (int yy = -2; yy <= 2; yy++) {
                for (int xx = -2; xx <= 2; xx++) {
                    if (xx == 0 && yy == 0) {
                        continue;
                    }
                    SDL_RenderDebugText(renderer, x + xx, y + yy, text.c_str());
                }
            }
            SDL_SetRenderDrawColor(renderer, r, g, b, a);
            SDL_RenderDebugText(renderer, x, y, text.c_str());
        };

        if (drawDebug) {
            std::string str{};

            auto bit = [](bool value, std::string_view bit) { return value ? fmt::format("{}", bit) : "."; };

            auto displaySH2 = [&](SH2Tracer &tracer, sh2::SH2 &sh2, bool master, bool enabled, int x, int y) {
                auto &regs = sh2.GetGPRs();
                drawText(x, y, fmt::format("{}SH2", master ? "M" : "S"));
                if (enabled) {
                    for (uint32 i = 0; i < 16; i++) {
                        drawText(x, y + 15 + i * 10, fmt::format("{:08X}", regs[i]));
                    }

                    drawText(x, y + 180, fmt::format("{:08X}", sh2.GetPC()));
                    drawText(x, y + 190, fmt::format("{:08X}", sh2.GetPR()));

                    auto mac = sh2.GetMAC();
                    drawText(x, y + 205, fmt::format("{:08X}", mac.H));
                    drawText(x, y + 215, fmt::format("{:08X}", mac.L));

                    auto sr = sh2.GetSR();
                    drawText(x, y + 230, fmt::format("{:08X}", sr.u32));
                    drawText(x, y + 240,
                             fmt::format("{}{}{}{} I={:X}", bit(sr.M, "M"), bit(sr.Q, "Q"), bit(sr.S, "S"),
                                         bit(sr.T, "T"), (uint8)sr.ILevel));

                    drawText(x, y + 255, fmt::format("{:08X}", sh2.GetGBR()));
                    drawText(x, y + 265, fmt::format("{:08X}", sh2.GetVBR()));

                    if (debugTrace) {
                        drawText(x, y + 280, "vec lv");
                        for (size_t i = 0; i < tracer.interruptsCount; i++) {
                            const size_t pos =
                                (tracer.interruptsPos - tracer.interruptsCount + i) % tracer.interrupts.size();
                            const auto &intr = tracer.interrupts[pos];
                            drawText(x, y + 290 + i * 10, fmt::format("{:02X}  {:02X}", intr.vecNum, intr.level));
                        }
                    }
                } else {
                    drawText(x, y + 15, "(disabled)");
                }
            };

            auto displaySH2s = [&](int x, int y) {
                for (uint32 i = 0; i < 16; i++) {
                    drawText(x, y + 15 + i * 10, fmt::format("R{}", i));
                }

                drawText(x, y + 180, "PC");
                drawText(x, y + 190, "PR");

                drawText(x, y + 205, "MACH");
                drawText(x, y + 215, "MACL");

                drawText(x, y + 230, "SR");
                drawText(x, y + 240, "flags");

                drawText(x, y + 255, "GBR");
                drawText(x, y + 265, "VBR");

                drawText(x, y + 280, "INTs");

                if (!debugTrace) {
                    drawText(x, y + 290, "(trace unavailable)");
                }

                displaySH2(m_masterSH2Tracer, m_saturn.masterSH2, true, true, x + 50, y);
                displaySH2(m_slaveSH2Tracer, m_saturn.slaveSH2, false, m_saturn.slaveSH2Enabled, x + 150, y);
            };

            auto displaySCU = [&](int x, int y) {
                auto &scu = m_saturn.SCU;

                drawText(x, y, "SCU");

                drawText(x, y + 15, "Interrupts");
                drawText(x, y + 25, fmt::format("{:08X} mask", scu.GetInterruptMask().u32));
                drawText(x, y + 35, fmt::format("{:08X} status", scu.GetInterruptStatus().u32));

                if (debugTrace) {
                    auto &tracer = m_scuTracer;
                    for (size_t i = 0; i < tracer.interruptsCount; i++) {
                        size_t pos = (tracer.interruptsPos - tracer.interruptsCount + i) % tracer.interrupts.size();
                        constexpr const char *kNames[] = {"VBlank IN",
                                                          "VBlank OUT",
                                                          "HBlank IN",
                                                          "Timer 0",
                                                          "Timer 1",
                                                          "DSP End",
                                                          "Sound Request",
                                                          "System Manager",
                                                          "PAD Interrupt",
                                                          "Level 2 DMA End",
                                                          "Level 1 DMA End",
                                                          "Level 0 DMA End",
                                                          "DMA-illegal",
                                                          "Sprite Draw End",
                                                          "(0E)",
                                                          "(0F)",
                                                          "External 0",
                                                          "External 1",
                                                          "External 2",
                                                          "External 3",
                                                          "External 4",
                                                          "External 5",
                                                          "External 6",
                                                          "External 7",
                                                          "External 8",
                                                          "External 9",
                                                          "External A",
                                                          "External B",
                                                          "External C",
                                                          "External D",
                                                          "External E",
                                                          "External F"};

                        const auto &intr = tracer.interrupts[pos];
                        if (intr.level == 0xFF) {
                            drawText(x, y + 50 + i * 10, fmt::format("{:15s}  ack", kNames[intr.index], intr.level));
                        } else {
                            drawText(x, y + 50 + i * 10, fmt::format("{:15s}  {:02X}", kNames[intr.index], intr.level));
                        }
                    }
                } else {
                    drawText(x, y + 50, "(trace unavailable)");
                }
            };

            displaySH2s(5, 5);
            displaySCU(250, 5);

            int ww{};
            int wh{};
            SDL_GetWindowSize(screen.window, &ww, &wh);

            if (!debugTrace) {
                SDL_SetRenderDrawColor(renderer, 232, 117, 23, 255);
                drawText(5, wh - 5 - 8,
                         "Advanced tracing disabled - some features are not available. Press F11 to enable");
            }

            SDL_SetRenderDrawColor(renderer, 23, 148, 232, 255);
            std::string res = fmt::format("{}x{}", screen.width, screen.height);
            drawText(ww - 5 - res.size() * 8, 5, res);
        }

        SDL_RenderPresent(renderer);
    }

end_loop:; // the semicolon is not a typo!

    // Everything is cleaned up automatically by ScopeGuards
}

void App::OpenDiscImageFileDialog(const char *const *filelist, int filter) {
    if (filelist == nullptr) {
        fmt::println("Failed to open file dialog: {}", SDL_GetError());
    } else if (*filelist == nullptr) {
        fmt::println("File dialog cancelled");
    } else {
        // Only one file should be selected
        const char *file = *filelist;
        LoadDiscImage(file);
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
    m_saturn.LoadDisc(std::move(disc));
    return true;
}

void App::SH2Tracer::Interrupt(uint8 vecNum, uint8 level, uint32 pc) {
    interrupts[interruptsPos++] = {vecNum, level, pc};
    if (interruptsPos >= interrupts.size()) {
        interruptsPos = 0;
    }
    if (interruptsCount < interrupts.size()) {
        interruptsCount++;
    }
}

void App::SH2Tracer::Exception(uint8 vecNum, uint32 pc, uint32 sr) {
    exceptions[exceptionsPos++] = {vecNum, pc, sr};
    if (exceptionsPos >= exceptions.size()) {
        exceptionsPos = 0;
    }
    if (exceptionsCount < exceptions.size()) {
        exceptionsCount++;
    }
}

void App::SCUTracer::RaiseInterrupt(uint8 index, uint8 level) {
    PushInterrupt({index, level});
}

void App::SCUTracer::AcknowledgeInterrupt(uint8 index) {
    PushInterrupt({index, 0xFF});
}

void App::SCUTracer::PushInterrupt(InterruptInfo info) {
    interrupts[interruptsPos++] = info;
    if (interruptsPos >= interrupts.size()) {
        interruptsPos = 0;
    }
    if (interruptsCount < interrupts.size()) {
        interruptsCount++;
    }
}

} // namespace app
