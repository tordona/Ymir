#pragma once

#include <satemu/core_types.hpp>

#include <satemu/hw/cdblock/cdblock.hpp>
#include <satemu/hw/scsp/scsp.hpp>
#include <satemu/hw/scu/scu.hpp>
#include <satemu/hw/sh2/sh2.hpp>
#include <satemu/hw/smpc/smpc.hpp>
#include <satemu/hw/vdp/vdp1.hpp>
#include <satemu/hw/vdp/vdp2.hpp>

#include <satemu/util/inline.hpp>

namespace satemu {

class Saturn {
public:
    Saturn();

    void Reset(bool hard);

    void LoadIPL(std::span<uint8, kIPLSize> ipl);

    void Step();

    // TODO: void RunFrame();
    FORCE_INLINE void SetFramebufferCallbacks(vdp2::CBRequestFramebuffer cbRequestFramebuffer,
                                              vdp2::CBFrameComplete cbFrameComplete) {
        m_VDP2.SetCallbacks(cbRequestFramebuffer, cbFrameComplete);
    }

    FORCE_INLINE sh2::SH2 &MasterSH2() {
        return m_masterSH2;
    }

private:
    SH2Bus m_sh2bus;

    sh2::SH2 m_masterSH2;
    sh2::SH2 m_slaveSH2;

    scu::SCU m_SCU;
    smpc::SMPC m_SMPC;
    scsp::SCSP m_SCSP;
    cdblock::CDBlock m_CDBlock;
    vdp1::VDP1 m_VDP1;
    vdp2::VDP2 m_VDP2;
};

} // namespace satemu
