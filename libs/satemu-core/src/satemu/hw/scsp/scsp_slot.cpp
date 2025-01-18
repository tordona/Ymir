#include <satemu/hw/scsp/scsp_slot.hpp>

#include <satemu/util/data_ops.hpp>

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

    currAddress = 0;
    addressInc = 0;
    latchedLoopStartAddress = 0;
    latchedLoopEndAddress = 0;
    sampleCount = 0;
}

void Slot::TriggerKeyOn() {
    if (keyOn != keyOnBit) {
        keyOn = keyOnBit;
        envGen.TriggerKey(keyOn);
        if (keyOn) {
            // Latch parameters
            currAddress = startAddress;
            addressInc = ((0x400 + freqNumSwitch) << 7u) >> (15u - ((octave + 8u) & 15u));
            latchedLoopStartAddress = loopStartAddress;
            latchedLoopEndAddress = loopEndAddress;
            sampleCount = 0;
        }
    }
}

void Slot::Step() {
    if (!envGen.Step()) {
        return;
    }

    // TODO: scale step according to pitch LFO + FM
    sampleCount += addressInc;
    // TODO: increment currAddress and obey loop parameters
}

template <typename T>
T Slot::ReadReg(uint32 address) {
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
        bit::deposit_into<0, 4>(value, envGen.attackRate);
        bit::deposit_into<5>(value, envGen.egHold);
    }

    util::SplitReadWord<lowerByte, upperByte, 6, 10>(value, envGen.decay1Rate);

    if constexpr (upperByte) {
        bit::deposit_into<11, 15>(value, envGen.decay2Rate);
    }
    return value;
}

template <bool lowerByte, bool upperByte>
void Slot::WriteReg08(uint16 value) {
    if constexpr (lowerByte) {
        envGen.attackRate = bit::extract<0, 4>(value);
        envGen.egHold = bit::extract<5>(value);
    }

    util::SplitWriteWord<lowerByte, upperByte, 6, 10>(envGen.decay1Rate, value);

    if constexpr (upperByte) {
        envGen.decay2Rate = bit::extract<11, 15>(value);
    }
}

template <bool lowerByte, bool upperByte>
uint16 Slot::ReadReg0A() {
    uint16 value = 0;
    if constexpr (lowerByte) {
        bit::deposit_into<0, 4>(value, envGen.releaseRate);
    }

    util::SplitReadWord<lowerByte, upperByte, 5, 9>(value, envGen.decayLevel);

    if constexpr (upperByte) {
        bit::deposit_into<10, 13>(value, envGen.keyRateScaling);
        bit::deposit_into<14>(value, envGen.loopStartLink);
    }
    return value;
}

template <bool lowerByte, bool upperByte>
void Slot::WriteReg0A(uint16 value) {
    if constexpr (lowerByte) {
        envGen.releaseRate = bit::extract<0, 4>(value);
    }

    util::SplitWriteWord<lowerByte, upperByte, 5, 9>(envGen.decayLevel, value);

    if constexpr (upperByte) {
        envGen.keyRateScaling = bit::extract<10, 13>(value);
        envGen.loopStartLink = bit::extract<14>(value);
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

    util::SplitReadWord<lowerByte, upperByte, 6, 10>(value, modXSelect);

    if constexpr (upperByte) {
        bit::deposit_into<11, 15>(value, modLevel);
    }
    return value;
}

template <bool lowerByte, bool upperByte>
void Slot::WriteReg0E(uint16 value) {
    if constexpr (lowerByte) {
        modYSelect = bit::extract<0, 5>(value);
    }

    util::SplitWriteWord<lowerByte, upperByte, 6, 10>(modXSelect, value);

    if constexpr (upperByte) {
        modLevel = bit::extract<11, 15>(value);
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

} // namespace satemu::scsp
