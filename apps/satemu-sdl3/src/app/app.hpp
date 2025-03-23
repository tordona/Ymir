#pragma once

#include "cmdline_opts.hpp"

#include "audio_system.hpp"
#include "shared_context.hpp"

#include "events/emu_events.hpp"

#include "ui/scu_debugger.hpp"
#include "ui/sh2_debugger.hpp"
#include "ui/sh2_interrupt_tracer.hpp"
#include "ui/sh2_interrupts.hpp"

#include "blockingconcurrentqueue.h"

#include <SDL3/SDL_events.h>

#include <imgui.h>

#include <imgui_memory_editor.h>

#include <string>
#include <thread>
#include <variant>

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
    moodycamel::BlockingConcurrentQueue<EmuEvent> m_emuEventQueue;

    AudioSystem m_audioSystem;

    void RunEmulator();

    void EmulatorThread();

    void OpenLoadDiscDialog();
    void ProcessOpenDiscImageFileDialogSelection(const char *const *filelist, int filter);
    bool LoadDiscImage(std::filesystem::path path);

    // -----------------------------------------------------------------------------------------------------------------
    // Debugger

    void DrawDebug();

    SH2Debugger m_masterSH2Debugger;
    SH2Interrupts m_masterSH2Interrupts;
    SH2InterruptTracer m_masterSH2InterruptTracer;

    SH2Debugger m_slaveSH2Debugger;
    SH2Interrupts m_slaveSH2Interrupts;
    SH2InterruptTracer m_slaveSH2InterruptTracer;

    SCUDebugger m_scuDebugger;

    MemoryEditor m_memoryViewer;
    bool m_enableSideEffects = false;
};

} // namespace app
