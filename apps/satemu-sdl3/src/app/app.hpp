#pragma once

#include "cmdline_opts.hpp"

#include "audio_system.hpp"
#include "shared_context.hpp"

#include "ui/windows/about_window.hpp"
#include "ui/windows/system_status_window.hpp"

#include "ui/windows/debug/debug_output_window.hpp"
#include "ui/windows/debug/memory_viewer_window.hpp"
#include "ui/windows/debug/scu_window_set.hpp"
#include "ui/windows/debug/sh2_window_set.hpp"

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
    // Windows

    void DrawWindows();
    void OpenMemoryViewer();

    ui::SystemStatusWindow m_systemStatusWindow;

    ui::SH2WindowSet m_masterSH2WindowSet;
    ui::SH2WindowSet m_slaveSH2WindowSet;

    ui::SCUWindowSet m_scuWindowSet;

    ui::DebugOutputWindow m_debugOutputWindow;

    std::vector<ui::MemoryViewerWindow> m_memoryViewerWindows;

    ui::AboutWindow m_aboutWindow;
};

} // namespace app
