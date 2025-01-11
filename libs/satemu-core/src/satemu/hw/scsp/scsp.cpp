#include <satemu/hw/scsp/scsp.hpp>

#include <satemu/hw/scu/scu.hpp>

using namespace satemu::m68k;

namespace satemu::scsp {

SCSP::SCSP(scu::SCU &scu)
    : m_m68k(*this)
    , m_scu(scu) {
    Reset(true);
}

void SCSP::Reset(bool hard) {
    m_m68k.Reset(true);
    m_WRAM.fill(0);

    m_m68kEnabled = false;

    m_accumSampleCycles = 0;
    m_sampleCounter = 0;

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

    for (auto &sds : m_soundDataStack) {
        sds.fill(0);
    }

    m_dspCoeffs.fill(0);
    m_dspAddrs.fill(0);
}

void SCSP::Advance(uint64 cycles) {
    m_accumSampleCycles += cycles;
    while (m_accumSampleCycles >= kCyclesPerSample) {
        m_accumSampleCycles -= kCyclesPerSample;
        if (m_m68kEnabled && (m_sampleCounter % kCyclesPerM68KCycle) == 0) {
            // TODO: proper cycle counting
            m_m68k.Step();
        }
        m_sampleCounter++;
        ProcessSample();
    }
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

void SCSP::ProcessSample() {
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

ExceptionVector SCSP::AcknowledgeInterrupt(uint8 level) {
    // TODO: does the SCSP allow setting specific vector numbers?
    return ExceptionVector::AutoVectorRequest;
}

} // namespace satemu::scsp
