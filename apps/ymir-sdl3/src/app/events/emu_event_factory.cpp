#include "emu_event_factory.hpp"

#include "gui_event_factory.hpp"

#include <app/shared_context.hpp>

#include <util/ipl_rom_loader.hpp>
#include <ymir/util/dev_log.hpp>
#include <ymir/util/scope_guard.hpp>

#include <fstream>
#include <iostream>

using namespace ymir;

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

EmuEvent SetVideoStandard(config::sys::VideoStandard videoStandard) {
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

EmuEvent DumpMemory() {
    return RunFunction([](SharedContext &ctx) {
        devlog::info<grp::base>("Dumping all memory to {}...", std::filesystem::current_path().string());
        auto dumpPath = ctx.profile.GetPath(ProfilePath::Dumps);
        {
            std::ofstream out{dumpPath / "msh2-cache-data.bin", std::ios::binary};
            ctx.saturn.masterSH2.DumpCacheData(out);
        }
        {
            std::ofstream out{dumpPath / "msh2-cache-addrtag.bin", std::ios::binary};
            ctx.saturn.masterSH2.DumpCacheAddressTag(out);
        }
        {
            std::ofstream out{dumpPath / "ssh2-cache-data.bin", std::ios::binary};
            ctx.saturn.slaveSH2.DumpCacheData(out);
        }
        {
            std::ofstream out{dumpPath / "ssh2-cache-addrtag.bin", std::ios::binary};
            ctx.saturn.slaveSH2.DumpCacheAddressTag(out);
        }
        {
            std::ofstream out{dumpPath / "wram-lo.bin", std::ios::binary};
            ctx.saturn.mem.DumpWRAMLow(out);
        }
        {
            std::ofstream out{dumpPath / "wram-hi.bin", std::ios::binary};
            ctx.saturn.mem.DumpWRAMHigh(out);
        }
        {
            std::ofstream out{dumpPath / "vdp1-vram.bin", std::ios::binary};
            ctx.saturn.VDP.DumpVDP1VRAM(out);
        }
        {
            std::ofstream out{dumpPath / "vdp1-fbs.bin", std::ios::binary};
            ctx.saturn.VDP.DumpVDP1Framebuffers(out);
        }
        {
            std::ofstream out{dumpPath / "vdp2-vram.bin", std::ios::binary};
            ctx.saturn.VDP.DumpVDP2VRAM(out);
        }
        {
            std::ofstream out{dumpPath / "vdp2-cram.bin", std::ios::binary};
            ctx.saturn.VDP.DumpVDP2CRAM(out);
        }
        {
            std::ofstream out{dumpPath / "scu-dsp-prog.bin", std::ios::binary};
            ctx.saturn.SCU.DumpDSPProgramRAM(out);
        }
        {
            std::ofstream out{dumpPath / "scu-dsp-data.bin", std::ios::binary};
            ctx.saturn.SCU.DumpDSPDataRAM(out);
        }
        {
            std::ofstream out{dumpPath / "scu-dsp-regs.bin", std::ios::binary};
            ctx.saturn.SCU.DumpDSPRegs(out);
        }
        {
            std::ofstream out{dumpPath / "scsp-wram.bin", std::ios::binary};
            ctx.saturn.SCSP.DumpWRAM(out);
        }
        {
            std::ofstream out{dumpPath / "scsp-dsp-mpro.bin", std::ios::binary};
            ctx.saturn.SCSP.DumpDSP_MPRO(out);
        }
        {
            std::ofstream out{dumpPath / "scsp-dsp-temp.bin", std::ios::binary};
            ctx.saturn.SCSP.DumpDSP_TEMP(out);
        }
        {
            std::ofstream out{dumpPath / "scsp-dsp-mems.bin", std::ios::binary};
            ctx.saturn.SCSP.DumpDSP_MEMS(out);
        }
        {
            std::ofstream out{dumpPath / "scsp-dsp-coef.bin", std::ios::binary};
            ctx.saturn.SCSP.DumpDSP_COEF(out);
        }
        {
            std::ofstream out{dumpPath / "scsp-dsp-madrs.bin", std::ios::binary};
            ctx.saturn.SCSP.DumpDSP_MADRS(out);
        }
        {
            std::ofstream out{dumpPath / "scsp-dsp-mixs.bin", std::ios::binary};
            ctx.saturn.SCSP.DumpDSP_MIXS(out);
        }
        {
            std::ofstream out{dumpPath / "scsp-dsp-efreg.bin", std::ios::binary};
            ctx.saturn.SCSP.DumpDSP_EFREG(out);
        }
        {
            std::ofstream out{dumpPath / "scsp-dsp-exts.bin", std::ios::binary};
            ctx.saturn.SCSP.DumpDSP_EXTS(out);
        }
        {
            std::ofstream out{dumpPath / "scsp-dsp-regs.bin", std::ios::binary};
            ctx.saturn.SCSP.DumpDSPRegs(out);
        }
        devlog::info<grp::base>("Dump complete");
    });
}

static void InsertPeripheral(peripheral::PeripheralType type, peripheral::PeripheralPort &port) {
    switch (type) {
    case ymir::peripheral::PeripheralType::None: port.DisconnectPeripherals(); break;
    case ymir::peripheral::PeripheralType::StandardPad: port.ConnectStandardPad(); break;
    }
}

EmuEvent InsertPort1Peripheral(peripheral::PeripheralType type) {
    return RunFunction([=](SharedContext &ctx) {
        std::unique_lock lock{ctx.locks.peripherals};
        InsertPeripheral(type, ctx.saturn.SMPC.GetPeripheralPort1());
    });
}

EmuEvent InsertPort2Peripheral(peripheral::PeripheralType type) {
    return RunFunction([=](SharedContext &ctx) {
        std::unique_lock lock{ctx.locks.peripherals};
        InsertPeripheral(type, ctx.saturn.SMPC.GetPeripheralPort2());
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
        };
    });
}

EmuEvent Insert8MbitDRAMCartridge() {
    return RunFunction([](SharedContext &ctx) { ctx.saturn.InsertCartridge<cart::DRAM8MbitCartridge>(); });
}

EmuEvent Insert32MbitDRAMCartridge() {
    return RunFunction([](SharedContext &ctx) { ctx.saturn.InsertCartridge<cart::DRAM32MbitCartridge>(); });
}

EmuEvent InsertCartridgeFromSettings() {
    return RunFunction([](SharedContext &ctx) {
        std::unique_lock lock{ctx.locks.cart};

        auto &settings = ctx.settings.cartridge;

        switch (settings.type) {
        case Settings::Cartridge::Type::None:
            ctx.saturn.RemoveCartridge();
            devlog::info<grp::base>("Cartridge removed");
            break;

        case Settings::Cartridge::Type::BackupRAM: {
            // If a backup RAM cartridge is inserted, remove it first to unlock the file and reinsert the previous
            // cartridge in case of failure
            std::filesystem::path prevPath = "";
            util::ScopeGuard sgReinsertOnFailure{[&] {
                if (prevPath.empty()) {
                    return;
                }

                std::error_code error{};
                bup::BackupMemory bupMem{};
                auto result = bupMem.LoadFrom(prevPath, error);
                if (result == bup::BackupMemoryImageLoadResult::Success) {
                    ctx.saturn.InsertCartridge<cart::BackupMemoryCartridge>(std::move(bupMem));
                }
            }};
            if (auto *cart = ctx.saturn.GetCartridge().As<cart::CartType::BackupMemory>()) {
                prevPath = cart->GetBackupMemory().GetPath();
                if (prevPath == settings.backupRAM.imagePath) {
                    ctx.saturn.RemoveCartridge();
                } else {
                    sgReinsertOnFailure.Cancel();
                }
            } else {
                sgReinsertOnFailure.Cancel();
            }

            std::error_code error{};
            bup::BackupMemory bupMem{};
            bupMem.CreateFrom(settings.backupRAM.imagePath, CapacityToBupSize(settings.backupRAM.capacity), error);
            if (error) {
                devlog::info<grp::base>("Failed to insert {} backup RAM cartridge from {}: {}",
                                        BupCapacityShortName(settings.backupRAM.capacity),
                                        settings.backupRAM.imagePath.string(), error.message());
            } else {
                devlog::info<grp::base>("{} backup RAM cartridge inserted with image from {}",
                                        BupCapacityShortName(settings.backupRAM.capacity),
                                        settings.backupRAM.imagePath.string());
                ctx.saturn.InsertCartridge<cart::BackupMemoryCartridge>(std::move(bupMem));

                // If the cartridge was successfully inserted, we don't need to reinsert the previous cartridge
                sgReinsertOnFailure.Cancel();
            }
            break;
        }
        case Settings::Cartridge::Type::DRAM:
            switch (settings.dram.capacity) {
            case Settings::Cartridge::DRAM::Capacity::_32Mbit:
                ctx.saturn.InsertCartridge<cart::DRAM32MbitCartridge>();
                devlog::info<grp::base>("32 Mbit DRAM cartridge inserted");
                break;
            case Settings::Cartridge::DRAM::Capacity::_8Mbit:
                ctx.saturn.InsertCartridge<cart::DRAM8MbitCartridge>();
                devlog::info<grp::base>("8 Mbit DRAM cartridge inserted");
                break;
            }
            break;
        }
    });
}

EmuEvent DeleteBackupFile(std::string filename, bool external) {
    if (external) {
        return RunFunction([=](SharedContext &ctx) {
            if (auto *cart = ctx.saturn.GetCartridge().As<cart::CartType::BackupMemory>()) {
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
            if (auto *cart = ctx.saturn.GetCartridge().As<cart::CartType::BackupMemory>()) {
                cart->GetBackupMemory().Format();
            }
        });
    } else {
        return RunFunction([](SharedContext &ctx) { ctx.saturn.mem.GetInternalBackupRAM().Format(); });
    }
}

EmuEvent LoadInternalBackupMemory() {
    return RunFunction([](SharedContext &ctx) {
        std::filesystem::path path = ctx.settings.system.internalBackupRAMImagePath;
        if (path.empty()) {
            path = ctx.profile.GetPath(ProfilePath::PersistentState) / "bup-int.bin";
        }
        std::error_code error{};
        if (ctx.saturn.LoadInternalBackupMemoryImage(path, error); error) {
            devlog::warn<grp::base>("Failed to load internal backup memory from {}: {}", path.string(),
                                    error.message());
        } else {
            devlog::warn<grp::base>("Internal backup memory image loaded from {}", path.string());
        }
    });
}

EmuEvent SetEmulateSH2Cache(bool enable) {
    return RunFunction([=](SharedContext &ctx) {
        ctx.saturn.EnableSH2CacheEmulation(enable);
        devlog::info<grp::base>("SH2 cache emulation {}", (enable ? "enabled" : "disabled"));
    });
}

EmuEvent EnableThreadedVDP(bool enable) {
    return RunFunction([=](SharedContext &ctx) { ctx.saturn.configuration.video.threadedVDP = enable; });
}

EmuEvent EnableThreadedSCSP(bool enable) {
    return RunFunction([=](SharedContext &ctx) { ctx.saturn.configuration.audio.threadedSCSP = enable; });
}

EmuEvent LoadState(uint32 slot) {
    return RunFunction([=](SharedContext &ctx) {
        if (slot < ctx.saveStates.size() && ctx.saveStates[slot]) {
            if (!ctx.saturn.LoadState(*ctx.saveStates[slot])) {
                // TODO: notify failure to load save state
            }
        }
    });
}

EmuEvent SaveState(uint32 slot) {
    return RunFunction([=](SharedContext &ctx) {
        if (slot < ctx.saveStates.size()) {
            {
                std::unique_lock lock{ctx.locks.saveStates[slot]};
                if (!ctx.saveStates[slot]) {
                    ctx.saveStates[slot] = std::make_unique<state::State>();
                }
                ctx.saturn.SaveState(*ctx.saveStates[slot]);
            }
            ctx.EnqueueEvent(events::gui::StateSaved(slot));
        }
    });
}

} // namespace app::events::emu
