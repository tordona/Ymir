#pragma once

#include <satemu/core_types.hpp>
#include <satemu/hw/hw_defs.hpp>

#include "envelope_generator.hpp"

#include <satemu/util/bit_ops.hpp>

#include <array>

namespace satemu::scsp {

// Audio sampling rate in Hz
inline constexpr uint64 kAudioFreq = 44100;

// Number of SCSP cycles per sample
inline constexpr uint64 kCyclesPerSample = 512;

// Number of SCSP cycles per MC68EC000 cycle
inline constexpr uint64 kCyclesPerM68KCycle = 2;

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

        envGen.Reset();

        modLevel = 0;
        modXSelect = 0;
        modYSelect = 0;
        stackWriteInhibit = false;

        totalLevel = 0;
        soundDirect = false;

        octave = 0;
        freqNumSwitch = 0;

        lfoReset = false;
        lfofRaw = 0;
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

    void TriggerKeyOn() {
        if (keyOn != keyOnBit) {
            keyOn = keyOnBit;
            envGen.TriggerKey(keyOn);
        }
    }

    void Step() {
        envGen.Step();
    }

    template <typename T, bool fromM68K>
    T ReadReg(uint32 address) {
        static constexpr bool is16 = std::is_same_v<T, uint16>;
        auto shiftByte = [](uint16 value) {
            if constexpr (is16) {
                return value >> 8u;
            } else {
                return value;
            }
        };

        switch (address) {
        case 0x00: return shiftByte(ReadReg00<is16, true>());
        case 0x01: return ReadReg00<true, is16>();
        case 0x02: return shiftByte(ReadReg02());
        case 0x03: return ReadReg02();
        case 0x04: return shiftByte(ReadReg04());
        case 0x05: return ReadReg04();
        case 0x06: return shiftByte(ReadReg06());
        case 0x07: return ReadReg06();
        case 0x08: return shiftByte(ReadReg08<is16, true>());
        case 0x09: return ReadReg08<true, is16>();
        case 0x0A: return shiftByte(ReadReg0A<is16, true>());
        case 0x0B: return ReadReg0A<true, is16>();
        case 0x0C: return shiftByte(ReadReg0C<is16, true>());
        case 0x0D: return ReadReg0C<true, is16>();
        case 0x0E: return shiftByte(ReadReg0E<is16, true>());
        case 0x0F: return ReadReg0E<true, is16>();
        case 0x10: return shiftByte(ReadReg10<is16, true>());
        case 0x11: return ReadReg10<true, is16>();
        case 0x12: return shiftByte(ReadReg12<is16, true>());
        case 0x13: return ReadReg12<true, is16>();
        case 0x14: return shiftByte(ReadReg14<is16, true>());
        case 0x15: return ReadReg14<true, is16>();
        case 0x16: return shiftByte(ReadReg16<is16, true>());
        case 0x17: return ReadReg16<true, is16>();
        default: return 0;
        }
    }

    template <typename T, bool fromM68K>
    void WriteReg(uint32 address, T value) {
        static constexpr bool is16 = std::is_same_v<T, uint16>;
        uint16 value16 = value;
        if constexpr (!is16) {
            if ((address & 1) == 0) {
                value16 <<= 8u;
            }
        }

        switch (address) {
        case 0x00: WriteReg00<is16, true>(value16); break;
        case 0x01: WriteReg00<true, is16>(value16); break;
        case 0x02: WriteReg02<is16, true>(value16); break;
        case 0x03: WriteReg02<true, is16>(value16); break;
        case 0x04: WriteReg04<is16, true>(value16); break;
        case 0x05: WriteReg04<true, is16>(value16); break;
        case 0x06: WriteReg06<is16, true>(value16); break;
        case 0x07: WriteReg06<true, is16>(value16); break;
        case 0x08: WriteReg08<is16, true>(value16); break;
        case 0x09: WriteReg08<true, is16>(value16); break;
        case 0x0A: WriteReg0A<is16, true>(value16); break;
        case 0x0B: WriteReg0A<true, is16>(value16); break;
        case 0x0C: WriteReg0C<is16, true>(value16); break;
        case 0x0D: WriteReg0C<true, is16>(value16); break;
        case 0x0E: WriteReg0E<is16, true>(value16); break;
        case 0x0F: WriteReg0E<true, is16>(value16); break;
        case 0x10: WriteReg10<is16, true>(value16); break;
        case 0x11: WriteReg10<true, is16>(value16); break;
        case 0x12: WriteReg12<is16, true>(value16); break;
        case 0x13: WriteReg12<true, is16>(value16); break;
        case 0x14: WriteReg14<is16, true>(value16); break;
        case 0x15: WriteReg14<true, is16>(value16); break;
        case 0x16: WriteReg16<is16, true>(value16); break;
        case 0x17: WriteReg16<true, is16>(value16); break;
        }
    }

    template <bool lowerByte, bool upperByte, uint32 lb, uint32 ub>
    void SplitRead(uint16 &dstValue, uint16 srcValue) {
        static constexpr uint32 dstlb = lowerByte ? lb : 8;
        static constexpr uint32 dstub = upperByte ? ub : 7;

        static constexpr uint32 srclb = dstlb - lb;
        static constexpr uint32 srcub = dstub - lb;

        bit::deposit_into<dstlb, dstub>(dstValue, bit::extract<srclb, srcub>(srcValue));
    }

    template <bool lowerByte, bool upperByte, uint32 lb, uint32 ub, std::integral TDst>
    void SplitWrite(TDst &dstValue, uint16 srcValue) {
        if constexpr (lowerByte && upperByte) {
            dstValue = bit::extract<lb, ub>(srcValue);
        } else {
            static constexpr uint32 srclb = lowerByte ? lb : 8;
            static constexpr uint32 srcub = upperByte ? ub : 7;

            static constexpr uint32 dstlb = srclb - lb;
            static constexpr uint32 dstub = srcub - lb;

            bit::deposit_into<dstlb, dstub>(dstValue, bit::extract<srclb, srcub>(srcValue));
        }
    }

    template <bool lowerByte, bool upperByte>
    uint16 ReadReg00() {
        uint16 value = 0;
        if constexpr (lowerByte) {
            bit::deposit_into<0, 3>(value, bit::extract<16, 19>(startAddress));
            bit::deposit_into<4>(value, pcm8Bit);
            bit::deposit_into<5, 6>(value, static_cast<uint16>(loopControl));
        }

        SplitRead<lowerByte, upperByte, 7, 8>(value, static_cast<uint16>(soundSource));

        if constexpr (upperByte) {
            bit::deposit_into<9, 10>(value, bit::extract<14, 15>(sampleXOR));
            bit::deposit_into<11>(value, keyOnBit);
        }
        return value;
    }

    template <bool lowerByte, bool upperByte>
    void WriteReg00(uint16 value) {
        if constexpr (lowerByte) {
            bit::deposit_into<16, 19>(startAddress, bit::extract<0, 3>(value));
            pcm8Bit = bit::extract<4>(value);
            loopControl = static_cast<LoopControl>(bit::extract<5, 6>(value));
        }

        auto soundSourceValue = static_cast<uint16>(soundSource);
        SplitWrite<lowerByte, upperByte, 7, 8>(soundSourceValue, value);
        soundSource = static_cast<SoundSource>(soundSourceValue);

        if constexpr (upperByte) {
            static constexpr uint16 kSampleXORTable[] = {0x0000, 0x7FFF, 0x8000, 0xFFFF};
            sampleXOR = kSampleXORTable[bit::extract<9, 10>(value)];
            keyOnBit = bit::extract<11>(value);
            // NOTE: bit 12 is KYONEX, handled in SCSP::WriteReg
        }
    }

    uint16 ReadReg02() {
        return bit::extract<0, 15>(startAddress);
    }

    template <bool lowerByte, bool upperByte>
    void WriteReg02(uint16 value) {
        static constexpr uint32 lb = lowerByte ? 0 : 8;
        static constexpr uint32 ub = upperByte ? 15 : 7;
        bit::deposit_into<lb, ub>(startAddress, bit::extract<lb, ub>(value));
    }

    uint16 ReadReg04() {
        return loopStartAddress;
    }

    template <bool lowerByte, bool upperByte>
    void WriteReg04(uint16 value) {
        static constexpr uint32 lb = lowerByte ? 0 : 8;
        static constexpr uint32 ub = upperByte ? 15 : 7;
        bit::deposit_into<lb, ub>(loopStartAddress, bit::extract<lb, ub>(value));
    }

    uint16 ReadReg06() {
        return loopEndAddress;
    }

    template <bool lowerByte, bool upperByte>
    void WriteReg06(uint16 value) {
        static constexpr uint32 lb = lowerByte ? 0 : 8;
        static constexpr uint32 ub = upperByte ? 15 : 7;
        bit::deposit_into<lb, ub>(loopEndAddress, bit::extract<lb, ub>(value));
    }

    template <bool lowerByte, bool upperByte>
    uint16 ReadReg08() {
        uint16 value = 0;
        if constexpr (lowerByte) {
            bit::deposit_into<0, 4>(value, envGen.attackRate);
            bit::deposit_into<5>(value, envGen.egHold);
        }

        SplitRead<lowerByte, upperByte, 6, 10>(value, envGen.decay1Rate);

        if constexpr (upperByte) {
            bit::deposit_into<11, 15>(value, envGen.decay2Rate);
        }
        return value;
    }

    template <bool lowerByte, bool upperByte>
    void WriteReg08(uint16 value) {
        if constexpr (lowerByte) {
            envGen.attackRate = bit::extract<0, 4>(value);
            envGen.egHold = bit::extract<5>(value);
        }

        SplitWrite<lowerByte, upperByte, 6, 10>(envGen.decay1Rate, value);

        if constexpr (upperByte) {
            envGen.decay2Rate = bit::extract<11, 15>(value);
        }
    }

    template <bool lowerByte, bool upperByte>
    uint16 ReadReg0A() {
        uint16 value = 0;
        if constexpr (lowerByte) {
            bit::deposit_into<0, 4>(value, envGen.releaseRate);
        }

        SplitRead<lowerByte, upperByte, 5, 9>(value, envGen.decayLevel);

        if constexpr (upperByte) {
            bit::deposit_into<10, 13>(value, envGen.keyRateScaling);
            bit::deposit_into<14>(value, envGen.loopStartLink);
        }
        return value;
    }

    template <bool lowerByte, bool upperByte>
    void WriteReg0A(uint16 value) {
        if constexpr (lowerByte) {
            envGen.releaseRate = bit::extract<0, 4>(value);
        }

        SplitWrite<lowerByte, upperByte, 5, 9>(envGen.decayLevel, value);

        if constexpr (upperByte) {
            envGen.keyRateScaling = bit::extract<10, 13>(value);
            envGen.loopStartLink = bit::extract<14>(value);
        }
    }

    template <bool lowerByte, bool upperByte>
    uint16 ReadReg0C() {
        uint16 value = 0;
        if constexpr (lowerByte) {
            bit::deposit_into<0, 7>(value, totalLevel);
        }

        if constexpr (upperByte) {
            bit::deposit_into<8>(value, soundDirect);
            bit::deposit_into<9>(value, stackWriteInhibit);
        }
        return 0;
    }

    template <bool lowerByte, bool upperByte>
    void WriteReg0C(uint16 value) {
        if constexpr (lowerByte) {
            totalLevel = bit::extract<0, 7>(value);
        }

        if constexpr (upperByte) {
            soundDirect = bit::extract<8>(value);
            stackWriteInhibit = bit::extract<9>(value);
        }
    }

    template <bool lowerByte, bool upperByte>
    uint16 ReadReg0E() {
        uint16 value = 0;
        if constexpr (lowerByte) {
            bit::deposit_into<0, 5>(value, modYSelect);
        }

        SplitRead<lowerByte, upperByte, 6, 10>(value, modXSelect);

        if constexpr (upperByte) {
            bit::deposit_into<11, 15>(value, modLevel);
        }
        return value;
    }

    template <bool lowerByte, bool upperByte>
    void WriteReg0E(uint16 value) {
        if constexpr (lowerByte) {
            modYSelect = bit::extract<0, 5>(value);
        }

        SplitWrite<lowerByte, upperByte, 6, 10>(modXSelect, value);

        if constexpr (upperByte) {
            modLevel = bit::extract<11, 15>(value);
        }
    }

    template <bool lowerByte, bool upperByte>
    uint16 ReadReg10() {
        uint16 value = 0;

        SplitRead<lowerByte, upperByte, 0, 9>(value, freqNumSwitch);

        if constexpr (upperByte) {
            bit::deposit_into<11, 14>(value, octave);
        }
        return value;
    }

    template <bool lowerByte, bool upperByte>
    void WriteReg10(uint16 value) {
        SplitWrite<lowerByte, upperByte, 0, 9>(freqNumSwitch, value);

        if constexpr (upperByte) {
            octave = bit::extract<11, 14>(value);
        }
    }

    template <bool lowerByte, bool upperByte>
    uint16 ReadReg12() {
        uint16 value = 0;
        if constexpr (lowerByte) {
            bit::deposit_into<0, 2>(value, ampLFOSens);
            bit::deposit_into<3, 4>(value, static_cast<uint16>(ampLFOWaveform));
            bit::deposit_into<5, 7>(value, pitchLFOSens);
        }

        if constexpr (upperByte) {
            bit::deposit_into<8, 9>(value, static_cast<uint16>(pitchLFOWaveform));
            bit::deposit_into<10, 14>(value, lfofRaw);
            bit::deposit_into<15>(value, lfoReset);
        }
        return value;
    }

    template <bool lowerByte, bool upperByte>
    void WriteReg12(uint16 value) {
        if constexpr (lowerByte) {
            ampLFOSens = bit::extract<0, 2>(value);
            ampLFOWaveform = static_cast<Waveform>(bit::extract<3, 4>(value));
            pitchLFOSens = bit::extract<5, 7>(value);
        }

        if constexpr (upperByte) {
            pitchLFOWaveform = static_cast<Waveform>(bit::extract<8, 9>(value));
            lfofRaw = bit::extract<10, 14>(value);
            lfoReset = bit::extract<15>(value);
        }
    }

    template <bool lowerByte, bool upperByte>
    uint16 ReadReg14() {
        uint16 value = 0;
        if constexpr (lowerByte) {
            bit::deposit_into<0, 2>(value, inputMixingLevel);
            bit::deposit_into<3, 6>(value, inputSelect);
        }
        return value;
    }

    template <bool lowerByte, bool upperByte>
    void WriteReg14(uint16 value) {
        if constexpr (lowerByte) {
            inputMixingLevel = bit::extract<0, 2>(value);
            inputSelect = bit::extract<3, 6>(value);
        }
    }

    template <bool lowerByte, bool upperByte>
    uint16 ReadReg16() {
        uint16 value = 0;
        if constexpr (lowerByte) {
            bit::deposit_into<0, 4>(value, effectPan);
            bit::deposit_into<5, 7>(value, effectSendLevel);
        }
        if constexpr (upperByte) {
            bit::deposit_into<8, 12>(value, directPan);
            bit::deposit_into<13, 15>(value, directSendLevel);
        }
        return value;
    }

    template <bool lowerByte, bool upperByte>
    void WriteReg16(uint16 value) {
        if constexpr (lowerByte) {
            effectPan = bit::extract<0, 4>(value);
            effectSendLevel = bit::extract<5, 7>(value);
        }
        if constexpr (upperByte) {
            directPan = bit::extract<8, 12>(value);
            directSendLevel = bit::extract<13, 15>(value);
        }
    }

    // -------------------------------------------------------------------------
    // Registers

    // --- Loop Control Register ---

    uint32 startAddress;     // (R/W) SA - Start Address
    uint32 loopStartAddress; // (R/W) LSA - Loop Start Address
    uint32 loopEndAddress;   // (R/W) LEA - Loop End Address
    bool pcm8Bit;            // (R/W) PCM8B - Wave format (true=8-bit PCM, false=16-bit PCM)
    bool keyOnBit;           // (R/W) KYONB - Key On Bit

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
    // Implementation notes:
    //   SBCTL0: 0x7FFF
    //   SBCTL1: 0x8000
    uint16 sampleXOR; // (R/W) SBCTL0/1

    enum class SoundSource { SoundRAM, Noise, Silence, Unknown };
    SoundSource soundSource; // (R/W) SSCTL

    // --- Envelope Generator Register ---

    EnvelopeGenerator envGen;

    // --- FM Modulation Control Register ---

    uint8 modLevel;         // (R/W) MDL - add +- n * pi where n is:
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

    bool lfoReset;             // (R/W) LFORE - true resets the LFO (TODO: is this a one-shot action?)
    uint8 lfofRaw;             // (R/W) LFOF - 0x00 to 0x1F (raw value)
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
        incrementMask = (1ull << incrementInterval) - 1;
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

// -----------------------------------------------------------------------------
// DSP

union DSPInstr {
    uint64 u64;
    uint16 u16[4];
    struct {
        uint64 NXADDR : 1; // 0
        uint64 ADRGB : 1;  // 1
        uint64 MASA : 5;   // 2-6
        uint64 : 1;        // 7
        uint64 NOFL : 1;   // 8
        uint64 CRA : 6;    // 9-14
        uint64 : 1;        // 15
        uint64 BSEL : 1;   // 16
        uint64 ZERO : 1;   // 17
        uint64 NEGB : 1;   // 18
        uint64 YRL : 1;    // 19
        uint64 SHFT0 : 1;  // 20
        uint64 SHFT1 : 1;  // 21
        uint64 FRCL : 1;   // 22
        uint64 ADRL : 1;   // 23
        uint64 EWA : 3;    // 24-27
        uint64 EWT : 1;    // 28
        uint64 MRT : 1;    // 29
        uint64 MWT : 1;    // 30
        uint64 TABLE : 1;  // 31
        uint64 IWA : 5;    // 32-36
        uint64 IWT : 1;    // 37
        uint64 IRA : 6;    // 38-43
        uint64 : 1;        // 44
        uint64 YSEL : 2;   // 45-46
        uint64 TWT : 1;    // 55
        uint64 TRA : 7;    // 56-62
        uint64 : 2;        // 63-64
    };
};

} // namespace satemu::scsp
