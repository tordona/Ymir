#include <satemu/hw/scsp/scsp.hpp>

#include <satemu/hw/scu/scu.hpp>

#include <satemu/sys/clocks.hpp>

#include <algorithm>
#include <limits>

using namespace satemu::m68k;

namespace satemu::scsp {

SCSP::SCSP(sys::System &system, core::Scheduler &scheduler, scu::SCU &scu)
    : m_m68k(*this)
    , m_system(system)
    , m_scu(scu)
    , m_scheduler(scheduler)
    , m_dsp(m_WRAM.data()) {

    m_sampleTickEvent = m_scheduler.RegisterEvent(core::events::SCSPSample, this, OnSampleTickEvent);

    for (uint32 i = 0; i < 32; i++) {
        m_slots[i].index = i;
    }

    Reset(true);
}

void SCSP::Reset(bool hard) {
    m_WRAM.fill(0);

    m_cddaBuffer.fill(0);
    m_cddaReadPos = 0;
    m_cddaWritePos = 0;
    m_cddaReady = false;

    m_m68k.Reset(true);
    m_m68kEnabled = false;

    m_m68kCycles = 0;
    m_sampleCounter = 0;
    m_egCycle = 0;
    m_egStep = false;

    m_lfsr = 1;

    if (hard) {
        // TODO: PAL flag
        UpdateClockRatios();

        m_scheduler.ScheduleFromNow(m_sampleTickEvent, kCyclesPerSample);
    }

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

    m_dsp.Reset();
}

void SCSP::MapMemory(sys::Bus &bus) {
    // WRAM
    bus.MapMemory(0x5A0'0000, 0x5AF'FFFF,
                  {
                      .ctx = this,
                      .read8 = [](uint32 address, void *ctx) -> uint8 {
                          return static_cast<SCSP *>(ctx)->ReadWRAM<uint8>(address);
                      },
                      .read16 = [](uint32 address, void *ctx) -> uint16 {
                          return static_cast<SCSP *>(ctx)->ReadWRAM<uint16>(address);
                      },
                      .read32 = [](uint32 address, void *ctx) -> uint32 {
                          uint32 value = static_cast<SCSP *>(ctx)->ReadWRAM<uint16>(address + 0) << 16u;
                          value |= static_cast<SCSP *>(ctx)->ReadWRAM<uint16>(address + 2) << 0u;
                          return value;
                      },
                      .write8 = [](uint32 address, uint8 value,
                                   void *ctx) { static_cast<SCSP *>(ctx)->WriteWRAM<uint8>(address, value); },
                      .write16 = [](uint32 address, uint16 value,
                                    void *ctx) { static_cast<SCSP *>(ctx)->WriteWRAM<uint16>(address, value); },
                      .write32 =
                          [](uint32 address, uint32 value, void *ctx) {
                              static_cast<SCSP *>(ctx)->WriteWRAM<uint16>(address + 0, value >> 16u);
                              static_cast<SCSP *>(ctx)->WriteWRAM<uint16>(address + 2, value >> 0u);
                          },
                  });

    // Registers
    bus.MapMemory(0x5B0'0000, 0x5BF'FFFF,
                  {
                      .ctx = this,
                      .read8 = [](uint32 address, void *ctx) -> uint8 {
                          return static_cast<SCSP *>(ctx)->ReadReg<uint8>(address);
                      },
                      .read16 = [](uint32 address, void *ctx) -> uint16 {
                          return static_cast<SCSP *>(ctx)->ReadReg<uint16>(address);
                      },
                      .read32 = [](uint32 address, void *ctx) -> uint32 {
                          uint32 value = static_cast<SCSP *>(ctx)->ReadReg<uint16>(address + 0) << 16u;
                          value |= static_cast<SCSP *>(ctx)->ReadReg<uint16>(address + 2) << 0u;
                          return value;
                      },
                      .write8 = [](uint32 address, uint8 value,
                                   void *ctx) { static_cast<SCSP *>(ctx)->WriteReg<uint8>(address, value); },
                      .write16 = [](uint32 address, uint16 value,
                                    void *ctx) { static_cast<SCSP *>(ctx)->WriteReg<uint16>(address, value); },
                      .write32 =
                          [](uint32 address, uint32 value, void *ctx) {
                              static_cast<SCSP *>(ctx)->WriteReg<uint16>(address + 0, value >> 16u);
                              static_cast<SCSP *>(ctx)->WriteReg<uint16>(address + 2, value >> 0u);
                          },
                  });
}

void SCSP::Advance(uint64 cycles) {
    if (m_m68kEnabled) {
        m_m68kCycles += cycles;
        while (m_m68kCycles >= kCyclesPerM68KCycle) {
            // TODO: proper cycle counting
            m_m68k.Step();
            m_m68kCycles -= kCyclesPerM68KCycle;
        }
    }
}

uint32 SCSP::ReceiveCDDA(std::span<uint8, 2048> data) {
    std::copy_n(data.begin(), 2048, m_cddaBuffer.begin() + m_cddaWritePos);
    m_cddaWritePos = (m_cddaWritePos + 2048) % m_cddaBuffer.size();
    sint32 len = static_cast<sint32>(m_cddaWritePos) - m_cddaReadPos;
    if (len < 0) {
        len += m_cddaBuffer.size();
    }
    if (len >= 2048 * 4) {
        m_cddaReady = true;
    }
    return len;
}

void SCSP::DumpWRAM(std::ostream &out) const {
    out.write((const char *)m_WRAM.data(), m_WRAM.size());
}

void SCSP::DumpDSP_MPRO(std::ostream &out) const {
    out.write((const char *)m_dsp.program.data(), sizeof(m_dsp.program));
}

void SCSP::DumpDSP_TEMP(std::ostream &out) const {
    out.write((const char *)m_dsp.tempMem.data(), sizeof(m_dsp.tempMem));
}

void SCSP::DumpDSP_MEMS(std::ostream &out) const {
    out.write((const char *)m_dsp.soundMem.data(), sizeof(m_dsp.soundMem));
}

void SCSP::DumpDSP_COEF(std::ostream &out) const {
    out.write((const char *)m_dsp.coeffs.data(), sizeof(m_dsp.coeffs));
}

void SCSP::DumpDSP_MADRS(std::ostream &out) const {
    out.write((const char *)m_dsp.addrs.data(), sizeof(m_dsp.addrs));
}

void SCSP::DumpDSP_MIXS(std::ostream &out) const {
    out.write((const char *)m_dsp.mixStack.data(), sizeof(m_dsp.mixStack));
}

void SCSP::DumpDSP_EFREG(std::ostream &out) const {
    out.write((const char *)m_dsp.effectOut.data(), sizeof(m_dsp.effectOut));
}

void SCSP::DumpDSP_EXTS(std::ostream &out) const {
    out.write((const char *)m_dsp.audioInOut.data(), sizeof(m_dsp.audioInOut));
}

void SCSP::DumpDSPRegs(std::ostream &out) const {
    m_dsp.DumpRegs(out);
}

void SCSP::SetCPUEnabled(bool enabled) {
    if (m_m68kEnabled != enabled) {
        rootLog.info("MC68EC00 processor {}", (enabled ? "enabled" : "disabled"));
        if (enabled) {
            m_m68k.Reset(true); // false? does it matter?
        }
        m_m68kEnabled = enabled;
    }
}

void SCSP::OnSampleTickEvent(core::EventContext &eventContext, void *userContext, uint64 cyclesLate) {
    auto &scsp = *static_cast<SCSP *>(userContext);
    scsp.Tick();
    eventContext.RescheduleFromNow(kCyclesPerSample);
}

void SCSP::UpdateClockRatios() {
    const auto &clockRatios = m_system.GetClockRatios();
    m_scheduler.SetEventCountFactor(m_sampleTickEvent, clockRatios.SCSPNum, clockRatios.SCSPDen);
}

void SCSP::HandleKYONEX() {
    for (auto &slot : m_slots) {
        if (slot.TriggerKey()) {
            static constexpr const char *loopNames[] = {"->|", ">->", "<-<", ">-<"};
            regsLog.trace(
                "Slot {:02d} key {} {:2d}-bit addr={:05X} loop={:04X}-{:04X} {} OCT={:02d} FNS={:03X} KRS={:X} "
                "EG {:02d} {:02d} {:02d} {:02d} DL={:03X} EGHOLD={} LPSLNK={} mod X={:02X} Y={:02X} lv={:X}",
                slot.index, (slot.keyOnBit ? " ON" : "OFF"), (slot.pcm8Bit ? 8 : 16), slot.startAddress,
                slot.loopStartAddress, slot.loopEndAddress, loopNames[static_cast<uint32>(slot.loopControl)],
                slot.octave, slot.freqNumSwitch, slot.keyRateScaling, slot.attackRate, slot.decay1Rate, slot.decay2Rate,
                slot.releaseRate, slot.decayLevel, static_cast<uint8>(slot.egHold),
                static_cast<uint8>(slot.loopStartLink), slot.modXSelect, slot.modYSelect, slot.modLevel);
        }
    }

    auto makeList = [&] {
        static char out[32];
        for (auto &slot : m_slots) {
            out[slot.index] = slot.keyOnBit ? '+' : '_';
        }
        return std::string_view(out, 32);
    };
    regsLog.trace("KYONEX: {}", makeList());
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

FORCE_INLINE void SCSP::Tick() {
    RunM68K();
    GenerateSample();
    UpdateTimers();
    UpdateM68KInterrupts();
    UpdateSCUInterrupts();
}

FORCE_INLINE void SCSP::RunM68K() {
    if (m_m68kEnabled) {
        for (uint64 cy = 0; cy < kM68KCyclesPerSample; cy++) {
            // TODO: proper cycle counting
            m_m68k.Step();
        }
    }
}

FORCE_INLINE void SCSP::GenerateSample() {
    sint32 outL = 0;
    sint32 outR = 0;

    auto adjustSendLevel = [](sint16 output, uint8 sendLevel) { return output >> (sendLevel ^ 7); };

    auto addPannedOutput = [&](sint16 output, uint8 pan) {
        const sint32 panL = pan < 0x10 ? pan : 0;
        const sint32 panR = pan < 0x10 ? 0 : (pan & 0xF);
        outL += output >> (panL + 1);
        outR += output >> (panR + 1);
    };

    // Process slots
    for (uint32 i = 0; i < 32; i++) {
        SlotProcessStep1(m_slots[i]);
        SlotProcessStep2(m_slots[(i - 1u) & 31]);
        SlotProcessStep3(m_slots[(i - 2u) & 31]);
        SlotProcessStep4(m_slots[(i - 3u) & 31]);
        SlotProcessStep5(m_slots[(i - 4u) & 31]);
        SlotProcessStep6(m_slots[(i - 5u) & 31]);
        SlotProcessStep7(m_slots[(i - 6u) & 31]);

        Slot &outputSlot = m_slots[(i - 6u) & 31];
        if (outputSlot.directSendLevel > 0) {
            const sint16 directOutput = adjustSendLevel(outputSlot.output, outputSlot.directSendLevel);
            addPannedOutput(directOutput, outputSlot.directPan);
        }

        if (outputSlot.inputMixingLevel > 0) {
            const sint16 mixsOutput = adjustSendLevel(outputSlot.output, outputSlot.inputMixingLevel);
            m_dsp.mixStack[outputSlot.inputSelect] += mixsOutput << 4;
        }

        m_soundStackIndex = (m_soundStackIndex + 1) & 63;
    }

    // Copy CDDA data to DSP EXTS (0=left, 1=right)
    if (m_cddaReady && m_cddaReadPos != m_cddaWritePos) {
        m_dsp.audioInOut[0] = util::ReadLE<uint16>(&m_cddaBuffer[m_cddaReadPos + 0]);
        m_dsp.audioInOut[1] = util::ReadLE<uint16>(&m_cddaBuffer[m_cddaReadPos + 2]);
        m_cddaReadPos = (m_cddaReadPos + 2 * sizeof(uint16)) % m_cddaBuffer.size();
    } else {
        // Buffer underrun
        m_dsp.audioInOut[0] = 0;
        m_dsp.audioInOut[1] = 0;
        m_cddaReady = false;
    }

    m_dsp.Run();

    for (uint32 i = 0; i < 16; i++) {
        const Slot &slot = m_slots[i];
        if (slot.effectSendLevel > 0) {
            const sint16 dspOutput = adjustSendLevel(m_dsp.effectOut[i], slot.effectSendLevel);
            addPannedOutput(dspOutput, slot.effectPan);
        }
    }
    for (uint32 i = 0; i < 2; i++) {
        const Slot &slot = m_slots[i + 16];
        if (slot.effectSendLevel > 0) {
            const sint16 dspOutput = adjustSendLevel(m_dsp.audioInOut[i], slot.effectSendLevel);
            addPannedOutput(dspOutput, slot.effectPan);
        }
    }

    outL >>= m_masterVolume ^ 0xF;
    outR >>= m_masterVolume ^ 0xF;

    static constexpr sint32 outMin = std::numeric_limits<sint16>::min();
    static constexpr sint32 outMax = std::numeric_limits<sint16>::max();

    outL = std::clamp<sint32>(outL, outMin, outMax);
    outR = std::clamp<sint32>(outR, outMin, outMax);

    m_cbOutputSample(outL, outR);

    m_sampleCounter++;
    m_egStep = m_sampleCounter & 1;
    if (m_egStep) {
        m_egCycle++;
        if (m_egCycle == 0x1000) {
            m_egCycle = 1;
        }
    }

    SetInterrupt(kIntrSample, true);
}

FORCE_INLINE void SCSP::UpdateTimers() {
    for (int i = 0; i < 3; i++) {
        auto &timer = m_timers[i];
        const bool trigger = (m_sampleCounter & timer.incrementMask) == 0;
        if (trigger && timer.Tick()) {
            SetInterrupt(kIntrTimerA + i, true);
        }
    }
}

FORCE_INLINE void SCSP::SlotProcessStep1(Slot &slot) {
    if (!slot.active) {
        return;
    }

    slot.IncrementLFO();

    // Pitch LFO waveform tables
    static constexpr auto sawTable = [] {
        std::array<sint8, 256> arr{};
        for (uint32 i = 0; i < 256; i++) {
            arr[i] = static_cast<sint8>(i);
        }
        return arr;
    }();
    static constexpr auto squareTable = [] {
        std::array<sint8, 256> arr{};
        for (uint32 i = 0; i < 256; i++) {
            arr[i] = i < 128 ? 127 : -128;
        }
        return arr;
    }();
    static constexpr auto triangleTable = [] {
        std::array<sint8, 256> arr{};
        for (uint32 i = 0; i < 128; i++) {
            const uint8 il = i - 64;
            const uint8 ir = 255 - i - 64;
            arr[il] = arr[ir] = i * 2 - 128;
        }
        return arr;
    }();

    // Compute pitch LFO
    sint32 pitchLFO = 0;
    using enum Slot::Waveform;
    switch (slot.pitchLFOWaveform) {
    case Saw: pitchLFO = sawTable[slot.lfoStep]; break;
    case Square: pitchLFO = squareTable[slot.lfoStep]; break;
    case Triangle: pitchLFO = triangleTable[slot.lfoStep]; break;
    case Noise: pitchLFO = static_cast<sint8>(m_lfsr & ~1); break;
    }

    slot.IncrementPhase((pitchLFO << slot.pitchLFOSens) >> 2);
}

FORCE_INLINE void SCSP::SlotProcessStep2(Slot &slot) {
    if (!slot.active) {
        return;
    }

    sint32 modulation = 0;
    if (slot.modLevel > 0 || slot.modXSelect != 0 || slot.modYSelect != 0) {
        const sint16 xd = m_soundStack[(m_soundStackIndex - 1 + slot.modXSelect) & 63];
        const sint16 yd = m_soundStack[(m_soundStackIndex - 1 + slot.modYSelect) & 63];
        const sint32 zd = (xd + yd) / 2;
        modulation = (zd << 5) >> (20 - slot.modLevel);
    }

    slot.IncrementSampleCounter();
    slot.IncrementAddress(modulation);
}

FORCE_INLINE void SCSP::SlotProcessStep3(Slot &slot) {
    if (!slot.active) {
        return;
    }

    // TODO: check behavior on loop boundaries
    const sint32 inc = slot.reverse ? -1 : +1;
    if (slot.pcm8Bit) {
        const uint32 address1 = slot.currAddress;
        const uint32 address2 = slot.currAddress + inc * sizeof(uint8);
        slot.sample1 = static_cast<sint8>(ReadWRAM<uint8>(address1)) << 8;
        if (address2 >= slot.startAddress && address2 < slot.startAddress + slot.loopEndAddress) {
            slot.sample2 = static_cast<sint8>(ReadWRAM<uint8>(address2)) << 8;
        } else {
            slot.sample2 = slot.sample1;
        }
    } else {
        const uint32 address1 = slot.currAddress;
        const uint32 address2 = slot.currAddress + inc * sizeof(uint16);
        slot.sample1 = static_cast<sint16>(ReadWRAM<uint16>(address1 & ~1));
        if (address2 >= slot.startAddress && address2 < slot.startAddress + slot.loopEndAddress * sizeof(uint16)) {
            slot.sample2 = static_cast<sint16>(ReadWRAM<uint16>(address2 & ~1));
        } else {
            slot.sample2 = slot.sample1;
        }
    }
    slot.sample1 ^= slot.sampleXOR;
    slot.sample2 ^= slot.sampleXOR;
}

FORCE_INLINE void SCSP::SlotProcessStep4(Slot &slot) {
    if (!slot.active) {
        return;
    }

    // TODO: make this configurable
    static constexpr bool interpolate = false;
    if constexpr (interpolate) {
        // Interpolate linearly between samples
        slot.output =
            slot.sample1 + (slot.sample2 - slot.sample1) * static_cast<sint64>(slot.currPhase & 0x3FFFF) / 0x40000;
    } else {
        slot.output = slot.sample1;
    }

    // TODO: what does the ALFO calculation deliver here?

    // Advance envelope generator
    if (!m_egStep) {
        return;
    }

    static constexpr uint32 kCounterShiftTable[] = {11, 11, 11, 11, // 0-3    (0x00-0x03)
                                                    10, 10, 10, 10, // 4-7    (0x04-0x07)
                                                    9,  9,  9,  9,  // 8-11   (0x08-0x0B)
                                                    8,  8,  8,  8,  // 12-15  (0x0C-0x0F)
                                                    7,  7,  7,  7,  // 16-19  (0x10-0x13)
                                                    6,  6,  6,  6,  // 20-23  (0x14-0x17)
                                                    5,  5,  5,  5,  // 24-27  (0x18-0x1B)
                                                    4,  4,  4,  4,  // 28-31  (0x1C-0x1F)
                                                    3,  3,  3,  3,  // 32-35  (0x20-0x23)
                                                    2,  2,  2,  2,  // 36-39  (0x24-0x27)
                                                    1,  1,  1,  1,  // 40-43  (0x28-0x2B)
                                                    0,  0,  0,  0,  // 44-47  (0x2C-0x2F)
                                                    0,  0,  0,  0,  // 48-51  (0x30-0x33)
                                                    0,  0,  0,  0,  // 52-55  (0x34-0x37)
                                                    0,  0,  0,  0,  // 56-59  (0x38-0x3B)
                                                    0,  0,  0,  0}; // 60-63  (0x3C-0x3F)

    static constexpr uint32 kIncrementTable[][8] = {
        {0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0},  // 0-1    (0x00-0x01)
        {0, 1, 1, 1, 0, 1, 1, 1}, {0, 1, 1, 1, 1, 1, 1, 1},  // 2-3    (0x02-0x03)
        {0, 1, 0, 1, 0, 1, 0, 1}, {0, 1, 0, 1, 1, 1, 0, 1},  // 4-5    (0x04-0x05)
        {0, 1, 1, 1, 0, 1, 1, 1}, {0, 1, 1, 1, 1, 1, 1, 1},  // 6-7    (0x06-0x07)
        {0, 1, 0, 1, 0, 1, 0, 1}, {0, 1, 0, 1, 1, 1, 0, 1},  // 8-9    (0x08-0x09)
        {0, 1, 1, 1, 0, 1, 1, 1}, {0, 1, 1, 1, 1, 1, 1, 1},  // 10-11  (0x0A-0x0B)
        {0, 1, 0, 1, 0, 1, 0, 1}, {0, 1, 0, 1, 1, 1, 0, 1},  // 12-13  (0x0C-0x0D)
        {0, 1, 1, 1, 0, 1, 1, 1}, {0, 1, 1, 1, 1, 1, 1, 1},  // 14-15  (0x0E-0x0F)
        {0, 1, 0, 1, 0, 1, 0, 1}, {0, 1, 0, 1, 1, 1, 0, 1},  // 16-17  (0x10-0x11)
        {0, 1, 1, 1, 0, 1, 1, 1}, {0, 1, 1, 1, 1, 1, 1, 1},  // 18-19  (0x12-0x13)
        {0, 1, 0, 1, 0, 1, 0, 1}, {0, 1, 0, 1, 1, 1, 0, 1},  // 20-21  (0x14-0x15)
        {0, 1, 1, 1, 0, 1, 1, 1}, {0, 1, 1, 1, 1, 1, 1, 1},  // 22-23  (0x18-0x17)
        {0, 1, 0, 1, 0, 1, 0, 1}, {0, 1, 0, 1, 1, 1, 0, 1},  // 24-25  (0x18-0x19)
        {0, 1, 1, 1, 0, 1, 1, 1}, {0, 1, 1, 1, 1, 1, 1, 1},  // 26-27  (0x1A-0x1B)
        {0, 1, 0, 1, 0, 1, 0, 1}, {0, 1, 0, 1, 1, 1, 0, 1},  // 28-29  (0x1C-0x1D)
        {0, 1, 1, 1, 0, 1, 1, 1}, {0, 1, 1, 1, 1, 1, 1, 1},  // 30-31  (0x1E-0x1F)
        {0, 1, 0, 1, 0, 1, 0, 1}, {0, 1, 0, 1, 1, 1, 0, 1},  // 32-33  (0x20-0x21)
        {0, 1, 1, 1, 0, 1, 1, 1}, {0, 1, 1, 1, 1, 1, 1, 1},  // 34-35  (0x22-0x23)
        {0, 1, 0, 1, 0, 1, 0, 1}, {0, 1, 0, 1, 1, 1, 0, 1},  // 36-37  (0x24-0x25)
        {0, 1, 1, 1, 0, 1, 1, 1}, {0, 1, 1, 1, 1, 1, 1, 1},  // 38-39  (0x26-0x27)
        {0, 1, 0, 1, 0, 1, 0, 1}, {0, 1, 0, 1, 1, 1, 0, 1},  // 40-41  (0x28-0x29)
        {0, 1, 1, 1, 0, 1, 1, 1}, {0, 1, 1, 1, 1, 1, 1, 1},  // 42-43  (0x2A-0x2B)
        {0, 1, 0, 1, 0, 1, 0, 1}, {0, 1, 0, 1, 1, 1, 0, 1},  // 44-45  (0x2C-0x2D)
        {0, 1, 1, 1, 0, 1, 1, 1}, {0, 1, 1, 1, 1, 1, 1, 1},  // 46-47  (0x2E-0x2F)
        {1, 1, 1, 1, 1, 1, 1, 1}, {1, 1, 1, 2, 1, 1, 1, 2},  // 48-49  (0x30-0x31)
        {1, 2, 1, 2, 1, 2, 1, 2}, {1, 2, 2, 2, 1, 2, 2, 2},  // 50-51  (0x32-0x33)
        {2, 2, 2, 2, 2, 2, 2, 2}, {2, 2, 2, 4, 2, 2, 2, 4},  // 52-53  (0x34-0x35)
        {2, 4, 2, 4, 2, 4, 2, 4}, {2, 4, 4, 4, 2, 4, 4, 4},  // 54-55  (0x36-0x37)
        {4, 4, 4, 4, 4, 4, 4, 4}, {4, 4, 4, 8, 4, 4, 4, 8},  // 56-57  (0x38-0x39)
        {4, 8, 4, 8, 4, 8, 4, 8}, {4, 8, 8, 8, 4, 8, 8, 8},  // 58-59  (0x3A-0x3B)
        {8, 8, 8, 8, 8, 8, 8, 8}, {8, 8, 8, 8, 8, 8, 8, 8},  // 60-61  (0x3C-0x3D)
        {8, 8, 8, 8, 8, 8, 8, 8}, {8, 8, 8, 8, 8, 8, 8, 8}}; // 62-63  (0x3E-0x3F)

    const uint32 rate = slot.CalcEffectiveRate(slot.GetCurrentEGRate());
    const uint32 shift = kCounterShiftTable[rate];
    uint32 inc{};
    if (m_egCycle & ((1 << shift) - 1)) {
        inc = 0;
    } else {
        inc = kIncrementTable[rate][(m_egCycle >> shift) & 7];
    }

    switch (slot.egState) {
    case Slot::EGState::Attack:
        if (slot.egLevel == 0 && !slot.loopStartLink) {
            slot.egState = Slot::EGState::Decay1;
        } else if (inc > 0 && slot.egLevel > 0 /*&& rate < 0x3E*/) {
            slot.egLevel += static_cast<sint32>(~static_cast<uint32>(slot.egLevel) * inc) >> 4;
        }
        break;
    case Slot::EGState::Decay1:
        if ((slot.egLevel >> 5u) >= slot.decayLevel) {
            slot.egState = Slot::EGState::Decay2;
        }
        // fallthrough
    case Slot::EGState::Decay2:  // fallthrough
    case Slot::EGState::Release: //
        slot.egLevel = std::min<uint16>(slot.egLevel + inc, 0x3FF);
        if (slot.egLevel == 0x3FF) {
            slot.active = false;
        }
    }
}

FORCE_INLINE void SCSP::SlotProcessStep5(Slot &slot) {
    m_lfsr = (m_lfsr >> 1u) | (((m_lfsr >> 5u) ^ m_lfsr) & 1u) << 16u;

    if (!slot.active) {
        slot.output = 0;
        return;
    }

    if (!slot.soundDirect) {
        static constexpr auto sawTable = [] {
            std::array<uint8, 256> arr{};
            for (uint32 i = 0; i < 256; i++) {
                arr[i] = i;
            }
            return arr;
        }();
        static constexpr auto squareTable = [] {
            std::array<uint8, 256> arr{};
            for (uint32 i = 0; i < 256; i++) {
                arr[i] = i < 128 ? 0x00 : 0xFF;
            }
            return arr;
        }();
        static constexpr auto triangleTable = [] {
            std::array<uint8, 256> arr{};
            for (uint32 i = 0; i < 128; i++) {
                const uint8 il = i;
                const uint8 ir = 255 - i;
                arr[il] = arr[ir] = i * 2;
            }
            return arr;
        }();

        uint32 alfoLevel = 0u;
        using enum Slot::Waveform;
        switch (slot.ampLFOWaveform) {
        case Saw: alfoLevel = sawTable[slot.lfoStep]; break;
        case Square: alfoLevel = squareTable[slot.lfoStep]; break;
        case Triangle: alfoLevel = triangleTable[slot.lfoStep]; break;
        case Noise: alfoLevel = static_cast<uint8>(m_lfsr & ~1); break;
        }

        alfoLevel = ((alfoLevel + 1u) >> (7u - slot.ampLFOSens)) << 1u;
        const sint32 envLevel = slot.GetEGLevel();
        const sint32 totalLevel = slot.totalLevel << 2u;
        const sint32 level = std::min<sint32>(alfoLevel + envLevel + totalLevel, 0x3FF);
        slot.output = (slot.output * ((level & 0x3F) ^ 0x7F)) >> ((level >> 6) + 7);
    }
}

FORCE_INLINE void SCSP::SlotProcessStep6(Slot &slot) {
    // TODO: implement level calculation part 2
}

FORCE_INLINE void SCSP::SlotProcessStep7(Slot &slot) {
    if (!slot.stackWriteInhibit) {
        const uint32 stackIndex = (m_soundStackIndex - 6) & 63;
        m_soundStack[stackIndex] = slot.output;
    }

    slot.sampleCount++;
}

ExceptionVector SCSP::AcknowledgeInterrupt(uint8 level) {
    return ExceptionVector::AutoVectorRequest;
}

} // namespace satemu::scsp
