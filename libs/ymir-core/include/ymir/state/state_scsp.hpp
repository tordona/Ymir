#pragma once

#include "state_m68k.hpp"
#include "state_scsp_dsp.hpp"
#include "state_scsp_slot.hpp"
#include "state_scsp_timer.hpp"

#include <ymir/hw/m68k/m68k_defs.hpp>

#include <ymir/core/types.hpp>

#include <array>

namespace ymir::state {

namespace v1 {

    struct SCSPState {
        alignas(16) std::array<uint8, m68k::kM68KWRAMSize> WRAM;

        alignas(16) std::array<uint8, 2048 * 75> cddaBuffer;
        uint32 cddaReadPos;
        uint32 cddaWritePos;
        bool cddaReady;

        M68KState m68k;
        uint64 m68kSpilloverCycles;
        bool m68kEnabled;

        alignas(16) std::array<SCSPSlotState, 32> slots;

        uint32 MVOL;
        bool DAC18B;
        bool MEM4MB;
        uint8 MSLC;

        std::array<SCSPTimer, 3> timers;

        uint16 MCIEB;
        uint16 MCIPD;
        uint16 SCIEB;
        uint16 SCIPD;

        bool DEXE;
        bool DDIR;
        bool DGATE;
        uint32 DMEA;
        uint16 DRGA;
        uint16 DTLG;

        alignas(16) std::array<uint16, 64> SOUS;
        uint32 soundStackIndex;

        SCSPDSP dsp;

        uint64 m68kCycles;
        uint64 sampleCycles;
        uint64 sampleCounter;

        uint16 egCycle;
        bool egStep;

        uint32 lfsr;
    };

} // namespace v1

namespace v2 {

    using v1::SCSPState;

} // namespace v2

inline namespace v3 {

    struct SCSPState {
        alignas(16) std::array<uint8, m68k::kM68KWRAMSize> WRAM;

        alignas(16) std::array<uint8, 2048 * 75> cddaBuffer;
        uint32 cddaReadPos;
        uint32 cddaWritePos;
        bool cddaReady;

        M68KState m68k;
        uint64 m68kSpilloverCycles;
        bool m68kEnabled;

        alignas(16) std::array<SCSPSlotState, 32> slots;

        uint32 MVOL;
        bool DAC18B;
        bool MEM4MB;
        uint8 MSLC;

        std::array<SCSPTimer, 3> timers;

        uint16 MCIEB;
        uint16 MCIPD;
        uint16 SCIEB;
        uint16 SCIPD;

        bool DEXE;
        bool DDIR;
        bool DGATE;
        uint32 DMEA;
        uint16 DRGA;
        uint16 DTLG;

        alignas(16) std::array<uint16, 64> SOUS;
        uint32 soundStackIndex;

        SCSPDSP dsp;

        uint64 m68kCycles;
        uint64 sampleCycles;
        uint64 sampleCounter;

        uint16 egCycle;
        bool egStep;

        uint32 lfsr;

        void Upgrade(const v2::SCSPState &state) {
            WRAM = state.WRAM;

            cddaBuffer = state.cddaBuffer;
            cddaReadPos = state.cddaReadPos;
            cddaWritePos = state.cddaWritePos;
            cddaReady = state.cddaReady;

            m68k = state.m68k;
            m68kSpilloverCycles = state.m68kSpilloverCycles;
            m68kEnabled = state.m68kEnabled;

            for (size_t i = 0; i < slots.size(); i++) {
                slots[i].Upgrade(state.slots[i]);
            }

            MVOL = state.MVOL;
            DAC18B = state.DAC18B;
            MEM4MB = state.MEM4MB;
            MSLC = state.MSLC;

            timers = state.timers;

            MCIEB = state.MCIEB;
            MCIPD = state.MCIPD;
            SCIEB = state.SCIEB;
            SCIPD = state.SCIPD;

            DEXE = state.DEXE;
            DDIR = state.DDIR;
            DGATE = state.DGATE;
            DMEA = state.DMEA;
            DRGA = state.DRGA;
            DTLG = state.DTLG;

            SOUS = state.SOUS;
            soundStackIndex = state.soundStackIndex;

            dsp = state.dsp;

            m68kCycles = state.m68kCycles;
            sampleCycles = state.sampleCycles;
            sampleCounter = state.sampleCounter;

            egCycle = state.egCycle;
            egStep = state.egStep;

            lfsr = state.lfsr;
        }
    };

} // namespace v3

} // namespace ymir::state
