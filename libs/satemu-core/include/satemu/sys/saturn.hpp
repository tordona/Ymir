#pragma once

#include <satemu/core/configuration.hpp>
#include <satemu/core/scheduler.hpp>

#include <satemu/state/state.hpp>

#include "memory.hpp"
#include "system.hpp"
#include "system_features.hpp"

#include <satemu/hw/cart/cart.hpp>
#include <satemu/hw/cart/cart_slot.hpp>
#include <satemu/hw/cdblock/cdblock.hpp>
#include <satemu/hw/scsp/scsp.hpp>
#include <satemu/hw/scu/scu.hpp>
#include <satemu/hw/sh2/sh2.hpp>
#include <satemu/hw/smpc/smpc.hpp>
#include <satemu/hw/vdp/vdp.hpp>

#include <satemu/sys/sys_ops.hpp>

#include <satemu/media/disc.hpp>

#include <memory>

namespace satemu {

struct Saturn : sys::ISystemOperations {
    Saturn();

    void Reset(bool hard); // Does a soft or hard reset of the system
    void FactoryReset();   // Erases SMPC settings and does a hard reset

    config::sys::VideoStandard GetVideoStandard() const {
        return configuration.system.videoStandard;
    }

    void SetVideoStandard(config::sys::VideoStandard videoStandard) {
        configuration.system.videoStandard = videoStandard;
    }

    sys::ClockSpeed GetClockSpeed() const;
    void SetClockSpeed(sys::ClockSpeed clockSpeed);

    const sys::ClockRatios &GetClockRatios() const;

    // Loads the specified IPL ROM image.
    void LoadIPL(std::span<uint8, sys::kIPLSize> ipl);

    // Inserts a cartridge into the cartridge slot.
    template <typename T, typename... Args>
        requires std::derived_from<T, cart::BaseCartridge>
    void InsertCartridge(Args &&...args) {
        SCU.InsertCartridge<T>(std::forward<Args>(args)...);
    }

    // Removes the cartridge from the cartridge slot.
    void RemoveCartridge() {
        SCU.RemoveCartridge();
    }

    // Returns a reference to the inserted cartridge.
    [[nodiscard]] cart::BaseCartridge &GetCartridge() {
        return SCU.GetCartridge();
    }

    void LoadDisc(media::Disc &&disc);
    void EjectDisc();
    void OpenTray();
    void CloseTray();
    bool IsTrayOpen() const;

    void AutodetectRegion(media::AreaCode areaCodes);

    // Enables or disables debug tracing on hot paths, which is required for certain debugging features to work, such as
    // breakpoints, watchpoints, and instruction and memory traces.
    //
    // Enabling this option incurs a noticeable performance penalty. It is disabled by default to ensure optimal
    // performance when those features are not needed.
    //
    // Disabling debug tracing also detaches all tracers from all components.
    void EnableDebugTracing(bool enable);

    bool IsDebugTracingEnabled() const {
        return m_systemFeatures.enableDebugTracing;
    }

    // Enables or disables SH2 cache emulation.
    //
    // Most games work fine without this. Enable it to improve accuracy and compatibility with specific games.
    //
    // Enabling this option incurs a small performance penalty and purges all SH2 caches.
    void EnableSH2CacheEmulation(bool enable) {
        configuration.system.emulateSH2Cache = enable;
    }

    bool IsSH2CacheEmulationEnabled() const {
        return configuration.system.emulateSH2Cache;
    }

    // Runs the emulator until the end of the current frame.
    void RunFrame() {
        (this->*m_runFrameFn)();
    }

    // Detaches tracers from all components.
    void DetachAllTracers() {
        masterSH2.UseTracer(nullptr);
        slaveSH2.UseTracer(nullptr);
        SCU.UseTracer(nullptr);
    }

    // -------------------------------------------------------------------------
    // Configuration

    core::Configuration configuration;

    // -------------------------------------------------------------------------
    // Save states

    void SaveState(state::State &state) const;
    bool LoadState(state::State &state);

private:
    // Runs the emulator until the end of the current frame
    template <bool debug, bool enableSH2Cache>
    void RunFrameImpl();

    // Runs the emulator until the next scheduled event
    template <bool debug, bool enableSH2Cache>
    void Run();

    using RunFrameFn = void (Saturn::*)();
    RunFrameFn m_runFrameFn;

    void UpdateRunFrameFn();

    // -------------------------------------------------------------------------
    // Cycle counting
    // NOTE: Scheduler must be initialized before other components as they use it to register events

    core::Scheduler m_scheduler;

    // -------------------------------------------------------------------------
    // Internal configuration

    std::vector<media::AreaCode> m_preferredRegionOrder;
    void UpdatePreferredRegionOrder(std::span<const config::sys::Region> regions);

    void UpdateSH2CacheEmulation(bool enabled);
    void UpdateVideoStandard(config::sys::VideoStandard videoStandard);

    // -------------------------------------------------------------------------
    // Global components and state

    sys::System m_system;
    sys::SystemFeatures m_systemFeatures;

public:
    // -------------------------------------------------------------------------
    // Components

    sys::SystemMemory mem;    // IPL ROM, low and high WRAM, internal backup memory
    sys::Bus mainBus;         // Primary system bus
    sh2::SH2 masterSH2;       // Master SH-2
    sh2::SH2 slaveSH2;        // Slave SH-2
    bool slaveSH2Enabled;     // Slave SH-2 enable flag
    scu::SCU SCU;             // SCU and its DSP, and the cartridge slot
    vdp::VDP VDP;             // VDP1 and VDP2
    smpc::SMPC SMPC;          // SMPC and input devices
    scsp::SCSP SCSP;          // SCSP and its DSP, and MC68EC000 CPU
    cdblock::CDBlock CDBlock; // CD block and media

private:
    // -------------------------------------------------------------------------
    // System operations (SMPC) - sys::ISystemOperations implementation

    bool GetNMI() const final;
    void RaiseNMI() final;

    void EnableAndResetSlaveSH2() final;
    void DisableSlaveSH2() final;

    void EnableAndResetM68K() final;
    void DisableM68K() final;

    void SoftResetSystem() final;      // Soft resets the entire system
    void ClockChangeSoftReset() final; // Soft resets VDP, SCU and SCSP after a clock change
};

} // namespace satemu
