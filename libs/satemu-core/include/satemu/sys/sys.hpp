#pragma once

#include <satemu/hw/cdblock/cdblock.hpp>
#include <satemu/hw/scsp/scsp.hpp>
#include <satemu/hw/scu/scu.hpp>
#include <satemu/hw/sh2/sh2_block.hpp>
#include <satemu/hw/smpc/smpc.hpp>
#include <satemu/hw/vdp/vdp1.hpp>
#include <satemu/hw/vdp/vdp2.hpp>

namespace satemu {

struct Saturn {
    Saturn();

    void Reset(bool hard);

    void LoadIPL(std::span<uint8, sh2::kIPLSize> ipl);

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
};

} // namespace satemu
