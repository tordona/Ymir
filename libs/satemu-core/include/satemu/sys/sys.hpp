#pragma once

#include <satemu/hw/cdblock/cdblock.hpp>
#include <satemu/hw/scsp/scsp.hpp>
#include <satemu/hw/scu/scu.hpp>
#include <satemu/hw/sh2/sh2.hpp>
#include <satemu/hw/smpc/smpc.hpp>
#include <satemu/hw/vdp/vdp1.hpp>
#include <satemu/hw/vdp/vdp2.hpp>

#include <satemu/sys/sh2_sys.hpp>
#include <satemu/sys/video_sys.hpp>

namespace satemu {

struct Saturn {
    Saturn();

    void Reset(bool hard);

    void LoadIPL(std::span<uint8, kIPLSize> ipl);

    // TODO: consider moving this to a system
    // TODO: implement RunFrame
    void Step();

    // -------------------------------------------------------------------------
    // Components

    scu::SCU SCU;
    smpc::SMPC SMPC;
    scsp::SCSP SCSP;
    cdblock::CDBlock CDBlock;

    // -------------------------------------------------------------------------
    // Systems
    //
    // Systems are logical groupings of components with complex logic and interactions between them.
    //
    // Each system owns one or more components and may optionally connect with other systems to perform more complex
    // interactions such as triggering interrupts.

    sys::SH2System sysSH2;     // owns SH2
    sys::VideoSystem sysVideo; // owns VDP1 and VDP2, talks to SCU system

    // TODO: implement more systems
    // - examples:
    //   - SMPCSystem: owns SMPC, talks to nearly all other systems (SH2s, the M68K in the SCSP, etc.)
    //   - SoundSystem: owns SCSP, talks to SCU system? (for interrupts)
    //   - CDSystem: owns CDBlock, talks to SCU system? (for interrupts)
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
