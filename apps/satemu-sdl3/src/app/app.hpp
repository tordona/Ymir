#pragma once

#include "cmdline_opts.hpp"

#include "audio_system.hpp"

#include <satemu/sys/saturn.hpp>

#include "debug/scu_tracer.hpp"
#include "debug/sh2_tracer.hpp"

namespace app {

class App {
public:
    int Run(const CommandLineOptions &options);

private:
    satemu::Saturn m_saturn;
    CommandLineOptions m_options;

    AudioSystem m_audioSystem;

    void RunEmulator();

    void OpenDiscImageFileDialog(const char *const *filelist, int filter);
    bool LoadDiscImage(std::filesystem::path path);

    // -----------------------------------------------------------------------------------------------------------------
    // Debugger

    void DrawDebug();

    SH2Tracer m_masterSH2Tracer;
    SH2Tracer m_slaveSH2Tracer;
    SCUTracer m_scuTracer;
};

} // namespace app
