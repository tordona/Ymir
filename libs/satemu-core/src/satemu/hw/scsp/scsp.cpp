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

    m_cpuEnabled = false;

    m_accumSampleCycles = 0;
    m_sampleCounter = 0;

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

    for (auto &slot : m_slots) {
        slot.Reset();
    }
}

void SCSP::Advance(uint64 cycles) {
    if (m_cpuEnabled) {
        // TODO: proper cycle counting
        for (uint64 i = 0; i < cycles; i++) {
            m_m68k.Step();
        }
    }
    m_accumSampleCycles += cycles;
    while (m_accumSampleCycles >= kCyclesPerSample) {
        m_accumSampleCycles -= kCyclesPerSample;
        m_sampleCounter++;
        ProcessSample();
    }
}

void SCSP::SetCPUEnabled(bool enabled) {
    fmt::println("SCSP: {} the MC68EC00 processor", (enabled ? "enabling" : "disabling"));
    if (enabled) {
        m_m68k.Reset(true); // false? does it matter?
    }
    m_cpuEnabled = enabled;
}

void SCSP::SetInterrupt(uint16 intr, bool level) {
    m_m68kPendingInterrupts &= ~(1 << intr);
    m_m68kPendingInterrupts |= level << intr;

    m_scuPendingInterrupts &= ~(1 << intr);
    m_scuPendingInterrupts |= level << intr;
}

void SCSP::UpdateM68KInterrupts() {
    // NOTE: interrupts 7-9 share the same level
    const uint16 baseMask = m_m68kPendingInterrupts & m_m68kEnabledInterrupts;
    const uint8 mask = baseMask | (bit::extract<8, 9>(baseMask) ? 0x80 : 0x00);

    // Check all active interrupts in parallel
    std::array<uint8, 3> scilv = m_m68kInterruptLevels;
    scilv[0] &= mask;
    scilv[1] &= mask;
    scilv[2] &= mask;

    // Select maximum level among active interrupts
    uint8 level = 0;
    if (scilv[2] & mask) {
        level |= 4;
        scilv[1] &= scilv[2];
        scilv[0] &= scilv[2];
    }
    if (scilv[1] & mask) {
        level |= 2;
        scilv[0] &= scilv[1];
    }
    if (scilv[0] & mask) {
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
