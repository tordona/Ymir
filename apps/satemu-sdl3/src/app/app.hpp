#pragma once

#include "cmdline_opts.hpp"

#include "audio_system.hpp"
#include "context.hpp"

#include <satemu/sys/saturn.hpp>

#include "debug/scu_tracer.hpp"
#include "debug/sh2_tracer.hpp"

#include "events/emu_events.hpp"

#include "ui/scu_debugger.hpp"
#include "ui/sh2_debugger.hpp"

#include "blockingconcurrentqueue.h"

#include <SDL3/SDL_events.h>

#include <imgui.h>

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

    Context m_context;
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

    SH2Tracer m_masterSH2Tracer;
    SH2Tracer m_slaveSH2Tracer;
    SCUTracer m_scuTracer;

    SH2Debugger m_masterSH2Debugger;
    SH2Debugger m_slaveSH2Debugger;
    SCUDebugger m_scuDebugger;
};

} // namespace app
