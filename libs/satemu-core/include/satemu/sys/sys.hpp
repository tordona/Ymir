#pragma once

#include <satemu/hw/cdblock/cdblock.hpp>
#include <satemu/hw/scsp/scsp.hpp>
#include <satemu/hw/scu/scu.hpp>
#include <satemu/hw/sh2/sh2.hpp>
#include <satemu/hw/smpc/smpc.hpp>
#include <satemu/hw/vdp/vdp1.hpp>
#include <satemu/hw/vdp/vdp2.hpp>

#include <satemu/sys/sh2_sys.hpp>

namespace satemu {

struct Saturn {
    Saturn();

    void Reset(bool hard);

    // TODO: consider moving this to a system
    // TODO: implement RunFrame
    void Step();

    // -------------------------------------------------------------------------
    // Components

    sh2::SH2 SH2;

    scu::SCU SCU;
    smpc::SMPC SMPC;
    scsp::SCSP SCSP;
    cdblock::CDBlock CDBlock;
    vdp1::VDP1 VDP1;
    vdp2::VDP2 VDP2;

    // -------------------------------------------------------------------------
    // Systems
    //
    // Systems are logical groupings of components with complex logic and interactions between them.

    sys::SH2System sysSH2;

    // TODO: implement more systems
    // - examples:
    //   - SoundSystem: uses SCSP, SCU (for interrupts probably), maybe both SH2s?
    //   - VideoSystem: VDP1, VDP2, SCU (interrupts), both SH2s, ...
    //   - InputSystem: SMPC, anything else?
    //   - MgmtSystem: SMPC and all the stuff it resets (SH2s, the M68K in the SCSP, etc.)
    //   - CDSystem: CDBlock, SCU? probably both SH2s for interrupts too...
    // - anything that needs to raise interrupts will very likely need SCU + both SH2s
    //   - might be useful to create a base class called InterruptSystem with common logic
    // - not everything needs to be in a dedicated system
    //   - the Saturn class itself can be considered a "global" system
    //   - Reset() is a global operation, so it makes sense to stay here
    //   - debugging features might live here too
    //   - save states
    //   - high-level control like running and stepping should probably be here too
    // - some components might still require references
    //   - e.g. SH2 still needs to be able to talk to an SH2Bus
    //     - move the emulation logic to an SH2System
    //       - could even include *both* SH2s in one system, saving a reference
};

} // namespace satemu
