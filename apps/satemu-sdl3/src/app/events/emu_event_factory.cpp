#include "emu_event_factory.hpp"

#include <app/shared_context.hpp>

#include <satemu/util/dev_log.hpp>

#include <fstream>
#include <iostream>

using namespace satemu;

namespace app::events::emu {

namespace grp {

    // -----------------------------------------------------------------------------
    // Dev log groups

    // Hierarchy:
    //
    // base

    struct base {
        static constexpr bool enabled = true;
        static constexpr devlog::Level level = devlog::level::debug;
        static constexpr std::string_view name = "Emulator";
    };

} // namespace grp

EmuEvent SetClockSpeed(sys::ClockSpeed clockSpeed) {
    return RunFunction([=](SharedContext &ctx) { ctx.saturn.SetClockSpeed(clockSpeed); });
}

EmuEvent SetVideoStandard(sys::VideoStandard videoStandard) {
    return RunFunction([=](SharedContext &ctx) { ctx.saturn.SetVideoStandard(videoStandard); });
}

EmuEvent SetAreaCode(uint8 areaCode) {
    return RunFunction([=](SharedContext &ctx) { ctx.saturn.SMPC.SetAreaCode(areaCode); });
}

EmuEvent SetDebugTrace(bool enable) {
    return RunFunction([=](SharedContext &ctx) {
        ctx.saturn.EnableDebugTracing(enable);
        if (enable) {
            ctx.saturn.masterSH2.UseTracer(&ctx.tracers.masterSH2);
            ctx.saturn.slaveSH2.UseTracer(&ctx.tracers.slaveSH2);
            ctx.saturn.SCU.UseTracer(&ctx.tracers.SCU);
        }
        devlog::info<grp::base>("Debug tracing {}", (enable ? "enabled" : "disabled"));
    });
}

EmuEvent SetEmulateSH2Cache(bool enable) {
    return RunFunction([=](SharedContext &ctx) {
        ctx.saturn.EnableSH2CacheEmulation(enable);
        devlog::info<grp::base>("SH2 cache emulation {}", (enable ? "enabled" : "disabled"));
    });
}

EmuEvent DumpMemory() {
    return RunFunction([](SharedContext &ctx) {
        devlog::info<grp::base>("Dumping all memory to {}...", std::filesystem::current_path().string());
        {
            std::ofstream out{"wram-lo.bin", std::ios::binary};
            ctx.saturn.mem.DumpWRAMLow(out);
        }
        {
            std::ofstream out{"wram-hi.bin", std::ios::binary};
            ctx.saturn.mem.DumpWRAMHigh(out);
        }
        {
            std::ofstream out{"vdp1-vram.bin", std::ios::binary};
            ctx.saturn.VDP.DumpVDP1VRAM(out);
        }
        {
            std::ofstream out{"vdp1-fbs.bin", std::ios::binary};
            ctx.saturn.VDP.DumpVDP1Framebuffers(out);
        }
        {
            std::ofstream out{"vdp2-vram.bin", std::ios::binary};
            ctx.saturn.VDP.DumpVDP2VRAM(out);
        }
        {
            std::ofstream out{"vdp2-cram.bin", std::ios::binary};
            ctx.saturn.VDP.DumpVDP2CRAM(out);
        }
        {
            std::ofstream out{"scu-dsp-prog.bin", std::ios::binary};
            ctx.saturn.SCU.DumpDSPProgramRAM(out);
        }
        {
            std::ofstream out{"scu-dsp-data.bin", std::ios::binary};
            ctx.saturn.SCU.DumpDSPDataRAM(out);
        }
        {
            std::ofstream out{"scu-dsp-regs.bin", std::ios::binary};
            ctx.saturn.SCU.DumpDSPRegs(out);
        }
        {
            std::ofstream out{"scsp-wram.bin", std::ios::binary};
            ctx.saturn.SCSP.DumpWRAM(out);
        }
        {
            std::ofstream out{"scsp-dsp-mpro.bin", std::ios::binary};
            ctx.saturn.SCSP.DumpDSP_MPRO(out);
        }
        {
            std::ofstream out{"scsp-dsp-temp.bin", std::ios::binary};
            ctx.saturn.SCSP.DumpDSP_TEMP(out);
        }
        {
            std::ofstream out{"scsp-dsp-mems.bin", std::ios::binary};
            ctx.saturn.SCSP.DumpDSP_MEMS(out);
        }
        {
            std::ofstream out{"scsp-dsp-coef.bin", std::ios::binary};
            ctx.saturn.SCSP.DumpDSP_COEF(out);
        }
        {
            std::ofstream out{"scsp-dsp-madrs.bin", std::ios::binary};
            ctx.saturn.SCSP.DumpDSP_MADRS(out);
        }
        {
            std::ofstream out{"scsp-dsp-mixs.bin", std::ios::binary};
            ctx.saturn.SCSP.DumpDSP_MIXS(out);
        }
        {
            std::ofstream out{"scsp-dsp-efreg.bin", std::ios::binary};
            ctx.saturn.SCSP.DumpDSP_EFREG(out);
        }
        {
            std::ofstream out{"scsp-dsp-exts.bin", std::ios::binary};
            ctx.saturn.SCSP.DumpDSP_EXTS(out);
        }
        {
            std::ofstream out{"scsp-dsp-regs.bin", std::ios::binary};
            ctx.saturn.SCSP.DumpDSPRegs(out);
        }
    });
}

EmuEvent InsertBackupMemoryCartridge(std::filesystem::path path) {
    return RunFunction([=](SharedContext &ctx) {
        std::error_code error{};
        bup::BackupMemory bupMem{};
        const auto result = bupMem.LoadFrom(path, error);
        switch (result) {
        case bup::BackupMemoryImageLoadResult::Success:
            ctx.saturn.InsertCartridge<cart::BackupMemoryCartridge>(std::move(bupMem));
            devlog::info<grp::base>("External backup memory cartridge loaded from {}", path.string());
            break;
        case bup::BackupMemoryImageLoadResult::FilesystemError:
            if (error) {
                devlog::warn<grp::base>("Failed to load external backup memory: {}", error.message());
            } else {
                devlog::warn<grp::base>("Failed to load external backup memory: Unspecified file system error");
            }
            break;
        case bup::BackupMemoryImageLoadResult::InvalidSize:
            devlog::warn<grp::base>("Failed to load external backup memory: Invalid image size");
            break;
        default: devlog::warn<grp::base>("Failed to load external backup memory: Unexpected error"); break;
        }
    });
}

EmuEvent Insert8MbitDRAMCartridge() {
    return RunFunction([](SharedContext &ctx) { ctx.saturn.InsertCartridge<cart::DRAM8MbitCartridge>(); });
}

EmuEvent Insert32MbitDRAMCartridge() {
    return RunFunction([](SharedContext &ctx) { ctx.saturn.InsertCartridge<cart::DRAM32MbitCartridge>(); });
}

EmuEvent DeleteBackupFile(std::string filename, bool external) {
    if (external) {
        return RunFunction([=](SharedContext &ctx) {
            if (auto *cart = cart::As<cart::CartType::BackupMemory>(ctx.saturn.GetCartridge())) {
                cart->GetBackupMemory().Delete(filename);
            }
        });
    } else {
        return RunFunction([=](SharedContext &ctx) { ctx.saturn.mem.GetInternalBackupRAM().Delete(filename); });
    }
}

EmuEvent FormatBackupMemory(bool external) {
    if (external) {
        return RunFunction([](SharedContext &ctx) {
            if (auto *cart = cart::As<cart::CartType::BackupMemory>(ctx.saturn.GetCartridge())) {
                cart->GetBackupMemory().Format();
            }
        });
    } else {
        return RunFunction([](SharedContext &ctx) { ctx.saturn.mem.GetInternalBackupRAM().Format(); });
    }
}

} // namespace app::events::emu
