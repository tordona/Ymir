#pragma once

#include "app.hpp"

#include <satemu/sys/saturn.hpp>

#include <satemu/debug/debug_tracer_sh2.hpp>

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

    void TraceSH2Interrupt(bool master, uint8 vecNum, uint8 level);

    struct AppSH2Tracer final : public satemu::debug::ISH2Tracer {
        AppSH2Tracer(Impl &app, bool master);

        void Interrupt(uint8 vecNum, uint8 level) final;

    private:
        Impl &m_app;
        bool m_master;
    };

    struct AppTracer final : public satemu::debug::ITracer {
        AppTracer(Impl &app);

        AppSH2Tracer &GetMasterSH2Tracer() final;
        AppSH2Tracer &GetSlaveSH2Tracer() final;

    private:
        Impl &m_app;
        AppSH2Tracer m_masterSH2Tracer;
        AppSH2Tracer m_slaveSH2Tracer;
    };
};

} // namespace app
