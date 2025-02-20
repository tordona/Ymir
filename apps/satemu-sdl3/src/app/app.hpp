#pragma once

#include "cmdline_opts.hpp"

#include <satemu/sys/saturn.hpp>

namespace app {

class App {
public:
    int Run(const CommandLineOptions &options);

private:
    satemu::Saturn m_saturn;
    CommandLineOptions m_options;

    void RunEmulator();

    // -----------------------------------------------------------------------------------------------------------------
    // Debugger

    struct SH2InterruptInfo {
        uint8 vecNum;
        uint8 level;
    };
    std::array<SH2InterruptInfo, 16> m_masterSH2Interrupts;
    size_t m_masterSH2InterruptsPos = 0;
    size_t m_masterSH2InterruptsCount = 0;

    std::array<SH2InterruptInfo, 16> m_slaveSH2Interrupts;
    size_t m_slaveSH2InterruptsPos = 0;
    size_t m_slaveSH2InterruptsCount = 0;

    void TraceSH2Interrupt(bool master, uint8 vecNum, uint8 level);

    struct AppSystemTracer final : public satemu::debug::ISystemTracer {
        AppSystemTracer(App &app);

    private:
        App &m_app;
    };

    struct AppSH2Tracer final : public satemu::debug::ISH2Tracer {
        AppSH2Tracer(App &app, bool master);

        void Interrupt(uint8 vecNum, uint8 level) final;

    private:
        App &m_app;
        bool m_master;
    };
};

} // namespace app
