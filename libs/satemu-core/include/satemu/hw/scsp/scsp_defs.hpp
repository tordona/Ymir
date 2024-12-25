#pragma once

#include <satemu/core_types.hpp>
#include <satemu/hw/hw_defs.hpp>

#include <satemu/util/bit_ops.hpp>

#include <array>

namespace satemu::scsp {

// Audio sampling rate in Hz
inline constexpr uint64 kAudioFreq = 44100;

// Number of SCSP cycles for each sample output
inline constexpr uint64 kCyclesPerSample = 512;

// SCSP clock frequency: 22,579,200 Hz = 44,100 Hz * 512 cycles per sample
inline constexpr uint64 kClockFreq = kAudioFreq * kCyclesPerSample;

// Pending interrupt flags
inline constexpr uint16 kIntrINT0N = 0;          // External INT0N line
inline constexpr uint16 kIntrINT1N = 1;          // External INT1N line
inline constexpr uint16 kIntrINT2N = 2;          // External INT2N line
inline constexpr uint16 kIntrMIDIInput = 3;      // MIDI input non-empty
inline constexpr uint16 kIntrDMATransferEnd = 4; // DMA transfer end
inline constexpr uint16 kIntrCPUManual = 5;      // CPU manual interrupt request
inline constexpr uint16 kIntrTimerA = 6;         // Timer A
inline constexpr uint16 kIntrTimerB = 7;         // Timer B
inline constexpr uint16 kIntrTimerC = 8;         // Timer C
inline constexpr uint16 kIntrMIDIOutput = 9;     // MIDI output empty
inline constexpr uint16 kIntrSample = 10;        // Once every sample tick

struct Slot {
    Slot() {
        Reset();
    }

    void Reset() {
        startAddress = 0;
        loopStartAddress = 0;
        loopEndAddress = 0;
        pcm8Bit = false;
        keyOnBit = false;
        loopControl = LoopControl::Off;
        sampleXOR = 0x0000;
        soundSource = SoundSource::SoundRAM;

        attackRate = 0;
        decay1Rate = 0;
        decay2Rate = 0;
        releaseRate = 0;
        decayLevel = 0;
        keyRateScaling = 0;
        loopStateLink = false;
        egHold = false;

        modulationLevel = 0;
        modXSelect = 0;
        modYSelect = 0;
        stackWriteInhibit = false;

        totalLevel = 0;
        soundDirect = false;

        octave = 0;
        freqNumSwitch = 0;

        lfoReset = false;
        lfof = 0;
        lfoFreq = s_lfoFreqTbl[0];
        ampLFOSens = 0;
        ampLFOWaveform = Waveform::Saw;
        pitchLFOWaveform = Waveform::Saw;

        inputMixingLevel = 0;
        inputSelect = 0;
        directSendLevel = 0;
        directPan = 0;
        effectSendLevel = 0;
        effectPan = 0;

        keyOn = false;
    }

    template <typename T, bool fromM68K>
    T ReadReg(uint32 address) {
        // TODO: implement similar to SCSP::ReadReg
        return 0;
    }

    template <typename T, bool fromM68K>
    void WriteReg(uint32 address, T value) {
        // TODO: implement similar to SCSP::WriteReg
    }

    // -------------------------------------------------------------------------
    // Registers

    // --- Loop Control Register ---

    uint32 startAddress;     // (R/W) SA
    uint32 loopStartAddress; // (R/W) LSA
    uint32 loopEndAddress;   // (R/W) LEA
    bool pcm8Bit;            // (R/W) PCM8B
    bool keyOnBit;           // (R/W) KYONB

    // Loop control specifies how the loop segment is played if the sound is held continuously.
    // All modes play the segment between SA and LSA forwards.
    //   Off disables sample looping. The sample stops at LEA.
    //   Normal loops the segment between LSA and LEA forwards.
    //   Reverse plays forwards from SA to LSA, then jumps to LEA and repeats the loop segment in reverse.
    //   Alternate plays the loop segment forwards, then backwards, then forwards, ...
    //
    //            SA     LSA         LEA
    //            |       |           |
    //       Off  +--->---+--->-------X   sample stops playing at LEA
    //            |       |           |
    //    Normal  +--->---+--->------->   sample repeats from LSA when it hits
    //            |       +--->------->   LEA and always plays forwards
    //            |       +--->------->
    //            |       |           |
    //   Reverse  +--->--->  >  >  >  |   sample skips LSA, plays backwards
    //            |       <-------<---+   from LEA and repeats from LEA upon
    //            |       <-------<---+   reaching LSA; always plays in reverse
    //            |       |           |
    // Alternate  +--->---+--->------->   sample plays forwards until LEA
    //            |       <-------<---+   then plays backwards until LSA,
    //            |       +--->------->   and keeps bouncing back and forth
    enum class LoopControl { Off, Normal, Reverse, Alternate };
    LoopControl loopControl; // (R/W) LPCTL

    // SBCTL enables XORing sample data.
    //   bit 0 flips every bit other than the sign bit
    //   bit 1 flips the sign bit
    // This is useful for supporting samples in different formats (e.g. unsigned)
    uint16 sampleXOR; // (R/W) SBCTL0/1
    // Implementation notes:
    //   SBCTL0: 0x7FFF
    //   SBCTL1: 0x8000
    // Make a simple LUT: 0x0000, 0x7FFF, 0x8000, 0xFFFF
    // To read back the value, simply look at bits 14 and 15.

    enum class SoundSource { SoundRAM, Noise, Silence, Unknown };
    SoundSource soundSource; // (R/W) SSCTL

    // --- Envelope Generator Register ---

    // States: Attack, Decay 1, Decay 2, Release
    //
    // Starts from Attack on Key ON.
    // While Key ON is held, goes through Attack -> Decay 1 -> Decay 2 and stays at the minimum value of Decay 2.
    // On Key OFF, it will immediately skip to Release state, decrementing the envelope from whatever point it was.
    //
    // Values range from 0x3FF (minimum) to 0x000 (maximum) - 10 bits.

    // Value ranges are from minimum to maximum.
    uint8 attackRate;     // (R/W) AR  - 0x00 to 0x1F
    uint8 decay1Rate;     // (R/W) D1R - 0x00 to 0x1F
    uint8 decay2Rate;     // (R/W) D2R - 0x00 to 0x1F
    uint8 releaseRate;    // (R/W) RR  - 0x00 to 0x1F
    uint8 decayLevel;     // (R/W) DL  - 0x1F to 0x00
                          //   specifies the MSB 5 bits of the EG value where to switch from decay 1 to decay 2
    uint8 keyRateScaling; // (R/W) KRS - 0x00 to 0x0E; 0x0F turns off scaling
    bool loopStateLink;   // (R/W) LPSLNK
                          //   true:  switches to decay 1 state on LSA
                          //          attack state is interrupted if too slow or held if too fast
                          //          if the state change happens below DL, decay 2 state is never reached
                          //   false: state changes are dictated by rates only
    bool egHold;          // (R/W) EGHOLD
                          //   true:  volume raises during attack state
                          //   false: volume is set to maximum during attack phase while maintaining the same duration

    // --- FM Modulation Control Register ---

    uint8 modulationLevel;  // (R/W) MDL - add +- n * pi where n is:
                            // 0-4   5     6    7    8   9  A  B  C  D   E   F
                            //  0   1/16  1/8  1/4  1/2  1  2  4  8  16  32  64
    uint8 modXSelect;       // (R/W) MDXSL - selects modulation input X
    uint8 modYSelect;       // (R/W) MDYSL - selects modulation input Y
    bool stackWriteInhibit; // (R/W) STWINH - when set, blocks writes to direct data stack (SOUS)

    // --- Sound Volume Register ---

    uint8 totalLevel; // (R/W) TL - 0x00 = no attenuation, 0xFF = max attenuation (-95.7 dB)
    bool soundDirect; // (R/W) SDIR - true causes the sound from this slot to bypass the EG, TL, ALFO, etc.

    // --- Pitch Register ---

    uint8 octave;         // (R/W) OCT
    uint16 freqNumSwitch; // (R/W) FNS

    // --- LFO Register ---

    enum class Waveform { Saw, Square, Triangle, Noise };

    static constexpr std::array<uint32, 32> s_lfoFreqTbl = {1020, 892, 764, 636, 508, 444, 380, 316, 252, 220, 188,
                                                            156,  124, 108, 92,  76,  60,  52,  44,  36,  28,  24,
                                                            20,   16,  12,  10,  8,   6,   4,   3,   2,   1};

    bool lfoReset;             // (R/W) LFORE - true resets the LFO
    uint8 lfof;                // (R/W) LFOF - 0x00 to 0x1F (raw value)
    uint32 lfoFreq;            // (R/W) LFOF - determines the LFO increment interval (from s_lfoFreqTbl)
    uint8 ampLFOSens;          // (R/W) ALFOS - 0 (none) to 7 (maximum) intensity of tremor effect
    uint8 pitchLFOSens;        // (R/W) PLFOS - 0 (none) to 7 (maximum) intensity of tremolo effect
    Waveform ampLFOWaveform;   // (R/W) ALFOWS - unsigned from 0x00 to 0xFF (all waveforms start at zero and increment)
    Waveform pitchLFOWaveform; // (R/W) PLFOWS - signed from 0x80 to 0x7F (zero at 0x00, starting point of saw/triangle)

    // --- Mixer Register ---
    uint8 inputMixingLevel; // (R/W) IMXL - 0 (no mix) to 7 (maximum) - into MIXS DSP stack
    uint8 inputSelect;      // (R/W) ISEL - 0 to 15 - indexes a MIXS DSP stack
    uint8 directSendLevel;  // (R/W) DISDL - 0 (no send) to 7 (maximum)
    uint8 directPan;        // (R/W) DIPAN - 0 to 31  [100% left]  31..16  [center]  0..15  [100% right]
    uint8 effectSendLevel;  // (R/W) EFSDL - 0 (no send) to 7 (maximum)
    uint8 effectPan;        // (R/W) EFPAN - 0 to 31  [100% left]  31..16  [center]  0..15  [100% right]

    // -------------------------------------------------------------------------
    // State

    // KYONEX is a write-only register that applies the value of keyOnBit (KYONB) to this variable.
    // If changed, triggers the new state (e.g. begin ADSR attack phase on key ON or release phase on key OFF).
    // Writing 1 to KYONEX on any slot will apply KYONB on every slot.
    bool keyOn; // current key on state
};

struct Timer {
    Timer() {
        Reset();
    }

    void Reset() {
        incrementInterval = 0;
        reload = 0;
        incrementMask = 0;
        doReload = false;
        counter = 0;
    }

    bool Tick() {
        if (doReload) {
            counter = reload;
            doReload = false;
        } else {
            counter++;
        }
        return counter == 0xFF;
    }

    void WriteTIMx(uint8 value) {
        reload = value;
        doReload = true;
    }

    void WriteTxCTL(uint8 value) {
        incrementInterval = bit::extract<0, 2>(value);
        incrementMask = ((1u << value) - 1);
    }

    // -------------------------------------------------------------------------
    // Registers

    uint8 incrementInterval; // (W) TxCTL - 0 to 7 - increment every (1 << N) samples
    uint8 reload;            // (W) TIMx - resets the timer counter on the next tick

    // -------------------------------------------------------------------------
    // State

    uint64 incrementMask; // computed from TxCTL
    bool doReload;        // whether to reload the counter on the next tick
    uint8 counter;        // counts up to 0xFF, then raises an interrupt
};

} // namespace satemu::scsp
