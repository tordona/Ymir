#pragma once

#include <satemu/core/types.hpp>

#include <satemu/util/bit_ops.hpp>
#include <satemu/util/inline.hpp>

namespace satemu::sh2 {

struct WatchdogTimer {
    static constexpr uint64 kDividerShifts[] = {1, 6, 7, 8, 9, 10, 12, 13};

    WatchdogTimer() {
        Reset(false);
    }

    void Reset(bool watchdogInitiated) {
        WTCSR.u8 = 0x18;
        WTCNT = 0x00;
        if (!watchdogInitiated) {
            RSTCSR.u8 = 0x1F;
        }

        cycleCount = 0;
        clockDividerShift = kDividerShifts[WTCSR.CKSn];
        cycleCountMask = (1ull << clockDividerShift) - 1;
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

    union RegWTCSR {
        uint8 u8;
        struct {
            uint8 CKSn : 3;
            uint8 : 2;
            uint8 TME : 1;
            uint8 WT_nIT : 1;
            uint8 OVF : 1;
        };
    } WTCSR;

    FORCE_INLINE uint8 ReadWTCSR() const {
        return WTCSR.u8;
    }

    template <bool poke>
    FORCE_INLINE void WriteWTCSR(uint8 value) {
        if constexpr (poke) {
            WTCSR.OVF = bit::extract<7>(value);
        } else {
            WTCSR.OVF &= bit::extract<7>(value);
        }
        WTCSR.WT_nIT = bit::extract<6>(value);
        WTCSR.TME = bit::extract<5>(value);
        WTCSR.CKSn = bit::extract<0, 2>(value);

        clockDividerShift = kDividerShifts[WTCSR.CKSn];
        cycleCountMask = (1ull << clockDividerShift) - 1;
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

    union RegRSTCSR {
        uint8 u8;
        struct {
            uint8 : 5;
            uint8 RSTS : 1;
            uint8 RSTE : 1;
            uint8 WOVF : 1;
        };
    } RSTCSR;

    FORCE_INLINE uint8 ReadRSTCSR() const {
        return RSTCSR.u8;
    }

    template <bool poke>
    FORCE_INLINE void WriteRSTCSR(uint8 value) {
        if constexpr (poke) {
            RSTCSR.WOVF = bit::extract<7>(value);
        } else {
            RSTCSR.WOVF &= bit::extract<7>(value);
        }
        RSTCSR.RSTE = bit::extract<6>(value);
        RSTCSR.RSTS = bit::extract<5>(value);
    }

    FORCE_INLINE void WriteRSTE_RSTS(uint8 value) {
        RSTCSR.RSTE = bit::extract<6>(value);
        RSTCSR.RSTS = bit::extract<5>(value);
    }

    template <bool poke>
    FORCE_INLINE void WriteWOVF(uint8 value) {
        if constexpr (poke) {
            RSTCSR.WOVF = bit::extract<7>(value);
        } else {
            RSTCSR.WOVF &= bit::extract<7>(value);
        }
    }

    // -------------------------------------------------------------------------
    // State

    uint64 cycleCount;
    uint64 clockDividerShift; // derived from WTCSR.CKS
    uint64 cycleCountMask;    // derived from WTCSR.CKS

    uint64 CyclesUntilNextTick() const {
        return (1ull << clockDividerShift) - (cycleCount & cycleCountMask);
    }
};

} // namespace satemu::sh2
