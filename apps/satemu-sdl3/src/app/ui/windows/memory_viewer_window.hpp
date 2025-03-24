#pragma once

#include <app/shared_context.hpp>

#include <imgui.h>

#include <imgui_memory_editor.h>

#include <fmt/format.h>

#include <memory>
#include <span>

namespace app::ui {

class MemoryViewerWindow {
public:
    MemoryViewerWindow(SharedContext &context);

    void Display();
    void RequestFocus();

    uint32 Index() const {
        return m_index;
    }

    bool Open = false;

private:
    struct Region;

    struct Context {
        Context(SharedContext &sharedCtx)
            : sharedCtx(sharedCtx) {}

        SharedContext &sharedCtx;
        MemoryEditor memoryEditor;
        bool enableSideEffects = false;
        const Region *selectedRegion = nullptr;
    };

    struct Region {
        using ReadFn = ImU8 (*)(const ImU8 *mem, size_t off, void *user_data);
        using WriteFn = void (*)(ImU8 *mem, size_t off, ImU8 d, void *user_data);
        using BgColorFn = ImU32 (*)(const ImU8 *mem, size_t off, void *user_data);
        using ParamsFn = void (*)(Context *ctx);

        const char *name;
        const char *addressBlockName;
        uint32 baseAddress;
        uint32 size;
        ReadFn readFn;
        WriteFn writeFn;
        BgColorFn bgColorFn;
        ParamsFn paramsFn;

        std::string ToString() const {
            return fmt::format("[{}:{:08X}..{:08X}] {}", addressBlockName, baseAddress, baseAddress + size - 1, name);
        }
    };

    struct RegionGroup {
        const char *name;
        std::span<const Region> regions;
    };

    static uint32 s_index;
    SharedContext &m_sharedCtx;
    uint32 m_index;
    bool m_requestFocus = false;

    std::unique_ptr<Context> m_context;

    // -------------------------------------------------------------------------
    // Region handlers

    // --- Main address space ------
    // [Main:0000000..7FFFFFF] Main address space
    // [Main:0000000..00FFFFF] Boot ROM / IPL
    // [Main:0100000..010007F] SMPC registers
    // [Main:0180000..018FFFF] Internal backup RAM
    // [Main:0200000..02FFFFF] Low Work RAM
    // [Main:1000000..17FFFFF] MINIT area
    // [Main:1800000..1FFFFFF] SINIT area
    // [Main:2000000..5FFFFFF] SCU A-Bus
    // [Main:2000000..3FFFFFF] SCU A-Bus CS0
    // [Main:4000000..4FFFFFF] SCU A-Bus CS1
    // [Main:5800000..58FFFFF] SCU A-Bus CS2
    // [Main:5A00000..5FBFFFF] SCU B-Bus
    // [Main:5A00000..5A7FFFF] 68000 Work RAM
    // [Main:5B00000..5B00FFF] SCSP registers
    // [Main:5C00000..5C7FFFF] VDP1 VRAM
    // [Main:5C80000..5CBFFFF] VDP1 framebuffer
    // [Main:5D00000..5D0001F] VDP1 registers
    // [Main:5E00000..5E7FFFF] VDP2 VRAM
    // [Main:5F00000..5F00FFF] VDP2 CRAM
    // [Main:5F80000..5F801FF] VDP2 registers
    // [Main:5FE0000..5FE00FF] SCU registers
    // [Main:6000000..60FFFFF] High Work RAM
    // --- Master SH-2 -------------
    // NOTE: the associate purge area is intentionally omitted; the cache viewer is going to be more useful for that
    // [MSH2:00000000..07FFFFFF] MSH2 cached address space
    // [MSH2:20000000..27FFFFFF] MSH2 uncached address space
    // [MSH2:60000000..600003FF] MSH2 cache address array   (based on currently selected way)
    // [MSH2:C0000000..C0000FFF] MSH2 cache data array
    // [MSH2:FFFFFE00..FFFFFFFF] MSH2 on-chip registers
    // --- Slave SH-2 --------------
    // NOTE: the associate purge area is intentionally omitted; the cache viewer is going to be more useful for that
    // [SSH2:00000000..07FFFFFF] SSH2 cached address space
    // [SSH2:20000000..27FFFFFF] SSH2 uncached address space
    // [SSH2:60000000..600003FF] SSH2 cache address array   (based on currently selected way)
    // [SSH2:C0000000..C0000FFF] SSH2 cache data array
    // [SSH2:FFFFFE00..FFFFFFFF] SSH2 on-chip registers

    // TODO: cartridge contents
    // --- Cartridge ---------------
    // NOTE: populate based on the currently inserted cartridge
    // [Cart:000000..07FFFF] Backup RAM cart data    (4 Mbit)
    // [Cart:000000..0FFFFF] Backup RAM cart data    (8 Mbit)
    // [Cart:000000..1FFFFF] Backup RAM cart data   (16 Mbit)
    // [Cart:000000..3FFFFF] Backup RAM cart data   (32 Mbit)
    // [Cart:000000..0FFFFF] DRAM cart data          (8 Mbit)
    // [Cart:000000..3FFFFF] DRAM cart data         (32 Mbit)

    // TODO: raw CD-ROM contents
    // --- CD-ROM ------------------
    // [Disc:00000000..xxxxxxxx] CD-ROM contents

    static ImU8 MainBusRead(const ImU8 *mem, size_t off, void *user_data) {
        auto &ctx = *static_cast<const Context *>(user_data);
        off += ctx.selectedRegion->baseAddress;
        return ctx.sharedCtx.saturn.mainBus.Peek<uint8>(off);
    }

    static void MainBusWrite(ImU8 *mem, size_t off, ImU8 d, void *user_data) {
        auto &ctx = *static_cast<Context *>(user_data);
        off += ctx.selectedRegion->baseAddress;
        ctx.sharedCtx.eventQueues.emulator.enqueue(EmuEvent::DebugWriteMain(off, d, ctx.enableSideEffects));
    }

    static ImU32 MainBusBgColor(const ImU8 * /*mem*/, size_t /*off*/, void * /*user_data*/) {
        // auto &ctx = *static_cast<Context *>(user_data);
        // off += ctx.selectedRegion->baseAddress;
        // TODO: use this to colorize fields/regions
        return 0;
    }

    template <bool master>
    static ImU8 SH2BusRead(const ImU8 *mem, size_t off, void *user_data) {
        auto &ctx = *static_cast<const Context *>(user_data);
        off += ctx.selectedRegion->baseAddress;
        auto &sh2 = master ? ctx.sharedCtx.saturn.masterSH2 : ctx.sharedCtx.saturn.slaveSH2;
        return sh2.GetProbe().MemPeekByte(off);
    }

    template <bool master>
    static void SH2BusWrite(ImU8 *mem, size_t off, ImU8 d, void *user_data) {
        auto &ctx = *static_cast<Context *>(user_data);
        off += ctx.selectedRegion->baseAddress;
        ctx.sharedCtx.eventQueues.emulator.enqueue(EmuEvent::DebugWriteSH2(off, d, ctx.enableSideEffects, master));
    }

    template <bool master>
    static ImU32 SH2BusBgColor(const ImU8 * /*mem*/, size_t /*off*/, void * /*user_data*/) {
        // auto &ctx = *static_cast<Context *>(user_data);
        // off += ctx.selectedRegion->baseAddress;
        // auto &sh2 = master ? ctx.sharedCtx.saturn.masterSH2 : ctx.sharedCtx.saturn.slaveSH2;
        // TODO: use this to colorize fields/regions
        return 0;
    }

    static constexpr Region kMainRegions[] = {
        {
            .name = "Main address space",
            .addressBlockName = "Main",
            .baseAddress = 0x0000000,
            .size = 0x8000000,
            .readFn = MainBusRead,
            .writeFn = MainBusWrite,
            .bgColorFn = MainBusBgColor,
            .paramsFn = nullptr,
        },
        {
            .name = "Boot ROM / IPL",
            .addressBlockName = "Main",
            .baseAddress = 0x0000000,
            .size = 0x100000,
            .readFn = MainBusRead,
            .writeFn = MainBusWrite,
            .bgColorFn = MainBusBgColor,
            .paramsFn = nullptr,
        },
        {
            .name = "SMPC registers",
            .addressBlockName = "Main",
            .baseAddress = 0x0100000,
            .size = 0x80,
            .readFn = MainBusRead,
            .writeFn = MainBusWrite,
            .bgColorFn = MainBusBgColor,
            .paramsFn = nullptr,
        },
        {
            .name = "Internal backup RAM",
            .addressBlockName = "Main",
            .baseAddress = 0x0180000,
            .size = 0x10000,
            .readFn = MainBusRead,
            .writeFn = MainBusWrite,
            .bgColorFn = MainBusBgColor,
            .paramsFn = nullptr,
        },
        {
            .name = "Low Work RAM",
            .addressBlockName = "Main",
            .baseAddress = 0x0200000,
            .size = 0x100000,
            .readFn = MainBusRead,
            .writeFn = MainBusWrite,
            .bgColorFn = MainBusBgColor,
            .paramsFn = nullptr,
        },
        {
            .name = "MINIT area",
            .addressBlockName = "Main",
            .baseAddress = 0x1000000,
            .size = 0x800000,
            .readFn = MainBusRead,
            .writeFn = MainBusWrite,
            .bgColorFn = MainBusBgColor,
            .paramsFn = nullptr,
        },
        {
            .name = "SINIT area",
            .addressBlockName = "Main",
            .baseAddress = 0x1000000,
            .size = 0x800000,
            .readFn = MainBusRead,
            .writeFn = MainBusWrite,
            .bgColorFn = MainBusBgColor,
            .paramsFn = nullptr,
        },
        {
            .name = "SCU A-Bus",
            .addressBlockName = "Main",
            .baseAddress = 0x2000000,
            .size = 0x4000000,
            .readFn = MainBusRead,
            .writeFn = MainBusWrite,
            .bgColorFn = MainBusBgColor,
            .paramsFn = nullptr,
        },
        {
            .name = "SCU A-Bus CS0",
            .addressBlockName = "Main",
            .baseAddress = 0x2000000,
            .size = 0x2000000,
            .readFn = MainBusRead,
            .writeFn = MainBusWrite,
            .bgColorFn = MainBusBgColor,
            .paramsFn = nullptr,
        },
        {
            .name = "SCU A-Bus CS1",
            .addressBlockName = "Main",
            .baseAddress = 0x4000000,
            .size = 0x1000000,
            .readFn = MainBusRead,
            .writeFn = MainBusWrite,
            .bgColorFn = MainBusBgColor,
            .paramsFn = nullptr,
        },
        {
            .name = "SCU A-Bus CS2",
            .addressBlockName = "Main",
            .baseAddress = 0x5800000,
            .size = 0x100000,
            .readFn = MainBusRead,
            .writeFn = MainBusWrite,
            .bgColorFn = MainBusBgColor,
            .paramsFn = nullptr,
        },
        {
            .name = "SCU B-Bus",
            .addressBlockName = "Main",
            .baseAddress = 0x5A00000,
            .size = 0x5C0000,
            .readFn = MainBusRead,
            .writeFn = MainBusWrite,
            .bgColorFn = MainBusBgColor,
            .paramsFn = nullptr,
        },
        {
            .name = "68000 Work RAM",
            .addressBlockName = "Main",
            .baseAddress = 0x5A00000,
            .size = 0x80000,
            .readFn = MainBusRead,
            .writeFn = MainBusWrite,
            .bgColorFn = MainBusBgColor,
            .paramsFn = nullptr,
        },
        {
            .name = "SCSP registers",
            .addressBlockName = "Main",
            .baseAddress = 0x5B00000,
            .size = 0x1000,
            .readFn = MainBusRead,
            .writeFn = MainBusWrite,
            .bgColorFn = MainBusBgColor,
            .paramsFn = nullptr,
        },
        {
            .name = "VDP1 VRAM",
            .addressBlockName = "Main",
            .baseAddress = 0x5C00000,
            .size = 0x80000,
            .readFn = MainBusRead,
            .writeFn = MainBusWrite,
            .bgColorFn = MainBusBgColor,
            .paramsFn = nullptr,
        },
        {
            .name = "VDP1 framebuffer",
            .addressBlockName = "Main",
            .baseAddress = 0x5C80000,
            .size = 0x40000,
            .readFn = MainBusRead,
            .writeFn = MainBusWrite,
            .bgColorFn = MainBusBgColor,
            .paramsFn = nullptr,
        },
        {
            .name = "VDP1 registers",
            .addressBlockName = "Main",
            .baseAddress = 0x5D00000,
            .size = 0x20,
            .readFn = MainBusRead,
            .writeFn = MainBusWrite,
            .bgColorFn = MainBusBgColor,
            .paramsFn = nullptr,
        },
        {
            .name = "VDP2 VRAM",
            .addressBlockName = "Main",
            .baseAddress = 0x5E00000,
            .size = 0x80000,
            .readFn = MainBusRead,
            .writeFn = MainBusWrite,
            .bgColorFn = MainBusBgColor,
            .paramsFn = nullptr,
        },
        {
            .name = "VDP2 CRAM",
            .addressBlockName = "Main",
            .baseAddress = 0x5F00000,
            .size = 0x1000,
            .readFn = MainBusRead,
            .writeFn = MainBusWrite,
            .bgColorFn = MainBusBgColor,
            .paramsFn = nullptr,
        },
        {
            .name = "VDP2 registers",
            .addressBlockName = "Main",
            .baseAddress = 0x5FE0000,
            .size = 0x100,
            .readFn = MainBusRead,
            .writeFn = MainBusWrite,
            .bgColorFn = MainBusBgColor,
            .paramsFn = nullptr,
        },
        {
            .name = "High Work RAM",
            .addressBlockName = "Main",
            .baseAddress = 0x6000000,
            .size = 0x100000,
            .readFn = MainBusRead,
            .writeFn = MainBusWrite,
            .bgColorFn = MainBusBgColor,
            .paramsFn = nullptr,
        },
    };

    static constexpr Region kMSH2Regions[] = {
        {
            .name = "MSH2 cached address space",
            .addressBlockName = "MSH2",
            .baseAddress = 0x00000000,
            .size = 0x8000000,
            .readFn = SH2BusRead<true>,
            .writeFn = SH2BusWrite<true>,
            .bgColorFn = SH2BusBgColor<true>,
            .paramsFn = nullptr,
        },
        {
            .name = "MSH2 uncached address space",
            .addressBlockName = "MSH2",
            .baseAddress = 0x20000000,
            .size = 0x8000000,
            .readFn = SH2BusRead<true>,
            .writeFn = SH2BusWrite<true>,
            .bgColorFn = SH2BusBgColor<true>,
            .paramsFn = nullptr,
        },
        {
            .name = "MSH2 cache address array",
            .addressBlockName = "MSH2",
            .baseAddress = 0x60000000,
            .size = 0x400,
            .readFn = SH2BusRead<true>,
            .writeFn = SH2BusWrite<true>,
            .bgColorFn = SH2BusBgColor<true>,
            .paramsFn = nullptr,
        },
        {
            .name = "MSH2 cache data array",
            .addressBlockName = "MSH2",
            .baseAddress = 0xC0000000,
            .size = 0x1000,
            .readFn = SH2BusRead<true>,
            .writeFn = SH2BusWrite<true>,
            .bgColorFn = SH2BusBgColor<true>,
            .paramsFn = nullptr,
        },
        {
            .name = "MSH2 on-chip registers",
            .addressBlockName = "MSH2",
            .baseAddress = 0xFFFFFE00,
            .size = 0x200,
            .readFn = SH2BusRead<true>,
            .writeFn = SH2BusWrite<true>,
            .bgColorFn = SH2BusBgColor<true>,
            .paramsFn = nullptr,
        },
    };

    static constexpr Region kSSH2Regions[] = {
        {
            .name = "SSH2 cached address space",
            .addressBlockName = "SSH2",
            .baseAddress = 0x00000000,
            .size = 0x8000000,
            .readFn = SH2BusRead<false>,
            .writeFn = SH2BusWrite<false>,
            .bgColorFn = SH2BusBgColor<false>,
            .paramsFn = nullptr,
        },
        {
            .name = "SSH2 uncached address space",
            .addressBlockName = "SSH2",
            .baseAddress = 0x20000000,
            .size = 0x8000000,
            .readFn = SH2BusRead<false>,
            .writeFn = SH2BusWrite<false>,
            .bgColorFn = SH2BusBgColor<false>,
            .paramsFn = nullptr,
        },
        {
            .name = "SSH2 cache address array",
            .addressBlockName = "SSH2",
            .baseAddress = 0x60000000,
            .size = 0x400,
            .readFn = SH2BusRead<false>,
            .writeFn = SH2BusWrite<false>,
            .bgColorFn = SH2BusBgColor<false>,
            .paramsFn = nullptr,
        },
        {
            .name = "SSH2 cache data array",
            .addressBlockName = "SSH2",
            .baseAddress = 0xC0000000,
            .size = 0x1000,
            .readFn = SH2BusRead<false>,
            .writeFn = SH2BusWrite<false>,
            .bgColorFn = SH2BusBgColor<false>,
            .paramsFn = nullptr,
        },
        {
            .name = "SSH2 on-chip registers",
            .addressBlockName = "SSH2",
            .baseAddress = 0xFFFFFE00,
            .size = 0x200,
            .readFn = SH2BusRead<false>,
            .writeFn = SH2BusWrite<false>,
            .bgColorFn = SH2BusBgColor<false>,
            .paramsFn = nullptr,
        },
    };

    static constexpr RegionGroup kRegionGroups[] = {
        {
            .name = "Main address space",
            .regions = kMainRegions,
        },
        {
            .name = "Master SH-2",
            .regions = kMSH2Regions,
        },
        {
            .name = "Slave SH-2",
            .regions = kSSH2Regions,
        },
    };
};

} // namespace app::ui
