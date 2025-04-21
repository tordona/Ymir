#pragma once

#include <ymir/core/types.hpp>

namespace ymir::state {

namespace v1 {

    struct SCSPSlotState {
        uint32 SA;
        uint32 LSA;
        uint32 LEA;
        bool PCM8B;
        bool KYONB;

        enum class LoopControl { Off, Normal, Reverse, Alternate };
        LoopControl LPCTL;

        enum class SoundSource { SoundRAM, Noise, Silence, Unknown };
        SoundSource SSCTL;

        uint8 AR;
        uint8 D1R;
        uint8 D2R;
        uint8 RR;
        uint8 DL;

        uint8 KRS;
        bool EGHOLD;
        bool LPSLNK;

        uint8 MDL;
        uint8 MDXSL;
        uint8 MDYSL;
        bool STWINH;

        uint8 TL;
        bool SDIR;

        uint8 OCT;
        uint16 FNS;

        enum class Waveform : uint8 { Saw, Square, Triangle, Noise };
        bool LFORE;
        uint8 LFOF;
        uint8 ALFOS;
        uint8 PLFOS;
        Waveform ALFOWS;
        Waveform PLFOWS;

        uint8 IMXL;
        uint8 ISEL;
        uint8 DISDL;
        uint8 DIPAN;

        uint8 EFSDL;
        uint8 EFPAN;

        bool active;

        enum class EGState : uint8 { Attack, Decay1, Decay2, Release };
        EGState egState;

        uint16 egLevel;

        uint32 sampleCount;
        uint32 currAddress;
        uint32 currSample;
        uint32 currPhase;
        bool reverse;
        bool crossedLoopStart;

        uint32 lfoCycles;
        uint8 lfoStep;

        sint16 sample1;
        sint16 sample2;
        sint16 output;
    };

} // namespace v1

inline namespace v2 {

    using v1::SCSPSlotState;

} // namespace v2

} // namespace ymir::state
