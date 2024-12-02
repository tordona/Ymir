#pragma once

#include <satemu/core_types.hpp>

#include <satemu/hw/hw_defs.hpp>

#include <fmt/format.h>

#include <array>

// -----------------------------------------------------------------------------
// Forward declarations

namespace satemu::sys {

class SH2System;

} // namespace satemu::sys

// -----------------------------------------------------------------------------

namespace satemu::sh2 {

struct SH2State {
    friend class sys::SH2System;

    SH2State(bool master);

private:
    // Resets *most* of the state, except for PC and R15 which need to read from the bus.
    // SH2System::Reset(bool) invokes this and initializes PC and R15 properly.
    void Reset(bool hard);

public:
    void SetExternalInterrupt(uint8 level, uint8 vecNum);

    std::array<uint32, 16> R;

    uint32 PC;
    uint32 PR;

    union SR_t {
        uint32 u32;
        struct {
            uint32 T : 1;
            uint32 S : 1;
            uint32 : 2;
            uint32 I0 : 1;
            uint32 I1 : 1;
            uint32 I2 : 1;
            uint32 I3 : 1;
            uint32 Q : 1;
            uint32 M : 1;
        };
        struct {
            uint32 : 1;
            uint32 : 1;
            uint32 : 2;
            uint32 ILevel : 4;
        };
    } SR;
    uint32 GBR;
    uint32 VBR;

    union MAC_t {
        uint64 u64;
        struct {
            uint32 L;
            uint32 H;
        };
    } MAC;

    // -------------------------------------------------------------------------
    // On-chip peripherals

    union Reg16 {
        uint16 u16;
        uint8 u8[2];
    };

    // --- SCI module ---
    //
    // addr r/w  access   init      code    name
    // 000  R/W  8        00        SMR     Serial Mode Register
    //
    //   b  r/w  code  description
    //   7  R/W  C/nA  Communication Mode (0=async, 1=clocked sync)
    //   6  R/W  CHR   Character Length (0=8-bit, 1=7-bit)
    //   5  R/W  PE    Parity Enable (0=disable, 1=enable)
    //   4  R/W  O/nE  Parity Mode (0=even, 1=odd)
    //   3  R/W  STOP  Stop Bit Length (0=one, 1=two)
    //   2  R/W  MP    Multiprocessor Mode (0=disabled, 1=enabled)
    //   1  R/W  CKS1  Clock Select bit 1  (00=phi/4,  01=phi/16,
    //   0  R/W  CKS0  Clock Select bit 0   10=phi/64, 11=phi/256)
    //
    // 001  R/W  8        FF        BRR     Bit Rate Register
    // 002  R/W  8        00        SCR     Serial Control Register
    // 003  R/W  8        FF        TDR     Transmit Data Register
    // 004  R/W* 8        84        SSR     Serial Status Register
    //   * Can only write a 0 to clear the flags
    //
    // 005  R    8        00        RDR     Receive Data Register
    //
    // --- FRT module ---
    //
    // 010  ?    8        ??        TIER    ???
    // 011  ?    8        ??        FTCSR   ???
    // 012  ?    8        ??        FRC     ???
    // 013  ?    16?      ??        OCRA/B  ???
    // 015  ?    16?      ??        TCR     ???
    // 017  ?    8        ??        TOCR    ???
    // 018  ?    16?      ??        FICR    ???
    //
    // --- INTC module ---

    // 060  R/W  8,16     0000      IPRB    Interrupt priority setting register B
    //
    //   bits   r/w  code       description
    //   15-12  R/W  SCIIP3-0   Serial Communication Interface (SCI) Interrupt Priority Level
    //   11-8   R/W  FRTIP3-0   Free-Running Timer (FRT) Interrupt Priority Level
    //    7-0   R/W  Reserved   Must be zero
    //
    //   Interrupt priority levels range from 0 to 15.
    union IPRB_t {
        Reg16 val;
        struct {
            uint16 _rsvd0_7 : 8;
            uint16 FRTIPn : 4;
            uint16 SCIIPn : 4;
        };
    } IPRB;

    // 062  R/W  8,16     0000      VCRA    Vector number setting register A
    //
    //   bits   r/w  code     description
    //     15   R    -        Reserved - must be zero
    //   14-8   R/W  SERV6-0  Serial Communication Interface (SCI) Receive-Error Interrupt Vector Number
    //      7   R    -        Reserved - must be zero
    //    6-0   R/W  SRXV6-0  Serial Communication Interface (SCI) Receive-Data-Full Interrupt Vector Number
    union VCRA_t {
        Reg16 val;
        struct {
            uint16 SRXVn : 7;
            uint16 _rsvd7 : 1;
            uint16 SERVn : 7;
            uint16 _rsvd15 : 1;
        };
    } VCRA;

    // 064  R/W  8,16     0000      VCRB    Vector number setting register B
    //
    //   bits   r/w  code     description
    //     15   R    -        Reserved - must be zero
    //   14-8   R/W  STXV6-0  Serial Communication Interface (SCI) Transmit-Data-Empty Interrupt Vector Number
    //      7   R    -        Reserved - must be zero
    //    6-0   R/W  STEV6-0  Serial Communication Interface (SCI) Transmit-End Interrupt Vector Number
    union VCRB_t {
        Reg16 val;
        struct {
            uint16 STEVn : 7;
            uint16 _rsvd7 : 1;
            uint16 STXVn : 7;
            uint16 _rsvd15 : 1;
        };
    } VCRB;

    // 066  R/W  8,16     0000      VCRC    Vector number setting register C
    //
    //   bits   r/w  code     description
    //     15   R    -        Reserved - must be zero
    //   14-8   R/W  FICV6-0  Free-Running Timer (FRT) Input-Capture Interrupt Vector Number
    //      7   R    -        Reserved - must be zero
    //    6-0   R/W  FOCV6-0  Free-Running Timer (FRT) Output-Compare Interrupt Vector Number
    union VCRC_t {
        Reg16 val;
        struct {
            uint16 FOCVn : 7;
            uint16 _rsvd7 : 1;
            uint16 FICVn : 7;
            uint16 _rsvd15 : 1;
        };
    } VCRC;

    // 068  R/W  8,16     0000      VCRD    Vector number setting register D
    //
    //   bits   r/w  code     description
    //     15   R    -        Reserved - must be zero
    //   14-8   R/W  FOVV6-0  Free-Running Timer (FRT) Overflow Interrupt Vector Number
    //    7-0   R    -        Reserved - must be zero
    union VCRD_t {
        Reg16 val;
        struct {
            uint16 _rsvd0_7 : 8;
            uint16 FOVVn : 7;
            uint16 _rsvd15 : 1;
        };
    } VCRD;

    // 0E0  R/W  8,16     0000      ICR     Interrupt control register
    //
    //   bits   r/w  code   description
    //     15   R    NMIL   NMI Input Level
    //   14-9   R    -      Reserved - must be zero
    //      8   R/W  NMIE   NMI Edge Select (0=falling, 1=rising)
    //    7-1   R    -      Reserved - must be zero
    //      0   R/W  VECMD  IRL Interrupt Vector Mode Select (0=auto, 1=external)
    //                      Auto-vector mode assigns 71 to IRL15 and IRL14, and 64 to IRL1.
    //                      External vector mode reads from external vector number input pins D7-D0.
    //
    //    The default value may be either 8000 or 0000 because NMIL is an external signal.
    union ICR_t {
        Reg16 val;
        struct {
            uint16 VECMD : 1;
            uint16 _rsvd1_7 : 7;
            uint16 NMIE : 1;
            uint16 _rsvd9_14 : 6;
            uint16 NMIL : 1;
        };
    } ICR;

    // 0E2  R/W  8,16     0000      IPRA    Interrupt priority setting register A
    //
    //   bits   r/w  code       description
    //   15-12  R/W  DIVUIP3-0  Division Unit (DIVU) Interrupt Priority Level
    //   11-8   R/W  DMACIP3-0  DMA Controller (DMAC) Interrupt Priority Level
    //    7-4   R/W  WDTIP3-0   Watchdog Timer (WDT) Interrupt Priority Level
    //    3-0   R    -          Reserved - must be zero
    //
    //   Interrupt priority levels range from 0 to 15.
    //
    //   The DMAC priority level is assigned to both channels.
    //   If both channels raise an interrupt, channel 0 is prioritized.
    //
    //   WDTIP3-0 includes both the watchdog timer and bus state controller (BSC).
    //   WDT interrupt has priority over BSC.
    union IPRA_t {
        Reg16 val;
        struct {
            uint16 _rsvd0_3 : 4;
            uint16 WDTIPn : 4;
            uint16 DMACIPn : 4;
            uint16 DIVUIPn : 4;
        };
    } IPRA;

    // 0E4  R/W  8,16     0000      VCRWDT  Vector number setting register WDT
    //
    //   bits   r/w  code     description
    //     15   R    -        Reserved - must be zero
    //   14-8   R/W  WITV6-0  Watchdog Timer (WDT) Interval Interrupt Vector Number
    //      7   R    -        Reserved - must be zero
    //    6-0   R/W  BCMV6-0  Bus State Controller (BSC) Compare Match Interrupt Vector Number
    union VCRWDT_t {
        Reg16 val;
        struct {
            uint16 BCMVn : 7;
            uint16 _rsvd7 : 1;
            uint16 WITVn : 7;
            uint16 _rsvd15 : 1;
        };
    } VCRWDT;

    // --- DMAC module ---
    //
    // 071  ?    8        ??        DRCR0   ???
    // 072  ?    8        ??        DRCR1   ???
    //
    // --- WDT module ---
    //
    // 080  R    8        ??        WTCSR   ???
    // 081  R    8        ??        WTCNT   ???
    // 083  R    8        ??        RSTCSR  ???
    //
    // 080  W    8        ??        WTCSR   ???
    // 080  W    8        ??        WTCNT   ???
    // 082  W    8        ??        RSTCSR  ???
    //
    // --- Power-down module ---
    //
    // 091  ?    8        ??        SBYCR   ???
    //
    // --- Cache module ---

    static constexpr size_t kCacheWays = 4;
    static constexpr size_t kCacheEntries = 64;
    static constexpr size_t kCacheLineSize = 16;

    struct CacheEntry {
        // Tag layout:
        //   28..10: tag
        //        2: valid bit
        // All other bits must be zero
        // This matches the address array structure
        std::array<uint32, kCacheWays> tag;
        std::array<std::array<uint8, kCacheLineSize>, kCacheWays> line;
    };
    alignas(16) std::array<CacheEntry, kCacheEntries> cacheEntries;

    // 092  R/W  8        00        CCR     Cache Control Register
    //
    //   bits   r/w  code   description
    //      7   R/W  W1     Way Specification (MSB)
    //      6   R/W  W0     Way Specification (LSB)
    //      5   R    -      Reserved - must be zero
    //      4   R/W  CP     Cache Purge (0=normal, 1=purge)
    //      3   R/W  TW     Two-Way Mode (0=four-way, 1=two-way)
    //      2   R/W  OD     Data Replacement Disable (0=disabled, 1=data cache not updated on miss)
    //      1   R/W  ID     Instruction Replacement Disabled (same as above, but for code cache)
    //      0   R/W  CE     Cache Enable (0=disable, 1=enable)
    union CCR_t {
        uint8 u8;
        struct {
            uint8 CE : 1;
            uint8 ID : 1;
            uint8 OD : 1;
            uint8 TW : 1;
            uint8 CP : 1;
            uint8 _rsvd5 : 1;
            uint8 Wn : 2;
        };
    } CCR;

    void WriteCCR(uint8 value);

    // 0E0, 0E2, 0E4 are in INTC module above

    // --- DIVU module ---
    //
    // 100  R/W  32       ud        DVSR    Divisor register
    //
    //   bits   r/w  code   description
    //   31-0   R/W  -      Divisor number
    uint32 DVSR;

    // 104  R/W  32       ud        DVDNT   Dividend register L for 32-bit division
    //
    //   bits   r/w  code   description
    //   31-0   R/W  -      32-bit dividend number
    uint32 DVDNT;

    // 108  R/W  16,32    00000000  DVCR    Division control register
    //
    //   bits   r/w  code   description
    //   31-2   R    -      Reserved - must be zero
    //      1   R/W  OVFIE  OVF interrupt enable (0=disabled, 1=enabled)
    //      0   R/W  OVF    Overflow Flag (0=no overflow, 1=overflow)
    union DVCR_t {
        uint32 u32;
        struct {
            uint32 OVF : 1;
            uint32 OVFIE : 1;
            uint32 _rsvd2_31 : 30;
        };
    } DVCR;

    // 10C  R/W  16,32    ud        VCRDIV  Vector number register setting DIV
    //
    //   bits   r/w  code   description
    //  31-16   R    -      Reserved - must be zero
    //   15-0   R/W  -      Interrupt Vector Number
    uint16 VCRDIV;

    // 110  R/W  32       ud        DVDNTH  Dividend register H
    //
    //   bits   r/w  code   description
    //   31-0   R/W  -      64-bit dividend number (upper half)
    uint32 DVDNTH;

    // 114  R/W  32       ud        DVDNTL  Dividend register L
    //
    //   bits   r/w  code   description
    //   31-0   R/W  -      64-bit dividend number (lower half)
    uint32 DVDNTL;

    // 120..13F are mirrors of 100..11F

    // Both division calculations take 39 cycles to complete, or 6 if it results in overflow.
    // On overflow, the OVF bit is set and an overflow interrupt is generated if DVCR.OVFIE=1.
    // DVDNTH and DVDNTL will contain the partial results of the operation after 6 cycles.
    // If DVCR.OFVIE=0, DVDNTL will be saturated to 0x7FFFFFFF or 0x80000000 depending on the sign.
    // For 32-bit by 32-bit divisions, DVDNT receives a copy of DVDNTL.

    // Begins a 32-bit by 32-bit signed division calculation, storing the 32-bit quotient in DVDNT
    // and the 32-bit remainder in DVDNTH.
    void DIVUBegin32();

    // Begins a 64-bit by 32-bit signed division calculation, storing the 32-bit quotient in DVDNTL
    // and the 32-bit remainder in DVDNTH.
    void DIVUBegin64();

    // --- UBC module (channel A) ---
    //
    // 140  ?    16?      ??        BARAH   ???
    // 142  ?    16?      ??        BARAL   ???
    // 144  ?    16?      ??        BAMRAH  ???
    // 146  ?    16?      ??        BAMRAL  ???
    // 148  ?    16?      ??        BBRA    ???
    //
    // --- UBC module (channel B) ---
    //
    // 160  ?    16?      ??        BARBH   ???
    // 162  ?    16?      ??        BARBL   ???
    // 164  ?    16?      ??        BAMRBH  ???
    // 166  ?    16?      ??        BAMRBL  ???
    // 168  ?    16?      ??        BBRB    ???
    // 170  ?    16?      ??        BDRBH   ???
    // 172  ?    16?      ??        BDRBL   ???
    // 174  ?    16?      ??        BDMRBH  ???
    // 176  ?    16?      ??        BDMRBL  ???
    // 178  ?    16?      ??        BRCR    ???
    //
    // --- DMAC module ---

    // 180  ?    32?      ??        SAR0    ???
    // 184  ?    32?      ??        DAR0    ???
    // 188  ?    32?      ??        TCR0    ???
    // 18C  ?    32?      ??        CHCR0   ???

    // 1A0  R/W  32       ud        VCRDMA0 DMA vector number register 0
    //
    //   bits   r/w  code   description
    //   31-8   R    -      Reserved - must be zero
    //    7-0   R/W  VC7-0  Vector Number
    uint8 VCRDMA0;

    //
    // 190  ?    32?      ??        SAR1    ???
    // 194  ?    32?      ??        DAR1    ???
    // 198  ?    32?      ??        TCR1    ???
    // 19C  ?    32?      ??        CHCR1   ???

    // 1A8  R/W  32       ud        VCRDMA1 DMA vector number register 1
    //
    //   bits   r/w  code   description
    //   31-8   R    -      Reserved - must be zero
    //    7-0   R/W  VC7-0  Vector Number
    uint8 VCRDMA1;

    // 1B0  ?    32?      ??        DMAOR   ???
    //
    // --- BSC module ---
    //
    // 1E0  R/W  16,32    03F0      BCR1    Bus Control Register 1
    union BCR1_t {
        uint16 u16;
        struct {
            uint16 DRAMn : 3;
            uint16 _rsvd3 : 1;
            uint16 A0LWn : 2;
            uint16 A1LWn : 2;
            uint16 AHLWn : 2;
            uint16 PSHR : 1;
            uint16 BSTROM : 1;
            uint16 ENDIAN : 1;
            uint16 _rsvd13 : 1;
            uint16 _rsvd14 : 1;
            uint16 MASTER : 1;
        };
        uint16 u15 : 15;
    } BCR1;

    // 1E4  R/W  16,32    00FC      BCR2    Bus Control Register 2
    union BCR2_t {
        uint16 u16;
        struct {
            uint16 _rsvd0 : 1;
            uint16 _rsvd1 : 1;
            uint16 A1SZn : 2;
            uint16 A2SZn : 2;
            uint16 A3SZn : 2;
        };
    } BCR2;

    // 1E8  R/W  16,32    AAFF      WCR     Wait Control Register
    union WCR_t {
        uint16 u16;
        struct {
            uint16 W0n : 2;
            uint16 W1n : 2;
            uint16 W2n : 2;
            uint16 W3n : 2;
            uint16 IW0n : 2;
            uint16 IW1n : 2;
            uint16 IW2n : 2;
            uint16 IW3n : 2;
        };
    } WCR;

    // 1EC  R/W  16,32    0000      MCR     Individual Memory Control Register
    union MCR_t {
        uint16 u16;
        struct {
            uint16 _rsvd0 : 1;
            uint16 _rsvd1 : 1;
            uint16 RMD : 1;
            uint16 RFSH : 1;
            uint16 AMX0 : 1;
            uint16 AMX1 : 1;
            uint16 SZ : 1;
            uint16 AMX2 : 1;
            uint16 _rsvd8 : 1;
            uint16 RASD : 1;
            uint16 BE : 1;
            uint16 TRASn : 2;
            uint16 TRWL : 1;
            uint16 RCD : 1;
            uint16 TRP : 1;
        };
    } MCR;

    // 1F0  R/W  16,32    0000      RTCSR   Refresh Timer Control/Status Register
    union RTCSR_t {
        uint16 u16;
        struct {
            uint16 _rsvd0 : 1;
            uint16 _rsvd1 : 1;
            uint16 _rsvd2 : 1;
            uint16 CKSn : 3;
            uint16 CMIE : 1;
            uint16 CMF : 1;
        };
    } RTCSR;

    // 1F4  R/W  16,32    0000      RTCNT   Refresh Timer Counter
    uint8 RTCNT;

    // 1F8  R/W  16,32    0000      RTCOR   Refresh Timer Constant Register
    uint8 RTCOR;

    template <mem_access_type T>
    T OnChipRegRead(uint32 address) {
        // Misaligned memory accesses raise an address error, meaning all accesses here are aligned.
        // Therefore:
        //   (address & 3) == 2 is only valid for 16-bit accesses
        //   (address & 1) == 1 is only valid for 8-bit accesses
        // Additionally:
        //   (address & 1) == 0 has special cases for registers 0-255:
        //     8-bit read from a 16-bit register:  r >> 8u
        //     16-bit read from a 8-bit register: (r << 8u) | r
        //     Every other access returns just r

        // Registers 0-255 do not accept 32-bit accesses
        if constexpr (std::is_same_v<T, uint32>) {
            if (address < 0x100) {
                // TODO: raise an address error
            }
        }

        // Registers 256-511 do not accept 8-bit accesses
        if constexpr (std::is_same_v<T, uint8>) {
            if (address >= 0x100) {
                // TODO: raise an address error
            }
        }

        auto readWordLower = [&](Reg16 value) -> T {
            if constexpr (std::is_same_v<T, uint8>) {
                return value.u8[(address & 1) ^ 1];
            } else {
                return value.u16;
            }
        };
        auto readByteLower = [&](uint8 value) -> T {
            if constexpr (std::is_same_v<T, uint16>) {
                return (address & 1) ? value : ((value << 8u) | value);
            } else {
                return value;
            }
        };

        // Note: Clang generates marginally faster code with case ranges here.
        // See:
        // https://quick-bench.com/q/vB2HZ3bzAIlqIazYoVxy7A0ooKg

        switch (address) {
        case 0x60 ... 0x61: return readWordLower(IPRB.val);
        case 0x62 ... 0x63: return readWordLower(VCRA.val);
        case 0x64 ... 0x65: return readWordLower(VCRB.val);
        case 0x66 ... 0x67: return readWordLower(VCRC.val);
        case 0x68 ... 0x69: return readWordLower(VCRD.val);
        case 0x92 ... 0x9F: return readByteLower(CCR.u8);
        case 0xE0 ... 0xE1: return readWordLower(ICR.val);
        case 0xE2 ... 0xE3: return readWordLower(IPRA.val);
        case 0xE4 ... 0xE5: return readWordLower(VCRWDT.val);

        case 0x100:
        case 0x120: return DVSR;

        case 0x104:
        case 0x124: return DVDNT;

        case 0x108:
        case 0x128: return DVCR.u32;

        case 0x10C:
        case 0x12C: return VCRDIV;

        case 0x110:
        case 0x130: return DVDNTH;

        case 0x114:
        case 0x134: return DVDNTL;

        case 0x1A0: return VCRDMA0;
        case 0x1A8: return VCRDMA1;

        case 0x1E0 ... 0x1E2: return BCR1.u16;
        case 0x1E4 ... 0x1E6: return BCR2.u16;
        case 0x1E8 ... 0x1EA: return WCR.u16;
        case 0x1EC ... 0x1EE: return MCR.u16;
        case 0x1F0 ... 0x1F2: return RTCSR.u16;
        case 0x1F4 ... 0x1F6: return RTCNT;
        case 0x1F8 ... 0x1FA: return RTCOR;

        default: //
            fmt::println("unhandled {}-bit on-chip register read from {:02X}", sizeof(T) * 8, address);
            return 0;
        }
    }

    template <mem_access_type T>
    void OnChipRegWrite(uint32 address, T baseValue) {
        // Misaligned memory accesses raise an address error, meaning all accesses here are aligned.
        // Therefore:
        //   (address & 3) == 2 is only valid for 16-bit accesses
        //   (address & 1) == 1 is only valid for 8-bit accesses

        // Registers 0-255 do not accept 32-bit accesses
        if constexpr (std::is_same_v<T, uint32>) {
            if (address < 0x100) {
                // TODO: raise an address error
            }
        }

        // Registers 256-511 do not accept 8-bit accesses
        uint32 value = baseValue;
        if constexpr (std::is_same_v<T, uint8>) {
            if (address >= 0x100) {
                // TODO: raise an address error
                value |= value << 8u;
            }
        }

        // For registers 0-255, 8-bit writes to 16-bit registers change the corresponding byte
        auto writeWordLower = [&](Reg16 &reg, T value, uint16 mask) {
            if constexpr (std::is_same_v<T, uint8>) {
                uint32 index = ((address & 1) ^ 1);
                mask >>= index * 8;
                if ((mask & 0xFF) != 0) {
                    reg.u8[index] = value & mask;
                }
            } else {
                reg.u16 = value & mask;
            }
        };

        // Note: the repeated cases below might seem redundant, but it actually causes Clang to generate better code.
        // Case ranges (case 0x61 ... 0x62) or fallthrough (case 0x61: case 0x62) generate suboptimal code.
        // See:
        // https://quick-bench.com/q/j3XUHw-u-Y75chqoIPnxwyF3JXw
        // https://godbolt.org/z/5nbPxqd5d

        switch (address) {
        case 0x60: writeWordLower(IPRB.val, value, 0xFF00); break;
        case 0x61: writeWordLower(IPRB.val, value, 0xFF00); break;
        case 0x62: writeWordLower(VCRA.val, value, 0x7F7F); break;
        case 0x63: writeWordLower(VCRA.val, value, 0x7F7F); break;
        case 0x64: writeWordLower(VCRB.val, value, 0x7F7F); break;
        case 0x65: writeWordLower(VCRB.val, value, 0x7F7F); break;
        case 0x66: writeWordLower(VCRC.val, value, 0x7F7F); break;
        case 0x67: writeWordLower(VCRC.val, value, 0x7F7F); break;
        case 0x68: writeWordLower(VCRD.val, value, 0x7F00); break;
        case 0x69: writeWordLower(VCRD.val, value, 0x7F00); break;

        case 0x92: WriteCCR(value); break;

        case 0xE0: writeWordLower(ICR.val, value, 0x0101); break;
        case 0xE1: writeWordLower(ICR.val, value, 0x0101); break;
        case 0xE2: writeWordLower(IPRA.val, value, 0xFFF0); break;
        case 0xE3: writeWordLower(IPRA.val, value, 0xFFF0); break;
        case 0xE4: writeWordLower(VCRWDT.val, value, 0x7F7F); break;
        case 0xE5: writeWordLower(VCRWDT.val, value, 0x7F7F); break;

        case 0x100:
        case 0x120: DVSR = value; break;

        case 0x104:
        case 0x124:
            DVDNT = value;
            DVDNTL = value;
            DVDNTH = static_cast<sint32>(value) >> 31;
            DIVUBegin32();
            break;

        case 0x108:
        case 0x128: DVCR.u32 = value & 0x00000003; break;

        case 0x10C:
        case 0x12C: VCRDIV = value; break;

        case 0x110:
        case 0x130: DVDNTH = value; break;

        case 0x114:
        case 0x134:
            DVDNTL = value;
            DIVUBegin64();
            break;

        case 0x1A0: VCRDMA0 = value; break;
        case 0x1A8: VCRDMA1 = value; break;

        case 0x1E0: // BCR1
            // Only accepts 32-bit writes and the top 16 bits must be 0xA55A
            if constexpr (std::is_same_v<T, uint32>) {
                if ((value >> 16u) == 0xA55A) {
                    BCR1.u15 = value & 0x1FF7;
                }
            }
            break;
        case 0x1E4: // BCR2
            // Only accepts 32-bit writes and the top 16 bits must be 0xA55A
            if constexpr (std::is_same_v<T, uint32>) {
                if ((value >> 16u) == 0xA55A) {
                    BCR2.u16 = value & 0xFC;
                }
            }
            break;
        case 0x1E8: // WCR
            // Only accepts 32-bit writes and the top 16 bits must be 0xA55A
            if constexpr (std::is_same_v<T, uint32>) {
                if ((value >> 16u) == 0xA55A) {
                    WCR.u16 = value;
                }
            }
            break;
        case 0x1EC: // MCR
            // Only accepts 32-bit writes and the top 16 bits must be 0xA55A
            if constexpr (std::is_same_v<T, uint32>) {
                if ((value >> 16u) == 0xA55A) {
                    MCR.u16 = value & 0xFEFC;
                }
            }
            break;
        case 0x1F0: // RTCSR
            // Only accepts 32-bit writes and the top 16 bits must be 0xA55A
            if constexpr (std::is_same_v<T, uint32>) {
                if ((value >> 16u) == 0xA55A) {
                    // TODO: implement the set/clear rules for RTCSR.CMF
                    RTCSR.u16 = (value & 0x78) | (RTCSR.u16 & 0x80);
                }
            }
            break;
        case 0x1F4: // RTCNT
            // Only accepts 32-bit writes and the top 16 bits must be 0xA55A
            if constexpr (std::is_same_v<T, uint32>) {
                if ((value >> 16u) == 0xA55A) {
                    RTCNT = value;
                }
            }
            break;
        case 0x1F8: // RTCOR
            // Only accepts 32-bit writes and the top 16 bits must be 0xA55A
            if constexpr (std::is_same_v<T, uint32>) {
                if ((value >> 16u) == 0xA55A) {
                    RTCOR = value;
                }
            }
            break;
        default: //
            fmt::println("unhandled {}-bit on-chip register write to {:02X} = {:X}", sizeof(T) * 8, address, value);
            break;
        }
    }

    // -------------------------------------------------------------------------
    // Interrupts

    uint8 pendingExternalIntrLevel;
    uint8 pendingExternalIntrVecNum;

    struct PendingInterruptInfo {
        uint8 priority;
        uint8 vecNum;
    } pendingInterrupt;

    void CheckInterrupts();
};

} // namespace satemu::sh2
