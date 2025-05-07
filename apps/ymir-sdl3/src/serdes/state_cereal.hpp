#pragma once

#include <ymir/state/state.hpp>

#include <cereal/types/array.hpp>
#include <cereal/types/vector.hpp>

namespace ymir::state {

inline constexpr uint32 kStateVersion = 4;

} // namespace ymir::state

CEREAL_CLASS_VERSION(ymir::state::State, ymir::state::kStateVersion);

namespace ymir::state {

template <class Archive>
void serialize(Archive &ar, State &s, const uint32 version) {
    if (version == 0 || version > ymir::state::kStateVersion) {
        return;
    }

    // All states that haven't had revisions
    ar(s.scheduler);
    ar(s.system);
    ar(s.msh2);
    ar(s.ssh2);

    if (version == 1) {
        auto scu1 = std::make_unique<v1::SCUState>();
        auto scu2 = std::make_unique<v2::SCUState>();
        ar(*scu1);
        scu2->Upgrade(*scu1);
        s.scu.Upgrade(*scu2);
    } else if (version <= 3) {
        auto scu = std::make_unique<v3::SCUState>();
        ar(*scu);
        s.scu.Upgrade(*scu);
    } else {
        ar(s.scu);
    }

    ar(s.smpc);
    ar(s.vdp);

    if (version <= 2) {
        auto scsp2 = std::make_unique<v2::SCSPState>();
        auto scsp3 = std::make_unique<v3::SCSPState>();
        ar(*scsp2);
        scsp3->Upgrade(*scsp2);
        s.scsp.Upgrade(*scsp3);
    } else if (version == 3) {
        auto scsp = std::make_unique<v3::SCSPState>();
        ar(*scsp);
        s.scsp.Upgrade(*scsp);
    } else {
        ar(s.scsp);
    }

    ar(s.cdblock);
}

// ---------------------------------------------------------------------------------------------------------------------
// Version 1

namespace v1 {

    template <class Archive>
    void serialize(Archive &ar, SchedulerState &s) {
        ar(s.currCount, s.events);
    }

    template <class Archive>
    void serialize(Archive &ar, SchedulerState::EventState &s) {
        ar(s.target, s.countNumerator, s.countDenominator, s.id);
    }

    template <class Archive>
    void serialize(Archive &ar, SystemState &s) {
        ar(s.videoStandard, s.clockSpeed);
        ar(s.slaveSH2Enabled);
        ar(s.iplRomHash);
        ar(s.WRAMLow, s.WRAMHigh);
    }

    template <class Archive>
    void serialize(Archive &ar, SH2State &s) {
        ar(s.R, s.PC, s.PR, s.MACL, s.MACH, s.SR, s.GBR, s.VBR);
        ar(s.delaySlot, s.delaySlotTarget);
        ar(s.bsc, s.dmac, s.wdt, s.divu, s.frt, s.intc, s.cache, s.SBYCR);
    }

    template <class Archive>
    void serialize(Archive &ar, SH2State::BSC &s) {
        ar(s.BCR1, s.BCR2, s.WCR, s.MCR, s.RTCSR, s.RTCNT, s.RTCOR);
    }

    template <class Archive>
    void serialize(Archive &ar, SH2State::DMAC &s) {
        ar(s.DMAOR, s.channels);
    }

    template <class Archive>
    void serialize(Archive &ar, SH2State::DMAC::Channel &s) {
        ar(s.SAR, s.DAR, s.TCR, s.CHCR, s.DRCR);
    }

    template <class Archive>
    void serialize(Archive &ar, SH2State::WDT &s) {
        ar(s.WTCSR, s.WTCNT, s.RSTCSR, s.cycleCount);
    }

    template <class Archive>
    void serialize(Archive &ar, SH2State::DIVU &s) {
        ar(s.DVSR, s.DVDNT, s.DVCR, s.DVDNTH, s.DVDNTL, s.DVDNTUH, s.DVDNTUL);
    }

    template <class Archive>
    void serialize(Archive &ar, SH2State::FRT &s) {
        ar(s.TIER, s.FTCSR, s.FRC, s.OCRA, s.OCRB, s.TCR, s.TOCR, s.ICR, s.TEMP, s.cycleCount);
    }

    template <class Archive>
    void serialize(Archive &ar, SH2State::INTC &s) {
        ar(s.ICR, s.levels, s.vectors, s.pendingSource, s.pendingLevel, s.NMI, s.extVec);
    }

    template <class Archive>
    void serialize(Archive &ar, SH2State::Cache &s) {
        ar(s.CCR, s.entries, s.lru);
    }

    template <class Archive>
    void serialize(Archive &ar, SH2State::Cache::Entry &s) {
        ar(s.tags, s.lines);
    }

    template <class Archive>
    void serialize(Archive &ar, SCUState &s) {
        ar(s.dma, s.dsp);
        ar(s.cartType, s.dramCartData);
        ar(s.intrMask, s.intrStatus, s.abusIntrAck);
        ar(s.timer0Counter, s.timer0Compare);
        ar(s.timer1Reload, s.timer1Enable, s.timer1Mode);
        ar(s.wramSizeSelect);
    }

    template <class Archive>
    void serialize(Archive &ar, SCUDMAState &s) {
        ar(s.srcAddr, s.dstAddr, s.xferCount);
        ar(s.srcAddrInc, s.dstAddrInc, s.updateSrcAddr, s.updateDstAddr);
        ar(s.enabled, s.active, s.indirect, s.trigger, s.start);
        ar(s.currSrcAddr, s.currDstAddr, s.currXferCount);
        ar(s.currSrcAddrInc, s.currDstAddrInc);
        ar(s.currIndirectSrc, s.endIndirect);
    }

    template <class Archive>
    void serialize(Archive &ar, SCUDSPState &s) {
        ar(s.programRAM, s.dataRAM);
        ar(s.programExecuting, s.programPaused, s.programEnded, s.programStep);
        ar(s.PC, s.dataAddress);
        ar(s.nextPC, s.jmpCounter);
        ar(s.sign, s.zero, s.carry, s.overflow);
        ar(s.CT, s.ALU, s.AC, s.P, s.RX, s.RY, s.LOP, s.TOP);
        ar(s.dmaRun, s.dmaToD0, s.dmaHold, s.dmaCount, s.dmaSrc, s.dmaDst);
        ar(s.dmaReadAddr, s.dmaWriteAddr, s.dmaAddrInc);
    }

    template <class Archive>
    void serialize(Archive &ar, SMPCState &s) {
        ar(s.IREG, s.OREG, s.COMREG, s.SR, s.SF);
        ar(s.PDR1, s.PDR2, s.DDR1, s.DDR2, s.IOSEL, s.EXLE);
        ar(s.intback);
        ar(s.busValue, s.resetDisable);
        ar(s.rtcTimestamp, s.rtcSysClockCount);
    }

    template <class Archive>
    void serialize(Archive &ar, SMPCState::INTBACK &s) {
        ar(s.getPeripheralData, s.optimize, s.port1mode, s.port2mode);
        ar(s.report, s.reportOffset, s.inProgress);
    }

    template <class Archive>
    void serialize(Archive &ar, VDPState &s) {
        ar(s.VRAM1, s.VRAM2, s.CRAM, s.spriteFB, s.displayFB);
        ar(s.regs1, s.regs2);
        ar(s.HPhase, s.VPhase, s.VCounter);
        ar(s.renderer);
    }

    template <class Archive>
    void serialize(Archive &ar, VDPState::VDP1RegsState &s) {
        ar(s.TVMR, s.FBCR, s.PTMR);
        ar(s.EWDR, s.EWLR, s.EWRR, s.EDSR);
        ar(s.LOPR, s.COPR);
        ar(s.MODR);
        ar(s.manualSwap, s.manualErase);
    }

    template <class Archive>
    void serialize(Archive &ar, VDPState::VDP2RegsState &s) {
        ar(s.TVMD, s.EXTEN, s.TVSTAT, s.VRSIZE, s.HCNT, s.VCNT, s.RAMCTL);
        ar(s.CYCA0L, s.CYCA0U, s.CYCA1L, s.CYCA1U, s.CYCB0L, s.CYCB0U, s.CYCB1L, s.CYCB1U);
        ar(s.BGON);
        ar(s.MZCTL);
        ar(s.SFSEL, s.SFCODE);
        ar(s.CHCTLA, s.CHCTLB);
        ar(s.BMPNA, s.BMPNB);
        ar(s.PNCNA, s.PNCNB, s.PNCNC, s.PNCND, s.PNCR);
        ar(s.PLSZ);
        ar(s.MPOFN, s.MPOFR);
        ar(s.MPABN0, s.MPCDN0, s.MPABN1, s.MPCDN1, s.MPABN2, s.MPCDN2, s.MPABN3, s.MPCDN3);
        ar(s.MPABRA, s.MPCDRA, s.MPEFRA, s.MPGHRA, s.MPIJRA, s.MPKLRA, s.MPMNRA, s.MPOPRA);
        ar(s.MPABRB, s.MPCDRB, s.MPEFRB, s.MPGHRB, s.MPIJRB, s.MPKLRB, s.MPMNRB, s.MPOPRB);
        ar(s.SCXIN0, s.SCXDN0, s.SCYIN0, s.SCYDN0, s.ZMXIN0, s.ZMXDN0, s.ZMYIN0, s.ZMYDN0);
        ar(s.SCXIN1, s.SCXDN1, s.SCYIN1, s.SCYDN1, s.ZMXIN1, s.ZMXDN1, s.ZMYIN1, s.ZMYDN1);
        ar(s.SCXIN2, s.SCYIN2);
        ar(s.SCXIN3, s.SCYIN3);
        ar(s.ZMCTL, s.SCRCTL);
        ar(s.VCSTAU, s.VCSTAL);
        ar(s.LSTA0U, s.LSTA0L, s.LSTA1U, s.LSTA1L);
        ar(s.LCTAU, s.LCTAL);
        ar(s.BKTAU, s.BKTAL);
        ar(s.RPMD, s.RPRCTL, s.KTCTL, s.KTAOF);
        ar(s.OVPNRA, s.OVPNRB);
        ar(s.RPTAU, s.RPTAL);
        ar(s.WPSX0, s.WPSY0, s.WPEX0, s.WPEY0);
        ar(s.WPSX1, s.WPSY1, s.WPEX1, s.WPEY1);
        ar(s.WCTLA, s.WCTLB, s.WCTLC, s.WCTLD);
        ar(s.LWTA0U, s.LWTA0L, s.LWTA1U, s.LWTA1L);
        ar(s.SPCTL, s.SDCTL);
        ar(s.CRAOFA, s.CRAOFB);
        ar(s.LNCLEN);
        ar(s.SFPRMD);
        ar(s.CCCTL, s.SFCCMD);
        ar(s.PRISA, s.PRISB, s.PRISC, s.PRISD, s.PRINA, s.PRINB, s.PRIR);
        ar(s.CCRSA, s.CCRSB, s.CCRSC, s.CCRSD, s.CCRNA, s.CCRNB, s.CCRR);
        ar(s.CCRLB);
        ar(s.CLOFEN, s.CLOFSL);
        ar(s.COAR, s.COAG, s.COAB);
        ar(s.COBR, s.COBG, s.COBB);
    }

    template <class Archive>
    void serialize(Archive &ar, VDPState::VDPRendererState &s) {
        ar(s.vdp1State);
        ar(s.normBGLayerStates);
        ar(s.rotParamStates);
        ar(s.lineBackLayerState);
        ar(s.displayFB);
        ar(s.vdp1Done);
    }

    template <class Archive>
    void serialize(Archive &ar, VDPState::VDPRendererState::VDP1RenderState &s) {
        ar(s.sysClipH, s.sysClipV);
        ar(s.userClipX0, s.userClipY0, s.userClipX1, s.userClipY1);
        ar(s.localCoordX, s.localCoordY);
        ar(s.rendering);
        ar(s.cycleCount);
    }

    template <class Archive>
    void serialize(Archive &ar, VDPState::VDPRendererState::NormBGLayerState &s) {
        ar(s.fracScrollX, s.fracScrollY, s.scrollIncH);
        ar(s.lineScrollTableAddress);
        ar(s.mosaicCounterY);
    }

    template <class Archive>
    void serialize(Archive &ar, VDPState::VDPRendererState::RotationParamState &s) {
        ar(s.pageBaseAddresses);
        ar(s.scrX, s.scrY);
        ar(s.KA);
    }

    template <class Archive>
    void serialize(Archive &ar, VDPState::VDPRendererState::LineBackLayerState &s) {
        ar(s.lineColor);
        ar(s.backColor);
    }

    template <class Archive>
    void serialize(Archive &ar, M68KState &s) {
        ar(s.DA, s.SP_swap, s.PC, s.SR);
        ar(s.prefetchQueue, s.extIntrLevel);
    }

    template <class Archive>
    void serialize(Archive &ar, SCSPState &s) {
        ar(s.WRAM);
        ar(s.cddaBuffer, s.cddaReadPos, s.cddaWritePos, s.cddaReady);
        ar(s.m68k, s.m68kSpilloverCycles, s.m68kEnabled);
        ar(s.slots);
        ar(s.MVOL, s.DAC18B, s.MEM4MB, s.MSLC);
        ar(s.timers);
        ar(s.MCIEB, s.MCIPD);
        ar(s.SCIEB, s.SCIPD);
        ar(s.DEXE, s.DDIR, s.DGATE, s.DMEA, s.DRGA, s.DTLG);
        ar(s.SOUS, s.soundStackIndex);
        ar(s.dsp);
        ar(s.m68kCycles, s.sampleCycles, s.sampleCounter);
        ar(s.egCycle, s.egStep);
        ar(s.lfsr);
    }

    template <class Archive>
    void serialize(Archive &ar, SCSPDSP &s) {
        ar(s.MPRO, s.TEMP, s.MEMS, s.COEF, s.MADRS, s.MIXS, s.EFREG, s.EXTS);
        ar(s.RBP, s.RBL);
        ar(s.INPUTS);
        ar(s.SFT_REG, s.FRC_REG, s.Y_REG, s.ADRS_REG);
        ar(s.MDEC_CT);
        ar(s.readPending, s.readNOFL, s.readValue);
        ar(s.writePending, s.writeValue);
        ar(s.readWriteAddr);
    }

    template <class Archive>
    void serialize(Archive &ar, SCSPSlotState &s) {
        ar(s.SA, s.LSA, s.LEA, s.PCM8B, s.KYONB);
        ar(s.LPCTL);
        ar(s.SSCTL);
        ar(s.AR, s.D1R, s.D2R, s.RR, s.DL);
        ar(s.KRS, s.EGHOLD, s.LPSLNK);
        ar(s.MDL, s.MDXSL, s.MDYSL, s.STWINH);
        ar(s.TL, s.SDIR);
        ar(s.OCT, s.FNS);
        ar(s.LFORE, s.LFOF, s.ALFOS, s.PLFOS, s.ALFOWS, s.PLFOWS);
        ar(s.IMXL, s.ISEL, s.DISDL, s.DIPAN);
        ar(s.EFSDL, s.EFPAN);
        ar(s.active);
        ar(s.egState);
        ar(s.egLevel);
        ar(s.sampleCount, s.currAddress, s.currSample, s.currPhase, s.reverse, s.crossedLoopStart);
        ar(s.lfoCycles, s.lfoStep);
        ar(s.sample1, s.sample2, s.output);
    }

    template <class Archive>
    void serialize(Archive &ar, SCSPTimer &s) {
        ar(s.incrementInterval);
        ar(s.reload);
        ar(s.doReload);
        ar(s.counter);
    }

    template <class Archive>
    void serialize(Archive &ar, CDBlockState &s) {
        ar(s.discHash);
        ar(s.CR, s.HIRQ, s.HIRQMASK);
        ar(s.status);
        ar(s.readyForPeriodicReports);
        ar(s.currDriveCycles, s.targetDriveCycles);
        ar(s.playStartParam, s.playEndParam, s.playRepeatParam, s.scanDirection, s.scanCounter);
        ar(s.playStartPos, s.playEndPos, s.playMaxRepeat, s.playFile, s.bufferFullPause);
        ar(s.readSpeed);
        ar(s.discAuthStatus, s.mpegAuthStatus);
        ar(s.xferType, s.xferPos, s.xferLength, s.xferCount, s.xferBuffer, s.xferBufferPos);
        ar(s.xferSectorPos, s.xferSectorEnd, s.xferPartition);
        ar(s.xferSubcodeFrameAddress, s.xferSubcodeGroup);
        ar(s.xferExtraCount);
        ar(s.buffers, s.scratchBuffer);
        ar(s.filters);
        ar(s.cdDeviceConnection, s.lastCDWritePartition);
        ar(s.calculatedPartitionSize);
        ar(s.getSectorLength, s.putSectorLength);
        ar(s.processingCommand);
    }

    template <class Archive>
    void serialize(Archive &ar, CDBlockState::StatusState &s) {
        ar(s.statusCode);
        ar(s.frameAddress);
        ar(s.flags);
        ar(s.repeatCount);
        ar(s.controlADR);
        ar(s.track);
        ar(s.index);
    }

    template <class Archive>
    void serialize(Archive &ar, CDBlockState::BufferState &s) {
        ar(s.data, s.size);
        ar(s.frameAddress);
        ar(s.fileNum, s.chanNum, s.submode, s.codingInfo);
        ar(s.partitionIndex);
    }

    template <class Archive>
    void serialize(Archive &ar, CDBlockState::FilterState &s) {
        ar(s.startFrameAddress, s.frameAddressCount);
        ar(s.mode);
        ar(s.fileNum, s.chanNum);
        ar(s.submodeMask, s.submodeValue);
        ar(s.codingInfoMask, s.codingInfoValue);
        ar(s.trueOutput, s.falseOutput);
    }

} // namespace v1

// ---------------------------------------------------------------------------------------------------------------------
// Version 2

namespace v2 {

    template <class Archive>
    void serialize(Archive &ar, SCUState &s) {
        ar(s.dma, s.dsp);
        ar(s.cartType, s.dramCartData);
        ar(s.intrMask, s.intrStatus, s.abusIntrAck /*, s.intrPending*/);
        ar(s.timer0Counter, s.timer0Compare);
        ar(s.timer1Reload, s.timer1Enable, s.timer1Mode);
        ar(s.wramSizeSelect);
    }

} // namespace v2

// ---------------------------------------------------------------------------------------------------------------------
// Version 3

namespace v3 {

    template <class Archive>
    void serialize(Archive &ar, SCSPState &s) {
        ar(s.WRAM);
        ar(s.cddaBuffer, s.cddaReadPos, s.cddaWritePos, s.cddaReady);
        ar(s.m68k, s.m68kSpilloverCycles, s.m68kEnabled);
        ar(s.slots, s.KYONEX);
        ar(s.MVOL, s.DAC18B, s.MEM4MB, s.MSLC);
        ar(s.timers);
        ar(s.MCIEB, s.MCIPD);
        ar(s.SCIEB, s.SCIPD);
        ar(s.DEXE, s.DDIR, s.DGATE, s.DMEA, s.DRGA, s.DTLG);
        ar(s.SOUS, s.soundStackIndex);
        ar(s.dsp);
        ar(s.m68kCycles, s.sampleCycles, s.sampleCounter);
        ar(s.egCycle, s.egStep);
        ar(s.lfsr);
    }

    template <class Archive>
    void serialize(Archive &ar, SCSPSlotState &s) {
        ar(s.SA, s.LSA, s.LEA, s.PCM8B, s.KYONB);
        ar(s.SBCTL);
        ar(s.LPCTL);
        ar(s.SSCTL);
        ar(s.AR, s.D1R, s.D2R, s.RR, s.DL);
        ar(s.KRS, s.EGHOLD, s.LPSLNK, s.EGBYPASS);
        ar(s.MDL, s.MDXSL, s.MDYSL, s.STWINH);
        ar(s.TL, s.SDIR);
        ar(s.OCT, s.FNS);
        ar(s.LFORE, s.LFOF, s.ALFOS, s.PLFOS, s.ALFOWS, s.PLFOWS);
        ar(s.IMXL, s.ISEL, s.DISDL, s.DIPAN);
        ar(s.EFSDL, s.EFPAN);
        ar(s.extra0C, s.extra10, s.extra14);
        ar(s.active);
        ar(s.egState);
        ar(s.egLevel);
        ar(s.sampleCount, s.currAddress, s.currSample, s.currPhase, s.nextPhase, s.reverse, s.crossedLoopStart);
        ar(s.lfoCycles, s.lfoStep);
        ar(s.alfoOutput);
        ar(s.sample1, s.sample2, s.output);
    }

} // namespace v3

// ---------------------------------------------------------------------------------------------------------------------
// Version 4

inline namespace v4 {

    template <class Archive>
    void serialize(Archive &ar, SCUState &s) {
        ar(s.dma, s.dsp);
        ar(s.cartType, s.cartData);
        ar(s.intrMask, s.intrStatus, s.abusIntrAck /*, s.intrPending*/);
        ar(s.timer0Counter, s.timer0Compare);
        ar(s.timer1Reload, s.timer1Enable, s.timer1Mode);
        ar(s.wramSizeSelect);
    }

    template <class Archive>
    void serialize(Archive &ar, VDPState &s) {
        ar(s.VRAM1, s.VRAM2, s.CRAM, s.spriteFB, s.displayFB);
        ar(s.regs1, s.regs2);
        ar(s.HPhase, s.VPhase, s.VCounter);
        ar(s.renderer);
    }

    template <class Archive>
    void serialize(Archive &ar, VDPState::VDPRendererState &s) {
        ar(s.vdp1State);
        ar(s.normBGLayerStates);
        ar(s.rotParamStates);
        ar(s.lineBackLayerState);
        ar(s.vertCellScrollInc);
        ar(s.displayFB);
        ar(s.vdp1Done);
    }

    template <class Archive>
    void serialize(Archive &ar, VDPState::VDPRendererState::NormBGLayerState &s) {
        ar(s.fracScrollX, s.fracScrollY, s.scrollIncH);
        ar(s.lineScrollTableAddress);
        ar(s.vertCellScrollOffset);
        ar(s.mosaicCounterY);
    }

    template <class Archive>
    void serialize(Archive &ar, SCSPState &s) {
        ar(s.WRAM);
        ar(s.cddaBuffer, s.cddaReadPos, s.cddaWritePos, s.cddaReady);
        ar(s.m68k, s.m68kSpilloverCycles, s.m68kEnabled);
        ar(s.slots, s.KYONEX);
        ar(s.MVOL, s.DAC18B, s.MEM4MB, s.MSLC);
        ar(s.timers);
        ar(s.MCIEB, s.MCIPD);
        ar(s.SCIEB, s.SCIPD);
        ar(s.DEXE, s.DDIR, s.DGATE, s.DMEA, s.DRGA, s.DTLG);
        ar(s.SOUS, s.soundStackIndex);
        ar(s.dsp);
        ar(s.m68kCycles, s.sampleCycles, s.sampleCounter);
        ar(s.egCycle, s.egStep);
        ar(s.lfsr);
    }

    template <class Archive>
    void serialize(Archive &ar, SCSPSlotState &s) {
        ar(s.SA, s.LSA, s.LEA, s.PCM8B, s.KYONB);
        ar(s.SBCTL);
        ar(s.LPCTL);
        ar(s.SSCTL);
        ar(s.AR, s.D1R, s.D2R, s.RR, s.DL);
        ar(s.KRS, s.EGHOLD, s.LPSLNK, s.EGBYPASS);
        ar(s.MDL, s.MDXSL, s.MDYSL, s.STWINH);
        ar(s.TL, s.SDIR);
        ar(s.OCT, s.FNS);
        ar(s.LFORE, s.LFOF, s.ALFOS, s.PLFOS, s.ALFOWS, s.PLFOWS);
        ar(s.IMXL, s.ISEL, s.DISDL, s.DIPAN);
        ar(s.EFSDL, s.EFPAN);
        ar(s.extra0C, s.extra10, s.extra14);
        ar(s.active);
        ar(s.egState);
        ar(s.egLevel);
        ar(s.sampleCount, s.currSample, s.currPhase, s.nextPhase, s.addressInc, s.reverse, s.crossedLoopStart);
        ar(s.lfoCycles, s.lfoStep);
        ar(s.alfoOutput);
        ar(s.sample1, s.sample2, s.output);
    }

} // namespace v4

} // namespace ymir::state
