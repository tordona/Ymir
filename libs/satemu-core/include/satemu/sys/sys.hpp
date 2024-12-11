#pragma once

#include <satemu/hw/cdblock/cdblock.hpp>
#include <satemu/hw/scsp/scsp.hpp>
#include <satemu/hw/scu/scu.hpp>
#include <satemu/hw/sh2/sh2_block.hpp>
#include <satemu/hw/smpc/smpc.hpp>
#include <satemu/hw/vdp/vdp1.hpp>
#include <satemu/hw/vdp/vdp2.hpp>

#include <satemu/media/disc.hpp>

namespace satemu {

struct Saturn {
    Saturn();

    void Reset(bool hard);

    // -------------------------------------------------------------------------
    // Convenience methods

    void LoadIPL(std::span<uint8, sh2::kIPLSize> ipl);

    void LoadDisc(media::Disc &&disc);
    void EjectDisc();
    void OpenTray();
    void CloseTray();

    // TODO: consider moving this to a system
    // TODO: implement RunFrame
    void Step();

    // -------------------------------------------------------------------------
    // Components

    sh2::SH2Block SH2;
    scu::SCU SCU;
    vdp1::VDP1 VDP1;
    vdp2::VDP2 VDP2;
    smpc::SMPC SMPC;
    scsp::SCSP SCSP;
    cdblock::CDBlock CDBlock;

private:
    // -------------------------------------------------------------------------
    // Cycle counting

    uint64 m_scspCycles; // SCSP, M68K
    uint64 m_cdbCycles;  // CD Block, SH1, SMPC (1/5)
};

} // namespace satemu
