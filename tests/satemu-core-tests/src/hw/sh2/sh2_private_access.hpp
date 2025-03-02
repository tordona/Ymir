#pragma once

#include <satemu/hw/sh2/sh2.hpp>

namespace satemu::sh2 {

struct PrivateAccess {
    static std::array<uint32, 16> &R(SH2 &sh2) {
        return sh2.R;
    }

    static uint32 &PC(SH2 &sh2) {
        return sh2.PC;
    }

    static uint32 &PR(SH2 &sh2) {
        return sh2.PR;
    }

    static RegMAC &MAC(SH2 &sh2) {
        return sh2.MAC;
    }

    static RegSR &SR(SH2 &sh2) {
        return sh2.SR;
    }

    static uint32 &GBR(SH2 &sh2) {
        return sh2.GBR;
    }

    static uint32 &VBR(SH2 &sh2) {
        return sh2.VBR;
    }

    static InterruptController &INTC(SH2 &sh2) {
        return sh2.INTC;
    }

    static void RaiseInterrupt(SH2 &sh2, InterruptSource source) {
        // Set the corresponding signals
        switch (source) {
        case InterruptSource::None: break;
        case InterruptSource::FRT_OVI:
            sh2.FRT.FTCSR.OVF = 1;
            sh2.FRT.TIER.OVIE = 1;
            break;
        case InterruptSource::FRT_OCI:
            sh2.FRT.FTCSR.OCFA = 1;
            sh2.FRT.TIER.OCIAE = 1;
            break;
        case InterruptSource::FRT_ICI:
            sh2.FRT.FTCSR.ICF = 1;
            sh2.FRT.TIER.ICIE = 1;
            break;
        case InterruptSource::SCI_TEI: /*TODO*/ break;
        case InterruptSource::SCI_TXI: /*TODO*/ break;
        case InterruptSource::SCI_RXI: /*TODO*/ break;
        case InterruptSource::SCI_ERI: /*TODO*/ break;
        case InterruptSource::BSC_REF_CMI: /*TODO*/ break;
        case InterruptSource::WDT_ITI:
            sh2.WDT.WTCSR.OVF = 1;
            sh2.WDT.WTCSR.WT_nIT = 0;
            break;
        case InterruptSource::DMAC1_XferEnd:
            sh2.m_dmaChannels[1].xferEnded = true;
            sh2.m_dmaChannels[1].irqEnable = true;
            break;
        case InterruptSource::DMAC0_XferEnd:
            sh2.m_dmaChannels[0].xferEnded = true;
            sh2.m_dmaChannels[0].irqEnable = true;
            break;
        case InterruptSource::DIVU_OVFI:
            sh2.DIVU.DVCR.OVF = 1;
            sh2.DIVU.DVCR.OVFIE = 1;
            break;
        case InterruptSource::IRL: /*nothing to do*/ break;
        case InterruptSource::UserBreak: /*TODO*/ break;
        case InterruptSource::NMI: sh2.INTC.NMI = 1; break;
        }

        sh2.RaiseInterrupt(source);
    }

    static void LowerInterrupt(SH2 &sh2, InterruptSource source) {
        // Clear the corresponding signals
        switch (source) {
        case InterruptSource::None: break;
        case InterruptSource::FRT_OVI: sh2.FRT.FTCSR.OVF = 0; break;
        case InterruptSource::FRT_OCI: sh2.FRT.FTCSR.OCFA = 0; break;
        case InterruptSource::FRT_ICI: sh2.FRT.FTCSR.ICF = 0; break;
        case InterruptSource::SCI_TEI: /*TODO*/ break;
        case InterruptSource::SCI_TXI: /*TODO*/ break;
        case InterruptSource::SCI_RXI: /*TODO*/ break;
        case InterruptSource::SCI_ERI: /*TODO*/ break;
        case InterruptSource::BSC_REF_CMI: /*TODO*/ break;
        case InterruptSource::WDT_ITI: sh2.WDT.WTCSR.OVF = 0; break;
        case InterruptSource::DMAC1_XferEnd: sh2.m_dmaChannels[1].xferEnded = false; break;
        case InterruptSource::DMAC0_XferEnd: sh2.m_dmaChannels[0].xferEnded = false; break;
        case InterruptSource::DIVU_OVFI: sh2.DIVU.DVCR.OVF = 0; break;
        case InterruptSource::IRL: /*nothing to do*/ break;
        case InterruptSource::UserBreak: /*TODO*/ break;
        case InterruptSource::NMI: sh2.INTC.NMI = 0; break;
        }

        sh2.LowerInterrupt(source);
    }

    static bool CheckInterrupts(SH2 &sh2) {
        return sh2.CheckInterrupts();
    }
};

} // namespace satemu::sh2
