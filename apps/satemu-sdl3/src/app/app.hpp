#pragma once

#include "cmdline_opts.hpp"

#include "audio_system.hpp"
#include "shared_context.hpp"

#include "ui/windows/memory_viewer_window.hpp"
#include "ui/windows/scu_debugger_window.hpp"
#include "ui/windows/sh2_debugger_window.hpp"
#include "ui/windows/sh2_division_unit_window.hpp"
#include "ui/windows/sh2_interrupt_tracer_window.hpp"
#include "ui/windows/sh2_interrupts_window.hpp"

#include <SDL3/SDL_events.h>

#include <imgui.h>

#include <thread>
#include <vector>

namespace app {

class App {
public:
    App();

    int Run(const CommandLineOptions &options);

private:
    CommandLineOptions m_options;

    SharedContext m_context;
    SDL_PropertiesID m_fileDialogProps;

    std::thread m_emuThread;

    AudioSystem m_audioSystem;

    void RunEmulator();

    void EmulatorThread();

    void OpenLoadDiscDialog();
    void ProcessOpenDiscImageFileDialogSelection(const char *const *filelist, int filter);
    bool LoadDiscImage(std::filesystem::path path);

    // -----------------------------------------------------------------------------------------------------------------
    // Debugger

    void DrawDebug();

    ui::SH2DebuggerWindow m_masterSH2DebuggerWindow;
    ui::SH2InterruptsWindow m_masterSH2InterruptsWindow;
    ui::SH2InterruptTracerWindow m_masterSH2InterruptTracerWindow;
    ui::SH2DivisionUnitWindow m_masterSH2DivisionUnitWindow;

    ui::SH2DebuggerWindow m_slaveSH2DebuggerWindow;
    ui::SH2InterruptsWindow m_slaveSH2InterruptsWindow;
    ui::SH2InterruptTracerWindow m_slaveSH2InterruptTracerWindow;
    ui::SH2DivisionUnitWindow m_slaveSH2DivisionUnitWindow;

    ui::SCUDebuggerWindow m_scuDebuggerWindow;

    std::vector<ui::MemoryViewerWindow> m_memoryViewerWindows;

    void OpenMemoryViewer();
};

} // namespace app
