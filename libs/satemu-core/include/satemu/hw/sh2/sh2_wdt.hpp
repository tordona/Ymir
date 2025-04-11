#pragma once

#include <satemu/core/types.hpp>

#include <satemu/util/bit_ops.hpp>
#include <satemu/util/inline.hpp>

namespace satemu::sh2 {

struct WatchdogTimer {
    static constexpr uint64 kDividerShifts[] = {1, 6, 7, 8, 9, 10, 12, 13};

    enum class Event { None, Reset, RaiseInterrupt };

    WatchdogTimer() {
        Reset(false);
    }

    void Reset(bool watchdogInitiated) {
        WTCSR.Reset();
        WTCNT = 0x00;
        if (!watchdogInitiated) {
            RSTCSR.Reset();
        }

        m_cycleCount = 0;
        m_clockDividerShift = kDividerShifts[WTCSR.CKSn];
        m_cycleCountMask = (1ull << m_clockDividerShift) - 1;
    }

    FORCE_INLINE Event Advance(uint64 cycles) {
        if (!WTCSR.TME) {
            return Event::None;
        }

        m_cycleCount += cycles;
        const uint64 steps = m_cycleCount >> m_clockDividerShift;
        m_cycleCount -= steps << m_clockDividerShift;

        Event event = Event::None;

        uint64 nextCount = WTCNT + steps;
        if (nextCount >= 0x10000) {
            if (WTCSR.WT_nIT) {
                // Watchdog timer mode
                RSTCSR.WOVF = 1;
                if (RSTCSR.RSTE) {
                    event = Event::Reset;
                }
            } else {
                // Interval timer mode
                WTCSR.OVF = 1;
                event = Event::RaiseInterrupt;
            }
        }
        WTCNT = nextCount;

        return event;
    }

    FORCE_INLINE uint64 CyclesUntilNextTick() const {
        return (1ull << m_clockDividerShift) - (m_cycleCount & m_cycleCountMask);
    }

    // -------------------------------------------------------------------------
    // Registers

    // addr r/w  access   init      code    name
    // 080  R    8        18        WTCSR   Watchdog Timer Control/Status Register
    // 080  W    8        18        WTCSR   Watchdog Timer Control/Status Register
    //
    //   bits   r/w  code     description
    //      7   R/W  OVF      Overflow Flag
    //      6   R/W  WT/!IT   Timer Mode Select (0=interval timer (ITI), 1=watchdog timer)
    //      5   R/W  TME      Timer Enable
    //    4-3   R    -        Reserved - must be one
    //    2-0   R/W  CKS2-0   Clock Select
    //                          000 (0) = phi/2
    //                          001 (1) = phi/64
    //                          010 (2) = phi/128
    //                          011 (3) = phi/256
    //                          100 (4) = phi/512
    //                          101 (5) = phi/1024
    //                          110 (6) = phi/4096
    //                          111 (7) = phi/8192
    struct RegWTCSR {
        RegWTCSR() {
            Reset();
        }

        void Reset() {
            OVF = false;
            WT_nIT = false;
            TME = false;
            CKSn = 0;
        }

        bool OVF;    //   7   R/W  OVF      Overflow Flag
        bool WT_nIT; //   6   R/W  WT/!IT   Timer Mode Select (0=interval timer (ITI), 1=watchdog timer)
        bool TME;    //   5   R/W  TME      Timer Enable
        uint8 CKSn;  // 2-0   R/W  CKS2-0   Clock Select
    } WTCSR;

    FORCE_INLINE uint8 ReadWTCSR() const {
        uint8 value = 0;
        bit::deposit_into<7>(value, WTCSR.OVF);
        bit::deposit_into<6>(value, WTCSR.WT_nIT);
        bit::deposit_into<5>(value, WTCSR.TME);
        bit::deposit_into<3, 4>(value, 0b11);
        bit::deposit_into<0, 2>(value, WTCSR.CKSn);
        return value;
    }

    template <bool poke>
    FORCE_INLINE void WriteWTCSR(uint8 value) {
        if constexpr (poke) {
            WTCSR.OVF = bit::test<7>(value);
        } else {
            WTCSR.OVF &= bit::test<7>(value);
        }
        WTCSR.WT_nIT = bit::test<6>(value);
        WTCSR.TME = bit::test<5>(value);
        WTCSR.CKSn = bit::extract<0, 2>(value);

        m_clockDividerShift = kDividerShifts[WTCSR.CKSn];
        m_cycleCountMask = (1ull << m_clockDividerShift) - 1;
    }

    // 081  R    8        00        WTCNT   Watchdog Timer Counter
    // 080  W    8        00        WTCNT   Watchdog Timer Counter
    //
    //   bits   r/w  code     description
    //    7-0   R/W  WTCNT    Watchdog timer counter
    uint8 WTCNT;

    FORCE_INLINE uint8 ReadWTCNT() const {
        return WTCNT;
    }

    FORCE_INLINE void WriteWTCNT(uint8 value) {
        WTCNT = value;
    }

    // 083  R    8        1F        RSTCSR  Reset Control/Status Register
    // 082  W    8        1F        RSTCSR  Reset Control/Status Register
    //
    //   bits   r/w  code     description
    //      7   R/W  WOVF     Watchdog Timer Overflow Flag
    //      6   R/W  RSTE     Reset Enable
    //      5   R/W  RSTS     Reset Select (0=power-on reset, 1=manual reset)
    //    4-0   R    -        Reserved - must be one
    struct RegRSTCSR {
        RegRSTCSR() {
            Reset();
        }

        void Reset() {
            WOVF = false;
            RSTE = false;
            RSTS = false;
        }

        bool WOVF; // 7   R/W  WOVF     Watchdog Timer Overflow Flag
        bool RSTE; // 6   R/W  RSTE     Reset Enable
        bool RSTS; // 5   R/W  RSTS     Reset Select (0=power-on reset, 1=manual reset)
    } RSTCSR;

    FORCE_INLINE uint8 ReadRSTCSR() const {
        uint8 value = 0;
        bit::deposit_into<7>(value, RSTCSR.WOVF);
        bit::deposit_into<6>(value, RSTCSR.RSTE);
        bit::deposit_into<5>(value, RSTCSR.RSTS);
        bit::deposit_into<0, 4>(value, 0b11111);
        return value;
    }

    template <bool poke>
    FORCE_INLINE void WriteRSTCSR(uint8 value) {
        if constexpr (poke) {
            RSTCSR.WOVF = bit::test<7>(value);
        } else {
            RSTCSR.WOVF &= bit::test<7>(value);
        }
        RSTCSR.RSTE = bit::test<6>(value);
        RSTCSR.RSTS = bit::test<5>(value);
    }

    FORCE_INLINE void WriteRSTE_RSTS(uint8 value) {
        RSTCSR.RSTE = bit::test<6>(value);
        RSTCSR.RSTS = bit::test<5>(value);
    }

    template <bool poke>
    FORCE_INLINE void WriteWOVF(uint8 value) {
        if constexpr (poke) {
            RSTCSR.WOVF = bit::test<7>(value);
        } else {
            RSTCSR.WOVF &= bit::test<7>(value);
        }
    }

    // -------------------------------------------------------------------------
    // State

private:
    uint64 m_cycleCount;
    uint64 m_clockDividerShift; // derived from WTCSR.CKS
    uint64 m_cycleCountMask;    // derived from WTCSR.CKS
};

} // namespace satemu::sh2
