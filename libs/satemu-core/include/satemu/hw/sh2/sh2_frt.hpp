#pragma once

#include <satemu/core/types.hpp>

#include <satemu/util/bit_ops.hpp>
#include <satemu/util/inline.hpp>

namespace satemu::sh2 {

struct FreeRunningTimer {
    static constexpr uint64 kDividerShifts[] = {3, 5, 7, 0};

    enum class Event { None, OVI, OCI };

    FreeRunningTimer() {
        Reset();
    }

    void Reset() {
        TIER.u8 = 0x01;
        FTCSR.u8 = 0x00;
        FRC = 0x0000;
        OCRA = OCRB = 0xFFFF;
        TCR.u8 = 0x00;
        TOCR.u8 = 0x00;
        ICR = 0x0000;

        TEMP = 0x00;

        m_cycleCount = 0;
        m_clockDividerShift = kDividerShifts[TCR.CKSn];
        m_cycleCountMask = (1ull << m_clockDividerShift) - 1;
    }

    FORCE_INLINE Event Advance(uint64 cycles) {
        m_cycleCount += cycles;
        const uint64 steps = m_cycleCount >> m_clockDividerShift;
        m_cycleCount -= steps << m_clockDividerShift;

        Event event = Event::None;

        uint64 nextFRC = FRC + steps;
        if (FRC < OCRA && nextFRC >= OCRA) {
            FTCSR.OCFA = TOCR.OLVLA;
            if (FTCSR.CCLRA) {
                nextFRC = 0;
            }
            if (TIER.OCIAE) {
                event = Event::OCI;
            }
        }
        if (FRC < OCRB && nextFRC >= OCRB) {
            FTCSR.OCFB = TOCR.OLVLB;
            if (TIER.OCIBE) {
                event = Event::OCI;
            }
        }
        if (nextFRC >= 0x10000) {
            FTCSR.OVF = 1;
            if (TIER.OVIE) {
                event = Event::OVI;
            }
        }
        FRC = nextFRC;

        return event;
    }

    FORCE_INLINE uint64 CyclesUntilNextTick() const {
        return (1ull << m_clockDividerShift) - (m_cycleCount & m_cycleCountMask);
    }

    // -------------------------------------------------------------------------
    // Registers

    // addr r/w  access   init      code    name
    // 010  R/W  8        01        TIER    Timer interrupt enable register
    //
    //   bits   r/w  code     description
    //      7   R/W  ICIE     Input Capture Interrupt Enable
    //    6-4   R/W  -        Reserved - must be zero
    //      3   R/W  OCIAE    Output Compare Interrupt A Enable
    //      2   R/W  OCIBE    Output Compare Interrupt B Enable
    //      1   R/W  OVIE     Timer Overflow Interrupt Enable
    //      0   R/W  -        Reserved - must be one
    union RegTIER {
        uint8 u8;
        struct {
            uint8 : 1;
            uint8 OVIE : 1;
            uint8 OCIAE : 1;
            uint8 OCIBE : 1;
            uint8 : 3;
            uint8 ICIE : 1;
        };
    } TIER;

    FORCE_INLINE uint8 ReadTIER() const {
        return TIER.u8;
    }

    FORCE_INLINE void WriteTIER(uint8 value) {
        TIER.u8 = (value & 0x8E) | 1;
    }

    // 011  R/W  8        00        FTCSR   Free-running timer control/status register
    //
    //   bits   r/w  code     description
    //      7   R/W  ICF      Input Capture Flag (clear on zero write)
    //    6-4   R/W  -        Reserved - must be zero
    //      3   R/W  OCFA     Output Compare Flag A (clear on zero write)
    //      2   R/W  OCFB     Output Compare Flag B (clear on zero write)
    //      1   R/W  OVF      Timer Overflow Flag (clear on zero write)
    //      0   R/W  CCLRA    Counter Clear A
    union RegFTCSR {
        uint8 u8;
        struct {
            uint8 CCLRA : 1;
            uint8 OVF : 1;
            uint8 OCFB : 1;
            uint8 OCFA : 1;
            uint8 : 3;
            uint8 ICF : 1;
        };
    } FTCSR;

    FORCE_INLINE uint8 ReadFTCSR() const {
        return FTCSR.u8;
    }

    template <bool poke>
    FORCE_INLINE void WriteFTCSR(uint8 value) {
        if constexpr (poke) {
            FTCSR.ICF = bit::extract<7>(value);
            FTCSR.OCFA = bit::extract<3>(value);
            FTCSR.OCFB = bit::extract<2>(value);
            FTCSR.OVF = bit::extract<1>(value);
            FTCSR.CCLRA = bit::extract<0>(value);
        } else {
            FTCSR.ICF &= bit::extract<7>(value);
            FTCSR.OCFA &= bit::extract<3>(value);
            FTCSR.OCFB &= bit::extract<2>(value);
            FTCSR.OVF &= bit::extract<1>(value);
            FTCSR.CCLRA = bit::extract<0>(value);
        }
    }

    // 012  R/W  8        00        FRC H     Free-running counter H
    // 013  R/W  8        00        FRC L     Free-running counter L
    uint16 FRC;

    template <bool peek>
    FORCE_INLINE uint8 ReadFRCH() const {
        if constexpr (!peek) {
            TEMP = FRC;
        }
        return FRC >> 8u;
    }

    template <bool peek>
    FORCE_INLINE uint8 ReadFRCL() const {
        if constexpr (peek) {
            return FRC;
        } else {
            return TEMP;
        }
    }

    template <bool poke>
    FORCE_INLINE void WriteFRCH(uint8 value) {
        if constexpr (poke) {
            bit::deposit_into<8, 15>(FRC, value);
        } else {
            TEMP = value;
        }
    }

    template <bool poke>
    FORCE_INLINE void WriteFRCL(uint8 value) {
        if constexpr (poke) {
            bit::deposit_into<0, 7>(FRC, value);
        } else {
            FRC = value | (TEMP << 8u);
        }
    }

    // 014  R/W  8        FF        OCRA/B H  Output compare register A/B H
    // 015  R/W  8        FF        OCRA/B L  Output compare register A/B L
    uint16 OCRA, OCRB;

    uint16 &CurrOCR() {
        return TOCR.OCRS ? OCRB : OCRA;
    }

    uint16 CurrOCR() const {
        return TOCR.OCRS ? OCRB : OCRA;
    }

    FORCE_INLINE uint8 ReadOCRH() const {
        return CurrOCR() >> 8u;
    }

    FORCE_INLINE uint8 ReadOCRL() const {
        return CurrOCR() >> 0u;
    }

    template <bool poke>
    FORCE_INLINE void WriteOCRH(uint8 value) {
        if constexpr (poke) {
            bit::deposit_into<8, 15>(CurrOCR(), value);
        } else {
            TEMP = value;
        }
    }

    template <bool poke>
    FORCE_INLINE void WriteOCRL(uint8 value) {
        if constexpr (poke) {
            bit::deposit_into<0, 7>(CurrOCR(), value);
        } else {
            CurrOCR() = value | (TEMP << 8u);
        }
    }

    // 016  R/W  8        00        TCR       Timer control register
    //
    //   bits   r/w  code     description
    //      7   R/W  IEDGA    Input Edge Select (0=falling, 1=rising)
    //    6-2   R/W  -        Reserved - must be zero
    //    1-0   R/W  CKS1-0   Clock Select
    //                          00 (0) = Internal clock / 8
    //                          01 (1) = Internal clock / 32
    //                          10 (2) = Internal clock / 128
    //                          11 (3) = External clock (on rising edge)
    union RegTCR {
        uint8 u8;
        struct {
            uint8 CKSn : 2;
            uint8 : 5;
            uint8 IEDGA : 1;
        };
    } TCR;

    FORCE_INLINE uint8 ReadTCR() const {
        return TCR.u8;
    }

    FORCE_INLINE void WriteTCR(uint8 value) {
        TCR.u8 = value & 0x83;

        m_clockDividerShift = kDividerShifts[TCR.CKSn];
        m_cycleCountMask = (1ull << m_clockDividerShift) - 1;
    }

    // 017  R/W  8        E0        TOCR      Timer output compare control register
    //
    //   bits   r/w  code     description
    //    7-5   R/W  -        Reserved - must be one
    //      4   R/W  OCRS     Output Compare Register Select (0=OCRA, 1=OCRB)
    //    3-2   R/W  -        Reserved - must be zero
    //      1   R/W  OLVLA    Output Level A
    //      0   R/W  OLVLB    Output Level B
    union RegTOCR {
        uint8 u8;
        struct {
            uint8 OLVLA : 1;
            uint8 OLVLB : 1;
            uint8 : 2;
            uint8 OCRS : 1;
            uint8 : 3;
        };
    } TOCR;

    FORCE_INLINE uint8 ReadTOCR() const {
        return TOCR.u8;
    }

    FORCE_INLINE void WriteTOCR(uint8 value) {
        TOCR.u8 = value & 0x13;
    }

    // 018  R    8        00        ICR H     Input capture register H
    // 019  R    8        00        ICR L     Input capture register L
    uint16 ICR;

    template <bool peek>
    FORCE_INLINE uint8 ReadICRH() const {
        if constexpr (!peek) {
            TEMP = ICR;
        }
        return ICR >> 8u;
    }

    template <bool peek>
    FORCE_INLINE uint8 ReadICRL() const {
        if constexpr (peek) {
            return ICR;
        } else {
            return TEMP;
        }
    }

    template <bool poke>
    FORCE_INLINE void WriteICRH(uint8 value) {
        if constexpr (poke) {
            bit::deposit_into<8, 15>(ICR, value);
        }
    }

    template <bool poke>
    FORCE_INLINE void WriteICRL(uint8 value) {
        if constexpr (poke) {
            bit::deposit_into<0, 7>(ICR, value);
        }
    }

    mutable uint8 TEMP; // temporary storage to handle 16-bit transfers

    // -------------------------------------------------------------------------
    // State

private:
    uint64 m_cycleCount;
    uint64 m_clockDividerShift; // derived from TCR.CKS
    uint64 m_cycleCountMask;    // derived from TCR.CKS
};

} // namespace satemu::sh2
