#pragma once

#include "cmdline_opts.hpp"

#include "audio_system.hpp"

#include <satemu/sys/saturn.hpp>

#include "debug/scu_tracer.hpp"
#include "debug/sh2_tracer.hpp"

#include "blockingconcurrentqueue.h"

#include <imgui.h>

#include <string>
#include <thread>
#include <variant>

namespace app {

class App {
public:
    int Run(const CommandLineOptions &options);

private:
    satemu::Saturn m_saturn;
    CommandLineOptions m_options;

    struct Fonts {
        ImFont *sansSerifMedium = nullptr;
        ImFont *sansSerifBold = nullptr;
        ImFont *sansSerifMediumMedium = nullptr;
        ImFont *sansSerifMediumBold = nullptr;
        ImFont *sansSerifLargeBold = nullptr;
        ImFont *monospaceMedium = nullptr;
        ImFont *monospaceBold = nullptr;
        ImFont *monospaceMediumMedium = nullptr;
        ImFont *monospaceMediumBold = nullptr;
        ImFont *display = nullptr;
    } m_fonts;

    struct EmuCommand {
        enum class Type {
            FactoryReset,
            HardReset,
            SoftReset,
            FrameStep,

            SetPaused,

            SetDebugTrace,
            MemoryDump,

            OpenCloseTray,
            LoadDisc,
            EjectDisc,

            Shutdown
        };

        Type type;

        std::variant<bool, std::string> value;

        static EmuCommand FactoryReset() {
            return {.type = Type::FactoryReset};
        }

        static EmuCommand HardReset() {
            return {.type = Type::HardReset};
        }

        static EmuCommand SoftReset(bool resetLevel) {
            return {.type = Type::SoftReset, .value = resetLevel};
        }

        static EmuCommand FrameStep() {
            return {.type = Type::FrameStep};
        }

        static EmuCommand SetPaused(bool paused) {
            return {.type = Type::SetPaused, .value = paused};
        }

        static EmuCommand SetDebugTrace(bool enabled) {
            return {.type = Type::SetDebugTrace, .value = enabled};
        }

        static EmuCommand MemoryDump() {
            return {.type = Type::MemoryDump};
        }

        static EmuCommand OpenCloseTray() {
            return {.type = Type::OpenCloseTray};
        }

        static EmuCommand LoadDisc(std::string path) {
            return {.type = Type::LoadDisc, .value = path};
        }

        static EmuCommand EjectDisc() {
            return {.type = Type::EjectDisc};
        }

        static EmuCommand Shutdown() {
            return {.type = Type::Shutdown};
        }
    };

    std::thread m_emuThread;
    moodycamel::BlockingConcurrentQueue<EmuCommand> m_emuCommandQueue;

    AudioSystem m_audioSystem;

    void RunEmulator();

    void EmulatorThread();

    void ProcessOpenDiscImageFileDialogSelection(const char *const *filelist, int filter);
    bool LoadDiscImage(std::filesystem::path path);

    // -----------------------------------------------------------------------------------------------------------------
    // Debugger

    void DrawDebug();

    SH2Tracer m_masterSH2Tracer;
    SH2Tracer m_slaveSH2Tracer;
    SCUTracer m_scuTracer;
};

} // namespace app
