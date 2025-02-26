#pragma once

#include <satemu/core/scheduler.hpp>

#include "memory.hpp"
#include "system.hpp"

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

    void Reset(bool hard);
    void FactoryReset(); // Erases SMPC settings and does a hard reset

    sys::VideoStandard GetVideoStandard() const;
    void SetVideoStandard(sys::VideoStandard videoStandard);

    sys::ClockSpeed GetClockSpeed() const;
    void SetClockSpeed(sys::ClockSpeed clockSpeed);

    // -------------------------------------------------------------------------
    // Convenience methods

    void LoadIPL(std::span<uint8, sys::kIPLSize> ipl);

    template <typename T, typename... Args>
        requires std::derived_from<T, cart::BaseCartridge>
    [[nodiscard]] bool InsertCartridge(Args &&...args) {
        return SCU.InsertCartridge<T>(std::forward<Args>(args)...);
    }

    void EjectCartridge() {
        SCU.EjectCartridge();
    }

    void LoadDisc(media::Disc &&disc);
    void EjectDisc();
    void OpenTray();
    void CloseTray();
    bool IsTrayOpen() const;

    // Runs the emulator until the end of the current frame.
    //
    // `debug` enables advanced debug tracing, which may impact performance.
    void RunFrame(bool debug) {
        if (debug) {
            RunFrame<true>();
        } else {
            RunFrame<false>();
        }
    }

private:
    // Runs the emulator until the end of the current frame
    template <bool debug>
    void RunFrame();

    // Runs the emulator until the next scheduled event
    template <bool debug>
    void Run();

    // -------------------------------------------------------------------------
    // Cycle counting
    // NOTE: Scheduler must be initialized before other components as they use it to register events

    core::Scheduler m_scheduler;

    // -------------------------------------------------------------------------
    // Global components and state

    sys::System m_system;

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
