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

    void OpenDiscImageFileDialog(const char *const *filelist, int filter);
    bool LoadDiscImage(std::filesystem::path path);

    // -----------------------------------------------------------------------------------------------------------------
    // Debugger

    struct SH2Tracer final : public satemu::debug::ISH2Tracer {
        void Interrupt(uint8 vecNum, uint8 level, uint32 pc) final;
        void Exception(uint8 vecNum, uint32 pc, uint32 sr) final;

        struct InterruptInfo {
            uint8 vecNum;
            uint8 level;
            uint32 pc;
        };

        struct ExceptionInfo {
            uint8 vecNum;
            uint32 pc;
            uint32 sr;
        };

        std::array<InterruptInfo, 16> interrupts;
        size_t interruptsPos = 0;
        size_t interruptsCount = 0;

        std::array<ExceptionInfo, 16> exceptions;
        size_t exceptionsPos = 0;
        size_t exceptionsCount = 0;
    };

    struct SCUTracer final : public satemu::debug::ISCUTracer {
        struct InterruptInfo {
            uint8 index;
            uint8 level; // 0xFF == acknowledge
        };

        void RaiseInterrupt(uint8 index, uint8 level) final;
        void AcknowledgeInterrupt(uint8 index) final;

        std::array<InterruptInfo, 16> interrupts;
        size_t interruptsPos = 0;
        size_t interruptsCount = 0;

    private:
        void PushInterrupt(InterruptInfo info);
    };

    SH2Tracer m_masterSH2Tracer;
    SH2Tracer m_slaveSH2Tracer;
    SCUTracer m_scuTracer;
};

} // namespace app
