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

namespace v2 {

    using v1::SCSPSlotState;

} // namespace v2

inline namespace v3 {

    struct SCSPSlotState {
        uint32 SA;
        uint32 LSA;
        uint32 LEA;
        bool PCM8B;
        bool KYONB;
        uint16 SBCTL;

        using LoopControl = v2::SCSPSlotState::LoopControl;
        LoopControl LPCTL;

        using SoundSource = v2::SCSPSlotState::SoundSource;
        SoundSource SSCTL;

        uint8 AR;
        uint8 D1R;
        uint8 D2R;
        uint8 RR;
        uint8 DL;

        uint8 KRS;
        bool EGHOLD;
        bool LPSLNK;
        bool EGBYPASS;

        uint8 MDL;
        uint8 MDXSL;
        uint8 MDYSL;
        bool STWINH;

        uint8 TL;
        bool SDIR;

        uint8 OCT;
        uint16 FNS;

        using Waveform = v2::SCSPSlotState::Waveform;
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

        uint16 extra0C;
        uint16 extra10;
        uint16 extra14;

        bool active;

        using EGState = v2::SCSPSlotState::EGState;
        EGState egState;

        uint16 egLevel;

        uint32 sampleCount;
        uint32 currAddress;
        uint32 currSample;
        uint32 currPhase;
        uint32 nextPhase;
        bool reverse;
        bool crossedLoopStart;

        uint32 lfoCycles;
        uint8 lfoStep;

        sint16 sample1;
        sint16 sample2;
        sint16 output;

        void Upgrade(const v2::SCSPSlotState &state) {
            SA = state.SA;
            LSA = state.LSA;
            LEA = state.LEA;
            PCM8B = state.PCM8B;
            KYONB = state.KYONB;
            SBCTL = 0;

            LPCTL = state.LPCTL;

            SSCTL = state.SSCTL;

            AR = state.AR;
            D1R = state.D1R;
            D2R = state.D2R;
            RR = state.RR;
            DL = state.DL;

            KRS = state.KRS;
            EGHOLD = state.EGHOLD;
            LPSLNK = state.LPSLNK;
            EGBYPASS = false;

            MDL = state.MDL;
            MDXSL = state.MDXSL;
            MDYSL = state.MDYSL;
            STWINH = state.STWINH;

            TL = state.TL;
            SDIR = state.SDIR;

            OCT = state.OCT;
            FNS = state.FNS;

            LFORE = state.LFORE;
            LFOF = state.LFOF;
            ALFOS = state.ALFOS;
            PLFOS = state.PLFOS;
            ALFOWS = state.ALFOWS;
            PLFOWS = state.PLFOWS;

            IMXL = state.IMXL;
            ISEL = state.ISEL;
            DISDL = state.DISDL;
            DIPAN = state.DIPAN;

            EFSDL = state.EFSDL;
            EFPAN = state.EFPAN;

            extra0C = 0;
            extra10 = 0;
            extra14 = 0;

            active = state.active;

            egState = state.egState;

            egLevel = state.egLevel;

            sampleCount = state.sampleCount;
            currAddress = state.currAddress;
            currSample = state.currSample;
            currPhase = state.currPhase >> 4u;
            nextPhase = currPhase;
            reverse = state.reverse;
            crossedLoopStart = state.crossedLoopStart;

            lfoCycles = state.lfoCycles;
            lfoStep = state.lfoStep;

            sample1 = state.sample1;
            sample2 = state.sample2;
            output = state.output;
        }
    };

} // namespace v3

} // namespace ymir::state
