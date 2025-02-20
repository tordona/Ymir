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

    struct SH2Tracer final : public satemu::debug::ISH2Tracer {
        void Interrupt(uint8 vecNum, uint8 level) final;

        std::array<SH2InterruptInfo, 16> interrupts;
        size_t interruptsPos = 0;
        size_t interruptsCount = 0;
    };

    SH2Tracer m_masterSH2Tracer;
    SH2Tracer m_slaveSH2Tracer;
};

} // namespace app
