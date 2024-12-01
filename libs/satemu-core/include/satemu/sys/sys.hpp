#pragma once

#include <satemu/core_types.hpp>

#include <satemu/hw/cdblock/cdblock.hpp>
#include <satemu/hw/scsp/scsp.hpp>
#include <satemu/hw/scu/scu.hpp>
#include <satemu/hw/sh2/sh2.hpp>
#include <satemu/hw/smpc/smpc.hpp>
#include <satemu/hw/vdp/vdp1.hpp>
#include <satemu/hw/vdp/vdp2.hpp>

namespace satemu {

struct Saturn {
    Saturn();

    void Reset(bool hard);

    // TODO: consider moving this to a system
    // TODO: implement RunFrame
    void Step();

    SH2Bus sh2bus;

    sh2::SH2 masterSH2;
    sh2::SH2 slaveSH2;

    scu::SCU SCU;
    smpc::SMPC SMPC;
    scsp::SCSP SCSP;
    cdblock::CDBlock CDBlock;
    vdp1::VDP1 VDP1;
    vdp2::VDP2 VDP2;

    // TODO: introduce "systems"
    // - logical groupings of components with complex logic
    // - examples:
    //   - SoundSystem: uses SCSP, SCU (for interrupts probably), maybe SH2Bus?
    //   - VideoSystem: VDP1, VDP2, SCU (interrupts), ...
    //   - InputSystem: SMPC, anything else?
    //   - CDSystem: CDBlock, SCU?
    //   - EmulatorSystem? something to run and step through code
    // - some components might still require references
    //   - e.g. SH2 still needs to be able to talk to an SH2Bus
    //     - move the emulation logic to an SH2System
    //       - could even include *both* SH2s in one system, saving a reference
};

} // namespace satemu
