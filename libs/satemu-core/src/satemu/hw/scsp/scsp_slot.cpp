#include <satemu/hw/scsp/scsp_slot.hpp>

#include <satemu/util/data_ops.hpp>

#include <algorithm>

namespace satemu::scsp {

Slot::Slot() {
    Reset();
}

void Slot::Reset() {
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

    egHold = false;

    loopStartLink = false;

    modLevel = 0;
    modXSelect = 0;
    modYSelect = 0;
    stackWriteInhibit = false;

    totalLevel = 0;
    soundDirect = false;

    octave = 0;
    freqNumSwitch = 0x400u;

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

    active = false;

    egState = EGState::Release;

    egLevel = 0x3FF;

    sampleCount = 0;
    currAddress = 0;
    currSample = 0;
    currPhase = 0;
    reverse = false;
    crossedLoopStart = false;

    sample1 = 0;
    sample2 = 0;

    output = 0;
}

bool Slot::TriggerKey() {
    // Key ON only triggers when EG is in Release state
    // Key OFF only triggers when EG is in any other state
    const bool trigger = (egState == EGState::Release) == keyOnBit;
    if (trigger) {
        if (keyOnBit) {
            active = true;

            egState = EGState::Attack;

            if (keyRateScaling == 0xF) {
                egLevel = 0x280;
            } else {
                const sint8 oct = static_cast<sint8>(octave ^ 8) - 8;
                const uint8 krs = std::clamp<uint8>(keyRateScaling + oct, 0x0, 0xF);
                egLevel = (attackRate + krs >= 0x20) ? 0x000 : 0x280;
            }

            sampleCount = 0;
            currAddress = 0;
            currSample = 0;
            currPhase = 0;
            reverse = false;
            crossedLoopStart = false;

            sample1 = 0;
            sample2 = 0;

            output = 0;
        } else {
            egState = EGState::Release;
        }
    }
    return trigger;
}

template <typename T>
T Slot::ReadReg(uint32 address) {
    static constexpr bool is16 = std::is_same_v<T, uint16>;
    auto shiftByte = [](uint16 value) {
        if constexpr (is16) {
            return value;
        } else {
            return value >> 8u;
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

template uint8 Slot::ReadReg<uint8>(uint32 address);
template uint16 Slot::ReadReg<uint16>(uint32 address);

template <typename T>
void Slot::WriteReg(uint32 address, T value) {
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

template void Slot::WriteReg<uint8>(uint32 address, uint8 value);
template void Slot::WriteReg<uint16>(uint32 address, uint16 value);

template <bool lowerByte, bool upperByte>
uint16 Slot::ReadReg00() {
    uint16 value = 0;
    if constexpr (lowerByte) {
        bit::deposit_into<0, 3>(value, bit::extract<16, 19>(startAddress));
        bit::deposit_into<4>(value, pcm8Bit);
        bit::deposit_into<5, 6>(value, static_cast<uint16>(loopControl));
    }

    util::SplitReadWord<lowerByte, upperByte, 7, 8>(value, static_cast<uint16>(soundSource));

    if constexpr (upperByte) {
        bit::deposit_into<9, 10>(value, bit::extract<14, 15>(sampleXOR));
        bit::deposit_into<11>(value, keyOnBit);
    }
    return value;
}

template <bool lowerByte, bool upperByte>
void Slot::WriteReg00(uint16 value) {
    if constexpr (lowerByte) {
        bit::deposit_into<16, 19>(startAddress, bit::extract<0, 3>(value));
        pcm8Bit = bit::extract<4>(value);
        loopControl = static_cast<LoopControl>(bit::extract<5, 6>(value));
    }

    auto soundSourceValue = static_cast<uint16>(soundSource);
    util::SplitWriteWord<lowerByte, upperByte, 7, 8>(soundSourceValue, value);
    soundSource = static_cast<SoundSource>(soundSourceValue);

    if constexpr (upperByte) {
        static constexpr uint16 kSampleXORTable[] = {0x0000, 0x7FFF, 0x8000, 0xFFFF};
        sampleXOR = kSampleXORTable[bit::extract<9, 10>(value)];
        keyOnBit = bit::extract<11>(value);
        // NOTE: bit 12 is KYONEX, handled in SCSP::WriteReg
    }
}

uint16 Slot::ReadReg02() {
    return bit::extract<0, 15>(startAddress);
}

template <bool lowerByte, bool upperByte>
void Slot::WriteReg02(uint16 value) {
    static constexpr uint32 lb = lowerByte ? 0 : 8;
    static constexpr uint32 ub = upperByte ? 15 : 7;
    bit::deposit_into<lb, ub>(startAddress, bit::extract<lb, ub>(value));
}

uint16 Slot::ReadReg04() {
    return loopStartAddress;
}

template <bool lowerByte, bool upperByte>
void Slot::WriteReg04(uint16 value) {
    static constexpr uint32 lb = lowerByte ? 0 : 8;
    static constexpr uint32 ub = upperByte ? 15 : 7;
    bit::deposit_into<lb, ub>(loopStartAddress, bit::extract<lb, ub>(value));
}

uint16 Slot::ReadReg06() {
    return loopEndAddress;
}

template <bool lowerByte, bool upperByte>
void Slot::WriteReg06(uint16 value) {
    static constexpr uint32 lb = lowerByte ? 0 : 8;
    static constexpr uint32 ub = upperByte ? 15 : 7;
    bit::deposit_into<lb, ub>(loopEndAddress, bit::extract<lb, ub>(value));
}

template <bool lowerByte, bool upperByte>
uint16 Slot::ReadReg08() {
    uint16 value = 0;
    if constexpr (lowerByte) {
        bit::deposit_into<0, 4>(value, attackRate);
        bit::deposit_into<5>(value, egHold);
    }

    util::SplitReadWord<lowerByte, upperByte, 6, 10>(value, decay1Rate);

    if constexpr (upperByte) {
        bit::deposit_into<11, 15>(value, decay2Rate);
    }
    return value;
}

template <bool lowerByte, bool upperByte>
void Slot::WriteReg08(uint16 value) {
    if constexpr (lowerByte) {
        attackRate = bit::extract<0, 4>(value);
        egHold = bit::extract<5>(value);
    }

    util::SplitWriteWord<lowerByte, upperByte, 6, 10>(decay1Rate, value);

    if constexpr (upperByte) {
        decay2Rate = bit::extract<11, 15>(value);
    }
}

template <bool lowerByte, bool upperByte>
uint16 Slot::ReadReg0A() {
    uint16 value = 0;
    if constexpr (lowerByte) {
        bit::deposit_into<0, 4>(value, releaseRate);
    }

    util::SplitReadWord<lowerByte, upperByte, 5, 9>(value, decayLevel);

    if constexpr (upperByte) {
        bit::deposit_into<10, 13>(value, keyRateScaling);
        bit::deposit_into<14>(value, loopStartLink);
    }
    return value;
}

template <bool lowerByte, bool upperByte>
void Slot::WriteReg0A(uint16 value) {
    if constexpr (lowerByte) {
        releaseRate = bit::extract<0, 4>(value);
    }

    util::SplitWriteWord<lowerByte, upperByte, 5, 9>(decayLevel, value);

    if constexpr (upperByte) {
        keyRateScaling = bit::extract<10, 13>(value);
        loopStartLink = bit::extract<14>(value);
    }
}

template <bool lowerByte, bool upperByte>
uint16 Slot::ReadReg0C() {
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
void Slot::WriteReg0C(uint16 value) {
    if constexpr (lowerByte) {
        totalLevel = bit::extract<0, 7>(value);
    }

    if constexpr (upperByte) {
        soundDirect = bit::extract<8>(value);
        stackWriteInhibit = bit::extract<9>(value);
    }
}

template <bool lowerByte, bool upperByte>
uint16 Slot::ReadReg0E() {
    uint16 value = 0;
    if constexpr (lowerByte) {
        bit::deposit_into<0, 5>(value, modYSelect);
    }

    util::SplitReadWord<lowerByte, upperByte, 6, 11>(value, modXSelect);

    if constexpr (upperByte) {
        bit::deposit_into<12, 15>(value, modLevel);
    }
    return value;
}

template <bool lowerByte, bool upperByte>
void Slot::WriteReg0E(uint16 value) {
    if constexpr (lowerByte) {
        modYSelect = bit::extract<0, 5>(value);
    }

    util::SplitWriteWord<lowerByte, upperByte, 6, 11>(modXSelect, value);

    if constexpr (upperByte) {
        modLevel = bit::extract<12, 15>(value);
    }
}

template <bool lowerByte, bool upperByte>
uint16 Slot::ReadReg10() {
    uint16 value = 0;

    util::SplitReadWord<lowerByte, upperByte, 0, 9>(value, freqNumSwitch);

    if constexpr (upperByte) {
        bit::deposit_into<11, 14>(value, octave);
    }
    return value;
}

template <bool lowerByte, bool upperByte>
void Slot::WriteReg10(uint16 value) {
    util::SplitWriteWord<lowerByte, upperByte, 0, 9>(freqNumSwitch, value);

    if constexpr (upperByte) {
        octave = bit::extract<11, 14>(value);
    }
}

template <bool lowerByte, bool upperByte>
uint16 Slot::ReadReg12() {
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
void Slot::WriteReg12(uint16 value) {
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
uint16 Slot::ReadReg14() {
    uint16 value = 0;
    if constexpr (lowerByte) {
        bit::deposit_into<0, 2>(value, inputMixingLevel);
        bit::deposit_into<3, 6>(value, inputSelect);
    }
    return value;
}

template <bool lowerByte, bool upperByte>
void Slot::WriteReg14(uint16 value) {
    if constexpr (lowerByte) {
        inputMixingLevel = bit::extract<0, 2>(value);
        inputSelect = bit::extract<3, 6>(value);
    }
}

template <bool lowerByte, bool upperByte>
uint16 Slot::ReadReg16() {
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
void Slot::WriteReg16(uint16 value) {
    if constexpr (lowerByte) {
        effectPan = bit::extract<0, 4>(value);
        effectSendLevel = bit::extract<5, 7>(value);
    }
    if constexpr (upperByte) {
        directPan = bit::extract<8, 12>(value);
        directSendLevel = bit::extract<13, 15>(value);
    }
}

uint32 Slot::CalcEffectiveRate(uint8 rate) const {
    uint32 effectiveRate = rate << 1;
    if (keyRateScaling == 0xF) {
        return std::min<uint32>(effectiveRate, 0x3F);
    }
    const sint32 krsRate = keyRateScaling + static_cast<sint32>(octave ^ 8) - 8;
    if (krsRate >= 0) {
        effectiveRate += (krsRate << 1) + ((freqNumSwitch >> 9) & 1);
    }
    return std::min<uint32>(effectiveRate, 0x3F);
}

uint8 Slot::GetCurrentEGRate() const {
    switch (egState) {
    case EGState::Attack: return attackRate;
    case EGState::Decay1: return decay1Rate;
    case EGState::Decay2: return decay2Rate;
    case EGState::Release: return releaseRate;
    default: return releaseRate; // should not happen
    }
}

uint16 Slot::GetEGLevel() const {
    if (egState == EGState::Attack && egHold) {
        return 0x000;
    } else {
        return egLevel;
    }
}

void Slot::IncrementPhase(uint32 pitchLFO) {
    // NOTE: freqNumSwitch already has 0x400u added to it
    const uint32 phaseInc = freqNumSwitch << (octave ^ 8u);
    currPhase = (currPhase & 0x3FFFF) + phaseInc + pitchLFO;
}

void Slot::IncrementSampleCounter() {
    if (reverse) {
        currSample -= currPhase >> 18u;
    } else {
        currSample += currPhase >> 18u;
        if (!crossedLoopStart && currSample >= loopStartAddress) {
            crossedLoopStart = true;
            if (loopStartLink && egState == EGState::Attack) {
                egState = EGState::Decay1;
            }
        }
    }

    switch (loopControl) {
    case LoopControl::Off:
        if (currSample >= loopEndAddress) {
            active = false;
            keyOnBit = false;
        }
        break;
    case LoopControl::Normal:
        while (currSample >= loopEndAddress) {
            currSample -= loopEndAddress - loopStartAddress;
        }
        break;
    case LoopControl::Reverse:
        if (reverse) {
            while (currSample <= loopStartAddress) {
                currSample += loopEndAddress - loopStartAddress;
            }
        } else {
            if (currSample >= loopStartAddress) {
                reverse = true;
                currSample = loopEndAddress - currSample + loopStartAddress;
            }
        }
        break;
    case LoopControl::Alternate:
        if (reverse) {
            while (currSample <= loopStartAddress) {
                reverse = false;
                currSample += loopEndAddress - loopStartAddress;
            }
        } else {
            while (currSample >= loopEndAddress) {
                reverse = true;
                currSample -= loopEndAddress - loopStartAddress;
            }
        }
        break;
    }
}

void Slot::IncrementAddress(sint32 modulation) {
    const uint32 addressInc = (currSample + modulation) << (pcm8Bit ? 0 : 1);
    currAddress = startAddress + addressInc;
}

} // namespace satemu::scsp
