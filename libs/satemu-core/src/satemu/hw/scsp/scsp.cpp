#include <satemu/hw/scsp/scsp.hpp>

#include <satemu/hw/scu/scu.hpp>

using namespace satemu::m68k;

namespace satemu::scsp {

SCSP::SCSP(core::Scheduler &scheduler, scu::SCU &scu)
    : m_m68k(*this)
    , m_scu(scu)
    , m_scheduler(scheduler) {

    m_sampleTickEvent = m_scheduler.RegisterEvent(
        core::events::SCSPSample, this, [](core::EventContext &eventContext, void *userContext, uint64 cyclesLate) {
            auto &scsp = *static_cast<SCSP *>(userContext);
            scsp.ProcessSample();
            eventContext.RescheduleFromNow(kCyclesPerSample);
        });

    for (uint32 i = 0; i < 32; i++) {
        m_slots[i].index = i;
    }

    Reset(true);
}

void SCSP::Reset(bool hard) {
    m_WRAM.fill(0);

    m_m68k.Reset(true);
    m_m68kEnabled = false;

    m_m68kCycles = 0;
    m_sampleCounter = 0;

    m_scheduler.ScheduleFromNow(m_sampleTickEvent, 0);

    for (auto &slot : m_slots) {
        slot.Reset();
    }

    m_masterVolume = 0;
    m_mem4MB = false;
    m_dac18Bits = false;

    m_monitorSlotCall = 0;

    for (auto &timer : m_timers) {
        timer.Reset();
    }

    m_scuEnabledInterrupts = 0;
    m_scuPendingInterrupts = 0;
    m_m68kEnabledInterrupts = 0;
    m_m68kPendingInterrupts = 0;
    m_m68kInterruptLevels.fill(0);

    m_dmaExec = false;
    m_dmaXferToMem = false;
    m_dmaGate = false;
    m_dmaMemAddress = 0;
    m_dmaRegAddress = 0;
    m_dmaXferLength = 0;

    m_soundStack.fill(0);
    m_soundStackIndex = 0;

    m_dspProgram.fill({.u64 = 0});
    m_dspTemp.fill(0);
    m_dspSoundMem.fill(0);
    m_dspCoeffs.fill(0);
    m_dspAddrs.fill(0);
    m_dspMixStack.fill(0);
    m_dspEffectOut.fill(0);
    m_dspAudioIn.fill(0);

    m_dspRingBufferLeadAddress = 0;
    m_dspRingBufferLength = 0;
}

void SCSP::Advance(uint64 cycles) {
    if (m_m68kEnabled) {
        m_m68kCycles += cycles;
        while (m_m68kCycles >= 2) {
            // TODO: proper cycle counting
            m_m68k.Step();
            m_m68kCycles -= 2;
        }
    }
}

void SCSP::DumpWRAM(std::ostream &out) {
    out.write((const char *)m_WRAM.data(), m_WRAM.size());
}

void SCSP::SetCPUEnabled(bool enabled) {
    if (m_m68kEnabled != enabled) {
        rootLog.info("{} the MC68EC00 processor", (enabled ? "enabling" : "disabling"));
        if (enabled) {
            m_m68k.Reset(true); // false? does it matter?
        }
        m_m68kEnabled = enabled;
    }
}

void SCSP::SetInterrupt(uint16 intr, bool level) {
    m_m68kPendingInterrupts &= ~(1 << intr);
    m_m68kPendingInterrupts |= level << intr;

    m_scuPendingInterrupts &= ~(1 << intr);
    m_scuPendingInterrupts |= level << intr;
}

void SCSP::UpdateM68KInterrupts() {
    const uint16 baseMask = m_m68kPendingInterrupts & m_m68kEnabledInterrupts;
    uint8 mask = baseMask | (bit::extract<8, 9>(baseMask) ? 0x80 : 0x00);
    // NOTE: interrupts 7-9 share the same level

    // Check all active interrupts in parallel and select maximum level among them.
    //
    // The naïve approach is to iterate over all 8 interrupts, skip unselected interrupts, and update the level if the
    // configured interrupt level is greater than the one found so far. This takes 8 iterations, each one doing several
    // operations (including comparisons and bit manipulation).
    //
    // Luckily, we can use bit manipulation to our advantage. Due to the way the SCILV registers are naturally arranged,
    // we have 3 values containing the bits at positions 2, 1 and 0 of each interrupt. We can make all 8 comparisons in
    // parallel in only 3 iterations by taking advantage of bitwise operations.
    //
    // Suppose we want to pick the largest of four binary numbers: (x) 101, (y) 011, (z) 110, (w) 001.
    // First, let's rearrange the bits to group bits from each position into one word each:
    //          xyzw
    //   bit 2  1010
    //   bit 1  0110
    //   bit 0  1101
    //
    // Now we can take advantage of the fact that a binary digit can only be 0 or 1, meaning that if any bit is 1 in the
    // most significant bit of a number, we know that any other number that has a digit 0 in the same position cannot be
    // larger. We can then exclude these numbers from the search with a simple bitwise AND.
    //
    // Let's go through one iteration of the algorithm. We've already set up the table in a convenient manner to allow
    // parallel calculations. Let's create a bitmask to indicate which numbers we're still checking:
    //
    //   mask   1111
    //
    // We can AND the mask with the most significant bit we're checking to see if any number has that bit set:
    //
    //   bit2   mask
    //   1010 & 1111 = 1010
    //
    // The result is non-zero, meaning that we have at least one number with this bit set. Conveniently, the numbers
    // that don't have the bit set also happen to have cleared their corresponding bits in the result.
    // Because the result is non-zero, the output bit for position 2 is set to 1.
    //
    //   output = 1..
    //
    // We still have two bits to go. Copy the result to the mask and continue checking the next most significant bit:
    //
    //   bit1   mask
    //   0110 & 1010 = 0010
    //
    // Once again, the result is 1; write that to the output at position 1, update the mask and check the final bit:
    //
    //   output = 11.
    //
    //   bit0   mask
    //   1101 & 0010 = 0000
    //
    // Now the result is zero, so the final output bit is 0.
    //
    //   output = 110
    //
    // This indeed matches the biggest number out of the four: (z) 110.

    uint8 level = 0;
    if (m_m68kInterruptLevels[2] & mask) {
        level |= 4;
        mask &= m_m68kInterruptLevels[2];
    }
    if (m_m68kInterruptLevels[1] & mask) {
        level |= 2;
        mask &= m_m68kInterruptLevels[1];
    }
    if (m_m68kInterruptLevels[0] & mask) {
        level |= 1;
    }

    m_m68k.SetExternalInterruptLevel(level);
}

void SCSP::UpdateSCUInterrupts() {
    m_scu.TriggerSoundRequest(m_scuPendingInterrupts & m_scuEnabledInterrupts);
}

void SCSP::ExecuteDMA() {
    while (m_dmaExec) {
        if (m_dmaXferToMem) {
            const uint16 value = ReadReg<uint16>(m_dmaRegAddress);
            dmaLog.debug("Register {:03X} -> Memory {:06X} = {:04X}", m_dmaRegAddress, m_dmaMemAddress, value);
            WriteWRAM<uint16>(m_dmaMemAddress, m_dmaGate ? 0u : value);
        } else {
            const uint16 value = ReadWRAM<uint16>(m_dmaMemAddress);
            dmaLog.debug("Memory {:06X} -> Register {:03X} = {:04X}", m_dmaMemAddress, m_dmaRegAddress, value);
            WriteReg<uint16>(m_dmaRegAddress, m_dmaGate ? 0u : value);
        }
        m_dmaMemAddress = (m_dmaMemAddress + sizeof(uint16)) & 0x7FFFE;
        m_dmaRegAddress = (m_dmaRegAddress + sizeof(uint16)) & 0xFFE;
        m_dmaXferLength--;
        if (m_dmaXferLength == 0) {
            m_dmaExec = false;
            SetInterrupt(kIntrDMATransferEnd, true);

            // Send interrupt signals
            UpdateM68KInterrupts();
            UpdateSCUInterrupts();
        }
    }
}

FORCE_INLINE void SCSP::ProcessSample() {
    // Handle KYONEX
    for (int i = 0; auto &slot : m_slots) {
        if (m_keyOnEx && slot.TriggerKeyOn()) {
            regsLog.debug(
                "Slot {} key {}, start address {:X}, loop {:X}-{:X}, octave {}, FNS 0x{:03X}, EG rates: {} {} {} {}", i,
                (slot.keyOnBit ? "ON" : "OFF"), slot.startAddress, slot.loopStartAddress, slot.loopEndAddress,
                slot.octave, slot.freqNumSwitch, slot.envGen.attackRate, slot.envGen.decay1Rate, slot.envGen.decay2Rate,
                slot.envGen.releaseRate);
        }

        i++;
    }
    m_keyOnEx = false;

    // Process slots
    for (uint32 i = 0; i < 32; i++) {
        SlotProcessStep1(m_slots[i]);
        SlotProcessStep2(m_slots[(i - 1u) & 31]);
        SlotProcessStep3(m_slots[(i - 2u) & 31]);
        SlotProcessStep4(m_slots[(i - 3u) & 31]);
        SlotProcessStep5(m_slots[(i - 4u) & 31]);
        SlotProcessStep6(m_slots[(i - 5u) & 31]);
        SlotProcessStep7(m_slots[(i - 6u) & 31]);

        // TODO: direct mixing straight to final output (slot DISDL, DIPAN)
        // TODO: mix into MIXS DSP input (slot IMXL, ISEL)

        m_soundStackIndex = (m_soundStackIndex + 1) & 63;
    }

    // TODO: copy CDDA data to DSP EXTS (0=left, 1=right)

    // TODO: run DSP

    m_dspMixStack.fill(0);

    // TODO: effect mixing (slot EFSDL, EFPAN) over the EFREG array, then the EXTS array -> straight to output
    // TODO: shift down final output by MVOL^0xF

    // Trigger sample interrupt
    SetInterrupt(kIntrSample, true);

    // Update timers and trigger interrupts
    for (int i = 0; i < 3; i++) {
        auto &timer = m_timers[i];
        const bool trigger = (m_sampleCounter & timer.incrementMask) == 0;
        if (trigger && timer.Tick()) {
            SetInterrupt(kIntrTimerA + i, true);
        }
    }

    // Send interrupt signals
    UpdateM68KInterrupts();
    UpdateSCUInterrupts();
}

FORCE_INLINE void SCSP::SlotProcessStep1(Slot &slot) {
    if (slot.envGen.GetLevel() >= 0x3BF) {
        return;
    }

    // TODO: compute pitch LFO
    const uint32 pitchLFO = (0 << slot.pitchLFOSens) >> 2u;

    slot.IncrementPhase(pitchLFO);
}

FORCE_INLINE void SCSP::SlotProcessStep2(Slot &slot) {
    if (slot.envGen.GetLevel() >= 0x3BF) {
        return;
    }

    sint32 modulation = 0;
    if (slot.modLevel > 0) {
        const uint16 modShift = slot.modLevel ^ 0xF;
        const sint16 xd = m_soundStack[(m_soundStackIndex + slot.modXSelect) & 63];
        const sint16 yd = m_soundStack[(m_soundStackIndex + slot.modYSelect) & 63];
        const sint32 zd = (xd + yd) / 2;
        modulation = zd >> modShift;
    }

    slot.IncrementSampleCounter();
    slot.IncrementAddress(modulation);
}

FORCE_INLINE void SCSP::SlotProcessStep3(Slot &slot) {
    if (slot.envGen.GetLevel() >= 0x3BF) {
        return;
    }

    if (slot.pcm8Bit) {
        slot.output = static_cast<sint8>(ReadWRAM<uint8>(slot.currAddress)) << 8;
    } else {
        slot.output = static_cast<sint16>(ReadWRAM<uint16>(slot.currAddress));
    }
}

FORCE_INLINE void SCSP::SlotProcessStep4(Slot &slot) {
    if (slot.envGen.GetLevel() >= 0x3BF) {
        return;
    }

    // TODO: what does "interpolation" entail here?
    // TODO: what does the ALFO calculation deliver here?

    // TODO: check/fix EG calculation
    slot.envGen.Step();
}

FORCE_INLINE void SCSP::SlotProcessStep5(Slot &slot) {
    if (slot.envGen.GetLevel() >= 0x3BF) {
        slot.output = 0;
        return;
    }

    // TODO: implement level calculation part 1
}

FORCE_INLINE void SCSP::SlotProcessStep6(Slot &slot) {
    // TODO: implement level calculation part 2
}

FORCE_INLINE void SCSP::SlotProcessStep7(Slot &slot) {
    m_soundStack[m_soundStackIndex] = slot.output;
}

ExceptionVector SCSP::AcknowledgeInterrupt(uint8 level) {
    return ExceptionVector::AutoVectorRequest;
}

} // namespace satemu::scsp
