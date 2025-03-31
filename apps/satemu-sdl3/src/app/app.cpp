// satemu SDL3 frontend
//
// Foreword
// --------
// I find frontend development to be extremely tedious and unrewarding for the most part. Whenever I start working on
// it, my desire to code vanishes. I'd rather spend two weeks troubleshooting a stupid off-by-one bug in the emulator
// core, decompiling SH2 assembly and comparing gigabytes of execution traces against other emulators than write yet
// another goddamn window for a single hour.
//
// This abomination here is the result of my half-assed attempt to provide a usable frontend for the emulator.
// If you wish to rewrite it from scratch, be my guest. Use whatever tech you want, come up with whatever design you
// wish, or just fix this mess and send a PR.
//
// Just make sure it's awesome, and follow the instructions below on how to use the emulator core.
//
// - StrikerX3
//
// ---------------------------------------------------------------------------------------------------------------------
//
// General usage
// -------------
// satemu::Saturn emulates the entire system. You can make as many instances of it as you want; they're all completely
// independent. (Yay for not using global state!)
//
// Use the methods and members on that instance to control the emulator. The Saturn's components can be accessed
// directly through the instance as well.
//
// The constructor automatically hard resets the emulator with Reset(true). This is cheaper than constructing the object
// from scratch. You can also soft reset with Reset(false) or by changing the Reset button state through the SMPC.
//
// In order to run the emulator, set up a loop that processes application events and invokes RunFrame(false) to run the
// emulator for a single frame. The "false" arguments disables debug tracing, which increases performance at the cost of
// some debugging features, explained later in the Debugging section.
//
// The emulator core makes no attempt to pace execution to realtime speed - it's up to the frontend to implement some
// pacing method. If no such method is used, it will run as fast as your CPU allows.
//
// This frontend implements a simple audio sync that locks up the emulator thread while the audio ring buffer is full.
// Fast-forward simply disables audio sync, which allows the core to run as fast as possible as the audio callback
// overruns the audio buffer. The buffer size requested from the audio device is slightly smaller than 1/60 of the
// sample rate which results in video stuttering but no frame skipping.
//
//
// Receiving input
// ---------------
// To process inputs, you'll need to attach a controller to one or both ports. You'll find the ports in the SMPC.
//
// Use one of the Connect* methods to attempt to attach a controller to the port. If successful, the method will return
// a valid pointer to the specialized controller instance which you can use to send inputs to the system. A nullptr
// indicates failure to instantiate the object or to attach the peripheral due to incompatibility with existing
// peripherals.
//
// DisconnectPeripherals() will disconnect all peripherals connected to the port. Be careful: any existing pointers to
// previously connected peripheral(s) will become invalid. The same applies when replacing a peripheral.
//
// NOTE: There is currently no way to enumerate peripherals attached to a port.
// NOTE: The emulator currently only supports attaching a single standard Saturn Pad to the ports. Due to this, there's
// also no way to detach a specific peripheral. More types of peripherals (including multitap) are planned.
//
// This frontend attaches a standard Saturn Pad to both ports and redirects keyboard input to them with the following
// hardcoded key mappings:
//
//          Port 1                        Port 2
//     Q              E         KP7/Ins            KP9/PgUp
//     W                           Up
//   A   D  F/G/H   U I O        Lf  Rt  KPEnter  KP4 KP5 KP6
//     S    /Enter  J K L          Dn             KP1 KP2 KP3
//                            (arrow keys)
//                            (or Home/Del/End/PgDn)
//
// Saturn Standard Pad Layout
//     L                 R
//     Up
// Left  Right  Start  X Y Z
//    Down             A B C
//
//
// Receiving video frames and audio samples
// ----------------------------------------
// In order to receive video and audio, you must configure callbacks in VDP and SCSP.
//
// The VDP invokes the frame completed callback once a frame finishes rendering (as soon as it enters the VBlank area).
// The callback signature is:
//   void FrameCompleteCallback(uint32 *fb, uint32 width, uint32 height)
// where:
//   fb is a pointer to the rendered framebuffer in little-endian XBGR8888 format (..BBGGRR)
//   width and height specify the dimensions of the framebuffer
// NOTE: The most significant byte is set to 0xFF for convenience, so that it is fully opaque in case your framebuffer
// texture has an alpha channel (ABGR8888 format).
//
// Additionally, you can specify a VDP1 frame completed callback in order to count VDP1 frames. This callback has the
// following signature:
//   void VDP1FrameCompleteCallback()
//
// The SCSP invokes the sample callback on every sample (signed 16-bit PCM, stereo, 44100 Hz).
// The callback signature is:
//   void SCSPSampleCallback(sint16 left, sint16 right)
// where left and right are the samples for the respective channels.
// You'll probably want to accumulate those samples into a ring buffer before sending them to the audio system.
//
// You can run the emulator core without providing video and audio callbacks (headless mode). It will work fine, but you
// won't receive video frames or audio samples.
//
// All callbacks are invoked from inside the emulator core deep within the RunFrame() call stack, so if you're running
// it on a dedicated thread (as is done here) you need to make sure to sync/mutex updates coming from the callbacks into
// the GUI/main thread.
//
//
// Debugging
// ---------
// WARNING: The regs is a work in progress and in a flow state. Expect things to change dramatically.
//
// You can use Bus objects to directly read or write memory. Also, the regs framework provides two major components:
// the probes and the tracers.
//
// Bus instances provide Peek/Poke variants of Read/Write methods that circumvent memory access limitations, allowing
// debuggers to read from write-only registers or do 8-bit reads and writes to VDP registers which normally disallow
// accesses of that size. Peek and Poke also avoid side-effects when accessing certain registers such as the CD Block's
// data transfer register which would cause the transfer pointer to advance and break emulated software.
//
// Probes are provided by components to inspect or modify their internal state. They are always available and have
// virtually no performance cost on the emulator thread. Probes can perform operations that cannot normally be done
// through simple memory reads and writes such as directly reading from or writing to SH2 cache arrays or CD Block
// buffers. Not even Peek/Poke on the Bus can reach that far.
//
// Tracers are integrated into the components themselves in order to capture events as the emulator executes. The
// application must implement the provided interfaces in satemu/debug/*_tracer.hpp, then attach tracer instances to the
// components with the UseTracer(...) methods provided by them which will then receive events as they occur while the
// emulator is running.
//
// Some tracers require you to run the emulator in "debug mode", which is accomplished by invoking RunFrame(true)
// instead of RunFrame(false). There's no need to reset or reinitialize the emulator core to switch modes -- you can run
// the emulator normally for a while, then switch to debug mode at any point to enable tracing, and switch back and
// forth as often as you want. Tracers that need debug mode to work are documented as such in their header files.
//
// Running in debug mode has a noticeable performance penalty as the alternative code path enables calls to the tracers
// in hot paths. This is left as an option in order to maximize performance for the primary use case of playing games
// without using any debugging features.
//
// Some components always have tracing enabled if provided a valid instance, so in order avoid the performance penalty,
// make sure to also detach tracers from all components when you don't need them by calling DetachAllTracers() on the
// satemu::Saturn instance. Currently, only the SH2 tracer honors the debug mode flag.
//
// Debug mode is not necessary to use probes as they have no performace impact.
//
// Tracers are invoked from the emulator thread -- you will need to manage thread safety if trace data is to be consumed
// by another thread. It's also important to minimize performance impact, especially on hot tracers (memory accesses and
// CPU instructions primarily). A good approach to optimize time spent handling the event is to copy the trace data into
// a lock-free ring buffer to be processed further by another thread.
//
// WARNING: Since the emulator is not thread-safe, care must be taken when using buses, probes and tracers while the
// emulator is running in a multithreaded context:
// - Reads will retrieve dirty data but are otherwise safe.
// - Certain writes (especially to nontrivial registers or internal state) will cause race conditions and potentially
//   crash the emulator.
//
// This frontend enqueues regs writes to be executed on the emulator thread when it is convenient.
//
//
// Thread safety
// -------------
// The emulator core is *not* thread-safe and *will never be*. Make sure to provide your own synchronization mechanisms
// if you plan to run it in a dedicated thread.
//
// As noted above, the video and audio callbacks and debug tracers are invoked from the emulator thread. Provide proper
// synchronization between the emulator thread and the main/GUI thread when handling these events.
//
// The VDP renderer runs in its own thread and is thread-safe within the core.
//
// This frontend runs the emulator core in a dedicated thread while the GUI runs on the main thread. Synchronization
// between threads is accomplished by using a blocking concurrent queue to send events to the emulator thread, which
// processes the events between frames. The regs performs dirty reads and enqueues writes to be executed in the
// emulator thread. Video and audio callbacks use minimal synchronization.

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
    : m_systemStatusWindow(m_context)
    , m_masterSH2WindowSet(m_context, true)
    , m_slaveSH2WindowSet(m_context, false)
    , m_scuWindowSet(m_context)
    , m_debugOutputWindow(m_context)
    , m_aboutWindow(m_context) {

    // Preinitialize some memory viewers
    for (int i = 0; i < 8; i++) {
        m_memoryViewerWindows.emplace_back(m_context);
    }
}

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
    struct ScreenParams {
        ScreenParams()
            : framebuffer(vdp::kMaxResH * vdp::kMaxResV) {
            SetResolution(320, 224);
            prevWidth = width;
            prevHeight = height;
            prevScaleX = scaleX;
            prevScaleY = scaleY;
        }

        SDL_Window *window = nullptr;

        uint32 width;
        uint32 height;
        uint32 scaleX;
        uint32 scaleY;
        uint32 fbScale = 1;

        bool resolutionChanged = false;
        uint32 prevWidth;
        uint32 prevHeight;
        uint32 prevScaleX;
        uint32 prevScaleY;

        bool autoResizeWindow = true;
        bool displayVideoOutputInWindow = false;

        void SetResolution(uint32 width, uint32 height) {
            const bool doubleResH = width >= 640;
            const bool doubleResV = height >= 400;

            this->prevWidth = this->width;
            this->prevHeight = this->height;
            this->prevScaleX = this->scaleX;
            this->prevScaleY = this->scaleY;

            this->width = width;
            this->height = height;
            this->scaleX = doubleResV && !doubleResH ? 2 : 1;
            this->scaleY = doubleResH && !doubleResV ? 2 : 1;
            this->resolutionChanged = true;
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

    bool forceIntegerScaling = true;
    bool forceAspectRatio = false;
    double forcedAspect = 4.0 / 3.0;

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
    style.TableAngledHeadersAngle = 50.0f * (2.0f * std::numbers::pi / 360.0f);
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
        // TODO: config.MergeMode = true; to merge multiple fonts into one; useful for combining latin + JP + icons

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

        m_context.fonts.sansSerif.small.regular = loadFont("fonts/SplineSans-Medium.ttf", 14);
        m_context.fonts.sansSerif.small.bold = loadFont("fonts/SplineSans-Bold.ttf", 14);
        m_context.fonts.sansSerif.medium.regular = loadFont("fonts/SplineSans-Medium.ttf", 16);
        m_context.fonts.sansSerif.medium.bold = loadFont("fonts/SplineSans-Bold.ttf", 16);
        m_context.fonts.sansSerif.large.regular = loadFont("fonts/SplineSans-Medium.ttf", 20);
        m_context.fonts.sansSerif.large.bold = loadFont("fonts/SplineSans-Bold.ttf", 20);
        m_context.fonts.sansSerif.xlarge.regular = loadFont("fonts/SplineSans-Medium.ttf", 28);
        m_context.fonts.sansSerif.xlarge.bold = loadFont("fonts/SplineSans-Bold.ttf", 28);

        m_context.fonts.monospace.small.regular = loadFont("fonts/SplineSansMono-Medium.ttf", 14);
        m_context.fonts.monospace.small.bold = loadFont("fonts/SplineSansMono-Bold.ttf", 14);
        m_context.fonts.monospace.medium.regular = loadFont("fonts/SplineSansMono-Medium.ttf", 16);
        m_context.fonts.monospace.medium.bold = loadFont("fonts/SplineSansMono-Bold.ttf", 16);
        m_context.fonts.monospace.large.regular = loadFont("fonts/SplineSansMono-Medium.ttf", 20);
        m_context.fonts.monospace.large.bold = loadFont("fonts/SplineSansMono-Bold.ttf", 20);
        m_context.fonts.monospace.xlarge.regular = loadFont("fonts/SplineSansMono-Medium.ttf", 28);
        m_context.fonts.monospace.xlarge.bold = loadFont("fonts/SplineSansMono-Bold.ttf", 28);

        m_context.fonts.display.small = loadFont("fonts/ZenDots-Regular.ttf", 24);
        m_context.fonts.display.large = loadFont("fonts/ZenDots-Regular.ttf", 64);

        io.Fonts->Build();

        io.FontDefault = m_context.fonts.sansSerif.medium.regular;
    }

    // ---------------------------------
    // Create window

    SDL_PropertiesID windowProps = SDL_CreateProperties();
    if (windowProps == 0) {
        SDL_Log("Unable to create window properties: %s", SDL_GetError());
        return;
    }
    ScopeGuard sgDestroyWindowProps{[&] { SDL_DestroyProperties(windowProps); }};

    {
        // Equivalent to ImGui::GetFrameHeight() without requiring a window
        const float menuBarHeight = io.FontDefault->FontSize + style.FramePadding.y * 2.0f;

        // Assume the following calls succeed
        SDL_SetStringProperty(windowProps, SDL_PROP_WINDOW_CREATE_TITLE_STRING, "Unnamed Sega Saturn emulator");
        SDL_SetBooleanProperty(windowProps, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, true);
        SDL_SetNumberProperty(windowProps, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, screen.width * screen.scaleX * 4);
        SDL_SetNumberProperty(windowProps, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER,
                              screen.height * screen.scaleY * 4 + menuBarHeight);
        SDL_SetNumberProperty(windowProps, SDL_PROP_WINDOW_CREATE_X_NUMBER, SDL_WINDOWPOS_CENTERED);
        SDL_SetNumberProperty(windowProps, SDL_PROP_WINDOW_CREATE_Y_NUMBER, SDL_WINDOWPOS_CENTERED);
        SDL_SetNumberProperty(windowProps, SDL_PROP_WINDOW_CREATE_HIDDEN_BOOLEAN, true);
    }

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
    // Create textures to render on

    // We use two textures to render the Saturn display:
    // - The framebuffer texture containing the Saturn framebuffer, updated on every frame
    // - The display texture, rendered to the screen
    // The scaling technique used here is a combination of nearest and linear interpolations to make the uninterpolated
    // pixels look great at any scale. It consists of rendering the framebuffer texture into the display texture using
    // nearest interpolation with an integer scale, then rendering the display texture onto the screen with linear
    // interpolation.

    // Framebuffer texture
    auto fbTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_XBGR8888, SDL_TEXTUREACCESS_STREAMING, vdp::kMaxResH,
                                       vdp::kMaxResV);
    if (fbTexture == nullptr) {
        SDL_Log("Unable to create texture: %s", SDL_GetError());
        return;
    }
    ScopeGuard sgDestroyFbTexture{[&] { SDL_DestroyTexture(fbTexture); }};
    SDL_SetTextureScaleMode(fbTexture, SDL_SCALEMODE_NEAREST);

    // Display texture, containing the scaled framebuffer to be displayed on the screen
    auto dispTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_XBGR8888, SDL_TEXTUREACCESS_TARGET,
                                         vdp::kMaxResH * screen.fbScale, vdp::kMaxResV * screen.fbScale);
    if (dispTexture == nullptr) {
        SDL_Log("Unable to create texture: %s", SDL_GetError());
        return;
    }
    ScopeGuard sgDestroyDispTexture{[&] { SDL_DestroyTexture(dispTexture); }};
    SDL_SetTextureScaleMode(dispTexture, SDL_SCALEMODE_LINEAR);

    auto renderDispTexture = [&](double targetWidth, double targetHeight) {
        const double dispWidth = (forceAspectRatio ? screen.height * forcedAspect : screen.width) / screen.scaleY;
        const double dispHeight = (double)screen.height / screen.scaleX;
        const double dispScaleX = (double)targetWidth / dispWidth;
        const double dispScaleY = (double)targetHeight / dispHeight;
        const double dispScale = std::min(dispScaleX, dispScaleY);
        const uint32 scale = std::max(1.0, ceil(dispScale));

        // Recreate render target texture if scale changed
        if (scale != screen.fbScale) {
            screen.fbScale = scale;
            SDL_DestroyTexture(dispTexture);
            dispTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_XBGR8888, SDL_TEXTUREACCESS_TARGET,
                                            vdp::kMaxResH * screen.fbScale, vdp::kMaxResV * screen.fbScale);
        }

        // Remember previous render target to be restored later
        SDL_Texture *prevRenderTarget = SDL_GetRenderTarget(renderer);

        // Render scaled framebuffer into display texture
        SDL_FRect srcRect{.x = 0.0f, .y = 0.0f, .w = (float)screen.width, .h = (float)screen.height};
        SDL_FRect dstRect{.x = 0.0f,
                          .y = 0.0f,
                          .w = (float)screen.width * screen.fbScale,
                          .h = (float)screen.height * screen.fbScale};
        SDL_SetRenderTarget(renderer, dispTexture);
        SDL_RenderTexture(renderer, fbTexture, &srcRect, &dstRect);

        // Restore render target
        SDL_SetRenderTarget(renderer, prevRenderTarget);
    };

    // ---------------------------------
    // Setup Dear ImGui Platform/Renderer backends

    ImGui_ImplSDL3_InitForSDLRenderer(screen.window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    // Our state
    bool showDemoWindow = false;
    ImVec4 clearColor = ImVec4(0.15f, 0.18f, 0.37f, 1.00f);

    // ---------------------------------
    // Setup framebuffer and render callbacks

    m_context.saturn.VDP.SetRenderCallback({&screen, [](uint32 *fb, uint32 width, uint32 height, void *ctx) {
                                                auto &screen = *static_cast<ScreenParams *>(ctx);
                                                if (width != screen.width || height != screen.height) {
                                                    screen.SetResolution(width, height);
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

    if (!m_audioSystem.Init(kSampleRate, kSampleFormat, kChannels, kBufferSize)) {
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

    bool softResetPending = false;
    auto softResetTime = clk::now();
    auto softResetDuration = 1ms;

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
                m_context.eventQueues.emulator.enqueue(EmuEvent::OpenCloseTray());
            }
            break;
        case SDL_SCANCODE_F8:
            if (pressed) {
                m_context.eventQueues.emulator.enqueue(EmuEvent::EjectDisc());
            }
            break;
            // ---- END TODO ----

        case SDL_SCANCODE_EQUALS:
            if (pressed) {
                paused = true;
                m_context.eventQueues.emulator.enqueue(EmuEvent::FrameStep());
            }
            break;
        case SDL_SCANCODE_PAUSE:
            if (pressed) {
                paused = !paused;
                m_context.eventQueues.emulator.enqueue(EmuEvent::SetPaused(paused));
            }

        case SDL_SCANCODE_R:
            if (pressed) {
                if ((mod & SDL_KMOD_CTRL) && (mod & SDL_KMOD_SHIFT)) {
                    m_context.eventQueues.emulator.enqueue(EmuEvent::FactoryReset());
                } else if (mod & SDL_KMOD_CTRL) {
                    m_context.eventQueues.emulator.enqueue(EmuEvent::HardReset());
                }
            }
            if (mod & SDL_KMOD_SHIFT) {
                m_context.eventQueues.emulator.enqueue(EmuEvent::SoftReset(pressed));
            }
            break;
        case SDL_SCANCODE_TAB: m_audioSystem.SetSync(!pressed); break;
        case SDL_SCANCODE_F3:
            if (pressed) {
                m_context.eventQueues.emulator.enqueue(EmuEvent::MemoryDump());
            }
            break;
        case SDL_SCANCODE_F9:
            if (pressed) {
                screen.displayVideoOutputInWindow = !screen.displayVideoOutputInWindow;
            }
            break;
        case SDL_SCANCODE_F11:
            if (pressed) {
                m_context.eventQueues.emulator.enqueue(
                    EmuEvent::SetDebugTrace(!m_context.saturn.IsDebugTracingEnabled()));
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
        m_context.eventQueues.emulator.enqueue(EmuEvent::SetPaused(false));
        m_context.eventQueues.emulator.enqueue(EmuEvent::Shutdown());
        if (m_emuThread.joinable()) {
            m_emuThread.join();
        }
    }};

    SDL_ShowWindow(screen.window);

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
            if (SDL_LockTexture(fbTexture, &area, (void **)&pixels, &pitch)) {
                for (uint32 y = 0; y < screen.height; y++) {
                    std::copy_n(&screen.framebuffer[y * screen.width], screen.width,
                                &pixels[y * pitch / sizeof(uint32)]);
                }
                // std::copy_n(framebuffer.begin(), screen.width * screen.height, pixels);
                SDL_UnlockTexture(fbTexture);
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

        // Update soft reset signal if pending.
        // NOTE: This conflicts with the Shift+R keybinding, but the end result is still the same - the system resets.
        if (softResetPending && t2 > softResetTime) {
            softResetPending = false;
            m_context.eventQueues.emulator.enqueue(EmuEvent::SoftReset(false));
        }

        bool fitWindowToScreenNow = false;
        const bool prevForceAspectRatio = forceAspectRatio;
        const double prevForcedAspect = forcedAspect;

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
                    m_context.eventQueues.emulator.enqueue(EmuEvent::OpenCloseTray());
                }
                if (ImGui::MenuItem("Eject disc", "F8")) {
                    m_context.eventQueues.emulator.enqueue(EmuEvent::EjectDisc());
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
                    forcedAspect = 4.0 / 3.0;
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("16:9")) {
                    forcedAspect = 16.0 / 9.0;
                }
                ImGui::Separator();
                ImGui::MenuItem("Auto-fit window to screen", nullptr, &screen.autoResizeWindow);
                if (ImGui::MenuItem("Fit window to screen", nullptr, nullptr, !screen.displayVideoOutputInWindow)) {
                    fitWindowToScreenNow = true;
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Windowed video output", "F9", &screen.displayVideoOutputInWindow)) {
                    fitWindowToScreenNow = true;
                }
                ImGui::End();
            }
            if (ImGui::BeginMenu("Emulator")) {
                if (ImGui::MenuItem("Frame step", "=")) {
                    paused = true;
                    m_context.eventQueues.emulator.enqueue(EmuEvent::FrameStep());
                }
                if (ImGui::MenuItem("Pause/resume", "Pause")) {
                    paused = !paused;
                    m_context.eventQueues.emulator.enqueue(EmuEvent::SetPaused(paused));
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Soft reset", "Shift+R")) {
                    // Send Soft Reset pulse for a short time
                    m_context.eventQueues.emulator.enqueue(EmuEvent::SoftReset(true));
                    softResetPending = true;
                    softResetTime = clk::now() + softResetDuration;
                }
                if (ImGui::MenuItem("Hard reset", "Ctrl+R")) {
                    m_context.eventQueues.emulator.enqueue(EmuEvent::HardReset());
                }
                if (ImGui::MenuItem("Factory reset", "Ctrl+Shift+R")) {
                    m_context.eventQueues.emulator.enqueue(EmuEvent::FactoryReset());
                }
                ImGui::Separator();
                ImGui::MenuItem("System status", nullptr, &m_systemStatusWindow.Open);
                ImGui::End();
            }
            if (ImGui::BeginMenu("Settings")) {
                ImGui::TextUnformatted("(to be implemented)");
                ImGui::Separator();
                bool emulateSH2Cache = m_context.saturn.IsSH2CacheEmulationEnabled();
                if (ImGui::MenuItem("SH2 cache emulation", nullptr, &emulateSH2Cache)) {
                    m_context.eventQueues.emulator.enqueue(EmuEvent::SetEmulateSH2Cache(emulateSH2Cache));
                }
                ImGui::End();
            }
            if (ImGui::BeginMenu("Debug")) {
                bool debugTrace = m_context.saturn.IsDebugTracingEnabled();
                if (ImGui::MenuItem("Enable tracing", "F11", &debugTrace)) {
                    m_context.eventQueues.emulator.enqueue(EmuEvent::SetDebugTrace(debugTrace));
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Open memory viewer", nullptr)) {
                    OpenMemoryViewer();
                }
                if (ImGui::BeginMenu("Memory viewers")) {
                    for (auto &memView : m_memoryViewerWindows) {
                        ImGui::MenuItem(fmt::format("Memory viewer #{}", memView.Index() + 1).c_str(), nullptr,
                                        &memView.Open);
                    }
                    ImGui::EndMenu();
                }
                if (ImGui::MenuItem("Dump all memory", "F3")) {
                    m_context.eventQueues.emulator.enqueue(EmuEvent::MemoryDump());
                }
                ImGui::Separator();

                auto sh2Menu = [&](const char *name, ui::SH2WindowSet &set) {
                    if (ImGui::BeginMenu(name)) {
                        ImGui::MenuItem("[WIP] Debugger", nullptr, &set.debugger.Open);
                        ImGui::MenuItem("Interrupts", nullptr, &set.interrupts.Open);
                        ImGui::MenuItem("Interrupt trace", nullptr, &set.interruptTrace.Open);
                        ImGui::MenuItem("Cache", nullptr, &set.cache.Open);
                        ImGui::MenuItem("Division unit (DIVU)", nullptr, &set.divisionUnit.Open);
                        ImGui::MenuItem("Timers (FRT and WDT)", nullptr, &set.timers.Open);
                        ImGui::MenuItem("DMA Controller (DMAC)", nullptr, &set.dmaController.Open);
                        ImGui::MenuItem("DMA Controller trace", nullptr, &set.dmaControllerTrace.Open);
                        ImGui::EndMenu();
                    }
                };
                sh2Menu("Master SH2", m_masterSH2WindowSet);
                sh2Menu("Slave SH2", m_slaveSH2WindowSet);

                if (ImGui::BeginMenu("SCU")) {
                    ImGui::MenuItem("Registers", nullptr, &m_scuWindowSet.regs.Open);
                    ImGui::MenuItem("DSP", nullptr, &m_scuWindowSet.dsp.Open);
                    ImGui::MenuItem("DMA", nullptr, &m_scuWindowSet.dma.Open);
                    ImGui::MenuItem("DMA trace", nullptr, &m_scuWindowSet.dmaTrace.Open);
                    ImGui::MenuItem("Interrupt trace", nullptr, &m_scuWindowSet.intrTrace.Open);
                    ImGui::EndMenu();
                }
                ImGui::MenuItem("Debug output", nullptr, &m_debugOutputWindow.Open);
                ImGui::End();
            }
            if (ImGui::BeginMenu("Help")) {
                ImGui::MenuItem("ImGui demo window", nullptr, &showDemoWindow);
                ImGui::Separator();
                ImGui::MenuItem("About", nullptr, &m_aboutWindow.Open);
                ImGui::End();
            }
            ImGui::EndMainMenuBar();
        } else {
            ImGui::PopStyleVar();
        }

        {
            ImGuiViewport *viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->WorkPos);
            ImGui::SetNextWindowSize(viewport->WorkSize);
            ImGui::SetNextWindowViewport(viewport->ID);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
            ImGui::Begin("##dockspace_window", nullptr,
                         ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoDocking |
                             ImGuiWindowFlags_NoBackground);
            ImGui::PopStyleVar(3);
        }

        ImGui::DockSpace(ImGui::GetID("##main_dockspace"), ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
        {
            // Show the big ImGui demo window if enabled
            if (showDemoWindow) {
                ImGui::ShowDemoWindow(&showDemoWindow);
            }

            // Draw video output as a window
            if (screen.displayVideoOutputInWindow) {
                std::string title = fmt::format("Video Output - {}x{}###Display", screen.width, screen.height);

                const double aspectRatio = forceAspectRatio
                                               ? screen.scaleX / forcedAspect
                                               : (double)screen.height / screen.width * screen.scaleY / screen.scaleX;

                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
                ImGui::SetNextWindowSizeConstraints(
                    ImVec2(320, 224), ImVec2(FLT_MAX, FLT_MAX),
                    [](ImGuiSizeCallbackData *data) {
                        double aspectRatio = *(double *)data->UserData;
                        data->DesiredSize.y =
                            (float)(int)(data->DesiredSize.x * aspectRatio) + ImGui::GetFrameHeightWithSpacing();
                    },
                    (void *)&aspectRatio);
                if (ImGui::Begin(title.c_str(), &screen.displayVideoOutputInWindow, ImGuiWindowFlags_NoNavInputs)) {
                    const ImVec2 avail = ImGui::GetContentRegionAvail();
                    renderDispTexture(avail.x, avail.y);

                    ImGui::Image((ImTextureID)dispTexture, avail, ImVec2(0, 0),
                                 ImVec2((float)screen.width / vdp::kMaxResH, (float)screen.height / vdp::kMaxResV));
                }
                ImGui::End();
                ImGui::PopStyleVar();
            }

            // Draw regs windows
            DrawWindows();
        }
        ImGui::End();

        // ---------------------------------------------------------------------
        // Render window

        ImGui::Render();

        // Clear screen
        SDL_SetRenderDrawColorFloat(renderer, clearColor.x, clearColor.y, clearColor.z, clearColor.w);
        SDL_RenderClear(renderer);

        // Draw Saturn screen
        if (!screen.displayVideoOutputInWindow) {
            const bool aspectRatioChanged = forceAspectRatio && forcedAspect != prevForcedAspect;
            const bool forceAspectRatioChanged = prevForceAspectRatio != forceAspectRatio;
            const bool screenSizeChanged = aspectRatioChanged || forceAspectRatioChanged || screen.resolutionChanged;
            const bool fitWindowToScreen = (screen.autoResizeWindow && screenSizeChanged) || fitWindowToScreenNow;

            const float menuBarHeight = ImGui::GetFrameHeight();

            // Get window size
            int ww, wh;
            SDL_GetWindowSize(screen.window, &ww, &wh);
            wh -= menuBarHeight;

            double scaleFactor = 1.0;

            // Compute maximum scale to fit the display given the constraints above
            const double baseWidth =
                forceAspectRatio ? ceil(screen.height * forcedAspect * screen.scaleY) : screen.width * screen.scaleX;
            const double baseHeight = screen.height * screen.scaleY;
            const double scaleX = (double)ww / baseWidth;
            const double scaleY = (double)wh / baseHeight;
            double scale = std::max(1.0, std::min(scaleX, scaleY));

            // Preserve the previous scale if the aspect ratio changed or the force option was just enabled/disabled
            // when fitting the window to the screen
            if (fitWindowToScreen) {
                int screenWidth = screen.width;
                int screenHeight = screen.height;
                int screenScaleX = screen.scaleX;
                int screenScaleY = screen.scaleY;
                if (screen.resolutionChanged) {
                    // Handle double resolution scaling
                    const bool currDoubleRes = screen.prevWidth >= 640 || screen.prevHeight >= 400;
                    const bool nextDoubleRes = screen.width >= 640 || screen.height >= 400;
                    if (currDoubleRes != nextDoubleRes) {
                        scaleFactor = nextDoubleRes ? 0.5 : 2.0;
                    }
                    screenWidth = screen.prevWidth;
                    screenHeight = screen.prevHeight;
                    screenScaleX = screen.prevScaleX;
                    screenScaleY = screen.prevScaleY;
                }
                if (screenSizeChanged) {
                    const double baseWidth = forceAspectRatio ? ceil(screenHeight * prevForcedAspect * screenScaleY)
                                                              : screenWidth * screenScaleX;
                    const double baseHeight = screenHeight * screenScaleY;
                    const double scaleX = (double)ww / baseWidth;
                    const double scaleY = (double)wh / baseHeight;
                    scale = std::max(1.0, std::min(scaleX, scaleY));
                }
            }
            scale *= scaleFactor;
            if (forceIntegerScaling) {
                scale = floor(scale);
            }
            const int scaledWidth = baseWidth * scale;
            const int scaledHeight = baseHeight * scale;

            // Resize window without moving the display position relative to the screen
            if (fitWindowToScreen && (ww != scaledWidth || wh != scaledHeight)) {
                int wx, wy;
                SDL_GetWindowPosition(screen.window, &wx, &wy);

                int dx = scaledWidth - ww;
                int dy = scaledHeight - wh;
                SDL_SetWindowSize(screen.window, scaledWidth, scaledHeight + menuBarHeight);
                SDL_SetWindowPosition(screen.window, wx - dx / 2, wy - dy / 2);
            }

            // Render framebuffer to display texture
            renderDispTexture(scaledWidth, scaledHeight);

            // Determine how much slack there is on each axis in order to center the image on the window
            const int slackX = ww - scaledWidth;
            const int slackY = wh - scaledHeight;

            // Draw the texture
            SDL_FRect srcRect{.x = 0.0f,
                              .y = 0.0f,
                              .w = (float)(screen.width * screen.fbScale),
                              .h = (float)(screen.height * screen.fbScale)};
            SDL_FRect dstRect{.x = floor(slackX * 0.5f),
                              .y = floor(slackY * 0.5f + menuBarHeight),
                              .w = (float)scaledWidth,
                              .h = (float)scaledHeight};
            SDL_RenderTexture(renderer, dispTexture, &srcRect, &dstRect);
        }

        screen.resolutionChanged = false;
        /*screen.prevWidth = screen.width;
        screen.prevHeight = screen.height;
        screen.prevScaleX = screen.scaleX;
        screen.prevScaleY = screen.scaleY;*/

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

    while (true) {
        // Process all pending commands
        const size_t cmdCount = paused ? m_context.eventQueues.emulator.wait_dequeue_bulk(cmds.begin(), cmds.size())
                                       : m_context.eventQueues.emulator.try_dequeue_bulk(cmds.begin(), cmds.size());
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
                m_audioSystem.SetSilent(false);
                break;
            case SetPaused:
                paused = std::get<bool>(cmd.value);
                m_audioSystem.SetSilent(paused);
                break;
            case SetDebugTrace: //
            {
                const bool enable = std::get<bool>(cmd.value);
                m_context.saturn.EnableDebugTracing(enable);
                if (enable) {
                    m_context.saturn.masterSH2.UseTracer(&m_context.tracers.masterSH2);
                    m_context.saturn.slaveSH2.UseTracer(&m_context.tracers.slaveSH2);
                    m_context.saturn.SCU.UseTracer(&m_context.tracers.SCU);
                }
                fmt::println("Debug tracing {}", (enable ? "enabled" : "disabled"));
                break;
            }
            case SetEmulateSH2Cache: //
            {
                const bool enable = std::get<bool>(cmd.value);
                m_context.saturn.EnableSH2CacheEmulation(enable);
                fmt::println("SH2 cache emulation {}", (enable ? "enabled" : "disabled"));
                break;
            }
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
            case DebugWriteMain: {
                // TODO: support 16-bit and 32-bit writes
                auto [address, value, enableSideEffects] = std::get<EmuEvent::DebugWriteMainData>(cmd.value);
                if (enableSideEffects) {
                    m_context.saturn.mainBus.Write<uint8>(address, value);
                } else {
                    m_context.saturn.mainBus.Poke<uint8>(address, value);
                }
                break;
            }
            case DebugWriteSH2: {
                // TODO: support 16-bit and 32-bit writes
                auto [address, value, enableSideEffects, bypassSH2Cache, master] =
                    std::get<EmuEvent::DebugWriteSH2Data>(cmd.value);
                auto &sh2 = master ? m_context.saturn.masterSH2 : m_context.saturn.slaveSH2;
                auto &probe = sh2.GetProbe();
                if (enableSideEffects) {
                    probe.MemWriteByte(address, value, bypassSH2Cache);
                } else {
                    probe.MemPokeByte(address, value, bypassSH2Cache);
                }
                break;
            }
            case DebugDivide: {
                auto [div64, master] = std::get<EmuEvent::DebugDivideData>(cmd.value);
                auto &sh2 = master ? m_context.saturn.masterSH2 : m_context.saturn.slaveSH2;
                auto &probe = sh2.GetProbe();
                if (div64) {
                    probe.ExecuteDiv64();
                } else {
                    probe.ExecuteDiv32();
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
        if (!paused) {
            m_context.saturn.RunFrame();
        }
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
        m_context.eventQueues.emulator.enqueue(EmuEvent::LoadDisc(file));
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

void App::DrawWindows() {
    m_systemStatusWindow.Display();

    m_masterSH2WindowSet.DisplayAll();
    m_slaveSH2WindowSet.DisplayAll();

    m_scuWindowSet.DisplayAll();

    m_debugOutputWindow.Display();

    for (auto &memView : m_memoryViewerWindows) {
        memView.Display();
    }

    m_aboutWindow.Display();

    // TODO: SH2 instruction trace view
    // TODO: SH2 exception trace view
}

void App::OpenMemoryViewer() {
    for (auto &memView : m_memoryViewerWindows) {
        if (!memView.Open) {
            memView.Open = true;
            memView.RequestFocus();
            return;
        }
    }

    // If there are no more free memory viewers, request focus on the first window
    m_memoryViewerWindows[0].RequestFocus();

    // If there are no more free memory viewers, create more!
    /*auto &memView = m_memoryViewerWindows.emplace_back(m_context);
    memView.Open = true;
    memView.RequestFocus();*/
}

} // namespace app
