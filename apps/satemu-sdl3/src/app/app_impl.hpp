#pragma once

#include "app.hpp"

#include <satemu/sys/saturn.hpp>

namespace app {

class App::Impl {
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

    void TraceSH2Interrupt(bool master, uint8 vecnum, uint8 level);

    struct AppTracer final : public satemu::debug::ITracer {
        AppTracer(Impl &app);

        void SH2_Interrupt(bool master, uint8 vecnum, uint8 level) final;

    private:
        Impl &m_app;
    };
};

} // namespace app
