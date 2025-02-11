#pragma once

#include <satemu/core/scheduler.hpp>

#include "system.hpp"

#include <satemu/hw/cdblock/cdblock.hpp>
#include <satemu/hw/scsp/scsp.hpp>
#include <satemu/hw/scu/scu.hpp>
#include <satemu/hw/sh2/sh2_block.hpp>
#include <satemu/hw/smpc/smpc.hpp>
#include <satemu/hw/vdp/vdp.hpp>

#include <satemu/media/disc.hpp>

namespace satemu {

struct Saturn {
    Saturn();

    void Reset(bool hard);

    void SetVideoStandard(sys::VideoStandard videoStandard);
    void SetClockSpeed(sys::ClockSpeed clockSpeed);

    // -------------------------------------------------------------------------
    // Convenience methods

    void LoadIPL(std::span<uint8, sh2::kIPLSize> ipl);

    void LoadDisc(media::Disc &&disc);
    void EjectDisc();
    void OpenTray();
    void CloseTray();

    void RunFrame();
    void Step(); // FIXME: misnomer -- actually steps until next scheduled event

private:
    // -------------------------------------------------------------------------
    // Cycle counting

    core::Scheduler m_scheduler;

    // -------------------------------------------------------------------------
    // Global components and state

    sys::System m_system;

    void UpdateClockRatios();

public:
    // -------------------------------------------------------------------------
    // Components

    sh2::SH2Block SH2;        // Master and slave SH-2 CPUs
    scu::SCU SCU;             // SCU and its DSP
    vdp::VDP VDP;             // VDP1 and VDP2
    smpc::SMPC SMPC;          // SMPC and input devices
    scsp::SCSP SCSP;          // SCSP and MC68EC000 CPU
    cdblock::CDBlock CDBlock; // CD block and media
};

} // namespace satemu
