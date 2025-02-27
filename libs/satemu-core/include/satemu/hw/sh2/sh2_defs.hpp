#pragma once

#include <satemu/core/types.hpp>

#include <satemu/util/bit_ops.hpp>
#include <satemu/util/inline.hpp>

#include <array>

namespace satemu::sh2 {

// Standard SH-2 exception vectors
inline constexpr uint8 xvHardResetPC = 0x00;      // 00  00000000  Power-on reset PC value
inline constexpr uint8 xvHardResetSP = 0x01;      // 01  00000004  Power-on reset SP value
inline constexpr uint8 xvSoftResetPC = 0x02;      // 02  00000008  Manual reset PC value
inline constexpr uint8 xvSoftResetSP = 0x03;      // 03  0000000C  Manual reset SP value
inline constexpr uint8 xvGenIllegalInstr = 0x04;  // 04  00000010  General illegal instruction
inline constexpr uint8 xvSlotIllegalInstr = 0x06; // 06  00000018  Slot illegal instruction
inline constexpr uint8 xvCPUAddressError = 0x09;  // 09  00000024  CPU address error
inline constexpr uint8 xvDMAAddressError = 0x0A;  // 0A  00000028  DMA address error
inline constexpr uint8 xvIntrNMI = 0x0B;          // 0B  0000002C  NMI interrupt
inline constexpr uint8 xvIntrUserBreak = 0x0C;    // 0C  00000030  User break interrupt
inline constexpr uint8 xvIntrLevel1 = 0x40;       // 40* 00000100  IRL1
inline constexpr uint8 xvIntrLevels23 = 0x41;     // 41* 00000104  IRL2 and IRL3
inline constexpr uint8 xvIntrLevels45 = 0x42;     // 42* 00000108  IRL4 and IRL5
inline constexpr uint8 xvIntrLevels67 = 0x43;     // 43* 0000010C  IRL6 and IRL7
inline constexpr uint8 xvIntrLevels89 = 0x44;     // 44* 00000110  IRL8 and IRL9
inline constexpr uint8 xvIntrLevelsAB = 0x45;     // 45* 00000114  IRL10 and IRL11
inline constexpr uint8 xvIntrLevelsCD = 0x46;     // 46* 00000118  IRL12 and IRL13
inline constexpr uint8 xvIntrLevelsEF = 0x47;     // 47* 0000011C  IRL14 and IRL15
                                                  //
                                                  // * denote auto-vector numbers;
                                                  //   values vary when using external vector fetch
                                                  //
                                                  // vectors 05, 07, 08, 0D through 1F are reserved
                                                  // vectors 20 through 3F are reserved for TRAPA

// MACH and MACL
union RegMAC {
    uint64 u64;
    struct {
        uint32 L;
        uint32 H;
    };
};

union RegSR {
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
};

// Represents a 16-bit register with each byte individually accessible
union Reg16 {
    uint16 u16;
    uint8 u8[2];
};

// --- SCI module ---

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

// --- FRT module ---

struct FreeRunningTimer {
    static constexpr uint64 kDividerShifts[] = {3, 5, 7, 0};

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

        cycleCount = 0;
        clockDividerShift = kDividerShifts[TCR.CKSn];
        cycleCountMask = (1ull << clockDividerShift) - 1;
    }

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

    FORCE_INLINE void WriteFTCSR(uint8 value) {
        FTCSR.ICF &= bit::extract<7>(value);
        FTCSR.OCFA &= bit::extract<3>(value);
        FTCSR.OCFB &= bit::extract<2>(value);
        FTCSR.OVF &= bit::extract<1>(value);
        FTCSR.CCLRA = bit::extract<0>(value);
    }

    // 012  R/W  8        00        FRC H     Free-running counter H
    // 013  R/W  8        00        FRC L     Free-running counter L
    uint16 FRC;

    FORCE_INLINE uint8 ReadFRCH() const {
        TEMP = FRC;
        return FRC >> 8u;
    }

    FORCE_INLINE uint8 ReadFRCL() const {
        return TEMP;
    }

    FORCE_INLINE void WriteFRCH(uint8 value) {
        TEMP = value;
    }

    FORCE_INLINE void WriteFRCL(uint8 value) {
        FRC = value | (TEMP << 8u);
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

    FORCE_INLINE void WriteOCRH(uint8 value) {
        TEMP = value;
    }

    FORCE_INLINE void WriteOCRL(uint8 value) {
        CurrOCR() = value | (TEMP << 8u);
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

        clockDividerShift = kDividerShifts[TCR.CKSn];
        cycleCountMask = (1ull << clockDividerShift) - 1;
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

    FORCE_INLINE uint8 ReadICRH() const {
        TEMP = ICR;
        return ICR >> 8u;
    }

    FORCE_INLINE uint8 ReadICRL() const {
        return TEMP;
    }

    mutable uint8 TEMP; // temporary storage to handle 16-bit transfers

    // -------------------------------------------------------------------------
    // State

    uint64 cycleCount;
    uint64 clockDividerShift; // derived from TCR.CKS
    uint64 cycleCountMask;    // derived from TCR.CKS

    uint64 CyclesUntilNextTick() const {
        return (1ull << clockDividerShift) - (cycleCount & cycleCountMask);
    }
};

// --- INTC module ---

// 060  R/W  8,16     0000      IPRB    Interrupt priority setting register B
//
//   bits   r/w  code       description
//   15-12  R/W  SCIIP3-0   Serial Communication Interface (SCI) Interrupt Priority Level
//   11-8   R/W  FRTIP3-0   Free-Running Timer (FRT) Interrupt Priority Level
//    7-0   R/W  Reserved   Must be zero
//
//   Interrupt priority levels range from 0 to 15.

// 062  R/W  8,16     0000      VCRA    Vector number setting register A
//
//   bits   r/w  code     description
//     15   R    -        Reserved - must be zero
//   14-8   R/W  SERV6-0  Serial Communication Interface (SCI) Receive-Error Interrupt Vector Number
//      7   R    -        Reserved - must be zero
//    6-0   R/W  SRXV6-0  Serial Communication Interface (SCI) Receive-Data-Full Interrupt Vector Number

// 064  R/W  8,16     0000      VCRB    Vector number setting register B
//
//   bits   r/w  code     description
//     15   R    -        Reserved - must be zero
//   14-8   R/W  STXV6-0  Serial Communication Interface (SCI) Transmit-Data-Empty Interrupt Vector Number
//      7   R    -        Reserved - must be zero
//    6-0   R/W  STEV6-0  Serial Communication Interface (SCI) Transmit-End Interrupt Vector Number

// 066  R/W  8,16     0000      VCRC    Vector number setting register C
//
//   bits   r/w  code     description
//     15   R    -        Reserved - must be zero
//   14-8   R/W  FICV6-0  Free-Running Timer (FRT) Input-Capture Interrupt Vector Number
//      7   R    -        Reserved - must be zero
//    6-0   R/W  FOCV6-0  Free-Running Timer (FRT) Output-Compare Interrupt Vector Number

// 068  R/W  8,16     0000      VCRD    Vector number setting register D
//
//   bits   r/w  code     description
//     15   R    -        Reserved - must be zero
//   14-8   R/W  FOVV6-0  Free-Running Timer (FRT) Overflow Interrupt Vector Number
//    7-0   R    -        Reserved - must be zero

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
union RegICR {
    uint16 u16;
    struct {
        uint16 VECMD : 1;
        uint16 _rsvd1_7 : 7;
        uint16 NMIE : 1;
        uint16 _rsvd9_14 : 6;
        uint16 NMIL : 1;
    };
};

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

// 0E4  R/W  8,16     0000      VCRWDT  Vector number setting register WDT
//
//   bits   r/w  code     description
//     15   R    -        Reserved - must be zero
//   14-8   R/W  WITV6-0  Watchdog Timer (WDT) Interval Interrupt Vector Number
//      7   R    -        Reserved - must be zero
//    6-0   R/W  BCMV6-0  Bus State Controller (BSC) Compare Match Interrupt Vector Number

// --- WDT module ---

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

    FORCE_INLINE void WriteWTCSR(uint8 value) {
        WTCSR.OVF &= bit::extract<7>(value);
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

    /*FORCE_INLINE void WriteRSTCSR(uint8 value) {
        RSTCSR.WOVF &= bit::extract<7>(value);
        RSTCSR.RSTE = bit::extract<6>(value);
        RSTCSR.RSTS = bit::extract<5>(value);
    }*/

    FORCE_INLINE void WriteRSTE_RSTS(uint8 value) {
        RSTCSR.RSTE = bit::extract<6>(value);
        RSTCSR.RSTS = bit::extract<5>(value);
    }

    FORCE_INLINE void WriteWOVF(uint8 value) {
        RSTCSR.WOVF &= bit::extract<7>(value);
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

// --- Power-down module ---

// 091  R/W  8        00        SBYCR   Standby Control Register
//
//   bits   r/w  code   description
//      7   R/W  SBY    Standby (0=SLEEP -> sleep mode, 1=SLEEP -> standby mode)
//      6   R/W  HIZ    Port High Impedance (0=pin state retained in standby mode, 1=pin goes to high impedance)
//      5   R    -      Reserved - must be zero
//      4   R/W  MSTP4  Module Stop 4 - DMAC (0=DMAC runs, 1=halt)
//      3   R/W  MSTP3  Module Stop 3 - MULT (0=multiplication unit runs, 1=halt)
//      2   R/W  MSTP2  Module Stop 2 - DIVU (0=DIVU runs, 1=halt)
//      1   R/W  MSTP1  Module Stop 1 - FRT (0=FRT runs, 1=halt and reset)
//      0   R/W  MSTP0  Module Stop 0 - SCU (0=FRT runs, 1=halt and reset)
union RegSBYCR {
    uint8 u8;
    struct {
        uint8 MSTP0 : 1;
        uint8 MSTP1 : 1;
        uint8 MSTP2 : 1;
        uint8 MSTP3 : 1;
        uint8 MSTP4 : 1;
        uint8 : 1;
        uint8 HIZ : 1;
        uint8 SBY : 1;
    };
};

// --- Cache module ---

inline constexpr std::size_t kCacheWays = 4;
inline constexpr std::size_t kCacheEntries = 64;
inline constexpr std::size_t kCacheLineSize = 16;

struct CacheEntry {
    // Tag layout:
    //   28..10: tag
    //        2: valid bit
    // All other bits must be zero
    // This matches the address array structure
    union Tag {
        uint32 u32;
        struct {
            uint32 : 2;
            uint32 valid : 1;
            uint32 : 7;
            uint32 tagAddress : 19;
            uint32 : 3;
        };
    };
    static_assert(sizeof(Tag) == sizeof(uint32));

    alignas(16) std::array<Tag, kCacheWays> tag;
    alignas(16) std::array<std::array<uint8, kCacheLineSize>, kCacheWays> line;
};

// Stores the cache LRU update bits
struct CacheLRUUpdateBits {
    uint8 andMask;
    uint8 orMask;
};

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
union RegCCR {
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
};

// 0E0, 0E2, 0E4 are in INTC module above

// --- DIVU module ---
// Defined in sh2_divu.hpp

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

enum class DMATransferIncrementMode { Fixed, Increment, Decrement, Reserved };
enum class DMATransferSize { Byte, Word, Longword, QuadLongword };
enum class DMATransferBusMode : uint8 { CycleSteal, Burst };
enum class DMATransferAddressMode : uint8 { Dual, Single };
enum class SignalDetectionMode : uint8 { Level, Edge };
enum class DMAResourceSelect : uint8 { DREQ, RXI, TXI, Reserved };

struct DMAChannel {
    DMAChannel() {
        Reset();
    }

    void Reset() {
        xferSize = DMATransferSize::Byte;
        srcMode = DMATransferIncrementMode::Fixed;
        dstMode = DMATransferIncrementMode::Fixed;
        autoRequest = false;
        ackXferMode = false;
        ackLevel = false;
        dreqSelect = SignalDetectionMode::Level;
        dreqLevel = false;
        xferBusMode = DMATransferBusMode::CycleSteal;
        xferAddressMode = DMATransferAddressMode::Dual;
        irqEnable = false;
        xferEnded = false;
        xferEnabled = false;
        resSelect = DMAResourceSelect::DREQ;
    }

    // Determines if the DMA transfer is enabled for this channel.
    // The DMAC determines that a transfer is active by checking that DE = 1, DME = 1, TE = 0, NMIF = 0, AE = 0.
    // This method returns true if DE = 1 and TE = 0.
    // DME = 1, NMIF = 0 and AE = 0 must be checked externally as they're stored in DMAOR.
    bool IsEnabled() const {
        return xferEnabled && !xferEnded;
    }

    // 180  R/W  32       ud        SAR0    DMA source address register 0
    // 190  R/W  32       ud        SAR1    DMA source address register 1
    //
    //   bits   r/w  code   description
    //   31-0   R/W  -      Source address
    uint32 srcAddress;

    // 184  R/W  32       ud        DAR0    DMA destination address register 0
    // 194  R/W  32       ud        DAR1    DMA destination address register 1
    //
    //   bits   r/w  code   description
    //   31-0   R/W  -      Destination address
    uint32 dstAddress;

    // 188  R/W  32       ud        TCR0    DMA transfer counter register 0
    // 198  R/W  32       ud        TCR1    DMA transfer counter register 1
    //
    //   bits   r/w  code   description
    //  31-24   R    -      Reserved - must be zero
    //   23-0   R/W  -      Transfer count
    uint32 xferCount;

    // 18C  R/W  32       00000000  CHCR0   DMA channel control register 0
    // 19C  R/W  32       00000000  CHCR1   DMA channel control register 1
    //
    //   bits   r/w  code   description
    //  31-16   R    -      Reserved - must be zero
    //  15-14   R/W  DM1-0  Destination address mode
    //                        00 (0) = Fixed
    //                        01 (1) = Increment by transfer unit size
    //                        10 (2) = Decrement by transfer unit size
    //                        11 (3) = Reserved
    //  13-12   R/W  SM1-0  Source address mode
    //                        00 (0) = Fixed
    //                        01 (1) = Increment by transfer unit size
    //                        10 (2) = Decrement by transfer unit size
    //                        11 (3) = Reserved
    //  11-10   R/W  TS1-0  Transfer size
    //                        00 (0) = Byte unit
    //                        01 (1) = Word unit (2 bytes)
    //                        10 (2) = Longword unit (4 bytes)
    //                        11 (3) = 16-byte unit (4 longwords)
    //      9   R/W  AR     Auto-request mode
    //                        0 = Module request mode - external or on-chip SCI
    //                        1 = Auto request mode - generated within DMAC
    //      8   R/W  AM     Acknowledge/Transfer mode
    //                        In dual address mode (TA=0):
    //                          0 = output DACK signal during the data read cycle
    //                          1 = output DACK signal during the data write cycle
    //                        In single address mode (TA=1):
    //                          0 = transfer from memory to device
    //                          1 = transfer from device to memory
    //      7   R/W  AL     Acknowledge level (DACK signal: 0=active-high, 1=active-low)
    //      6   R/W  DS     DREQ select (0=detect by level, 1=detect by edge)
    //      5   R/W  DL     DREQ level (0=low level/falling edge, 1=high level/rising edge)
    //      4   R/W  TB     Transfer bus mode (0=cycle-steal, 1=burst)
    //      3   R/W  TA     Transfer address mode (0=dual address, 1=single address)
    //      2   R/W  IE     Interrupt enable (0=disable, 1=enable)
    //      1   R/W* TE     Transfer-end flag
    //                        read: current transfer end status
    //                          0 = in progress or aborted
    //                          1 = completed
    //                        write:
    //                          0 = clear flag if it was set to 1
    //                          1 = no effect
    //      0   R/W  DE     DMA enable (0=transfer disabled, 1=transfer enabled)

    DMATransferSize xferSize;
    DMATransferIncrementMode srcMode;
    DMATransferIncrementMode dstMode;
    bool autoRequest;
    bool ackXferMode;
    bool ackLevel;
    SignalDetectionMode dreqSelect;
    bool dreqLevel;
    DMATransferBusMode xferBusMode;
    DMATransferAddressMode xferAddressMode;
    bool irqEnable;
    bool xferEnded;
    bool xferEnabled;

    FORCE_INLINE uint32 ReadCHCR() const {
        uint32 value{};
        bit::deposit_into<14, 15>(value, static_cast<uint32>(dstMode));
        bit::deposit_into<12, 13>(value, static_cast<uint32>(srcMode));
        bit::deposit_into<10, 11>(value, static_cast<uint32>(xferSize));
        bit::deposit_into<9>(value, autoRequest);
        bit::deposit_into<8>(value, ackXferMode);
        bit::deposit_into<7>(value, ackLevel);
        bit::deposit_into<6>(value, static_cast<uint32>(dreqSelect));
        bit::deposit_into<5>(value, dreqLevel);
        bit::deposit_into<4>(value, static_cast<uint32>(xferBusMode));
        bit::deposit_into<3>(value, static_cast<uint32>(xferAddressMode));
        bit::deposit_into<2>(value, irqEnable);
        bit::deposit_into<1>(value, xferEnded);
        bit::deposit_into<0>(value, xferEnabled);
        return value;
    }

    FORCE_INLINE void WriteCHCR(uint32 value) {
        dstMode = static_cast<DMATransferIncrementMode>(bit::extract<14, 15>(value));
        srcMode = static_cast<DMATransferIncrementMode>(bit::extract<12, 13>(value));
        xferSize = static_cast<DMATransferSize>(bit::extract<10, 11>(value));
        autoRequest = bit::extract<9>(value);
        ackXferMode = bit::extract<8>(value);
        ackLevel = bit::extract<7>(value);
        dreqSelect = static_cast<SignalDetectionMode>(bit::extract<6>(value));
        dreqLevel = bit::extract<5>(value);
        xferBusMode = static_cast<DMATransferBusMode>(bit::extract<4>(value));
        xferAddressMode = static_cast<DMATransferAddressMode>(bit::extract<3>(value));
        irqEnable = bit::extract<2>(value);
        xferEnded &= bit::extract<1>(value);
        xferEnabled = bit::extract<0>(value);
    }

    // 1A0  R/W  32       ud        VCRDMA0 DMA vector number register 0
    // 1A8  R/W  32       ud        VCRDMA1 DMA vector number register 1
    //
    //   bits   r/w  code   description
    //   31-8   R    -      Reserved - must be zero
    //    7-0   R/W  VC7-0  Vector Number

    // 071  R/W  8        00        DRCR0   DMA request/response selection control register 0
    // 072  R/W  8        00        DRCR1   DMA request/response selection control register 1
    //
    //   bits   r/w  code   description
    //    7-2   R    -      Reserved - must be zero
    //    1-0   R/W  RS1-0  Resource select
    //                        00 (0) = DREQ (external request)
    //                        01 (1) = RXI (on-chip SCI receive-data-full interrupt transfer request)
    //                        10 (2) = TXI (on-chip SCI transmit-data-empty interrupt transfer request)
    //                        11 (3) = Reserved
    DMAResourceSelect resSelect;

    FORCE_INLINE uint8 ReadDRCR() const {
        return static_cast<uint8>(resSelect);
    }

    FORCE_INLINE void WriteDRCR(uint8 value) {
        resSelect = static_cast<DMAResourceSelect>(bit::extract<0, 1>(value));
    }
};

// 1B0  R/W  32       00000000  DMAOR   DMA operation register
//
//   bits   r/w  code   description
//   31-4   R    -      Reserved - must be zero
//      3   R/W  PR     Priority mode
//                        0 = Fixed (channel 0 > channel 1)
//                        1 = Round-robin
//      2   R/W  AE     Address error flag
//                        read: current status (0=no error, 1=error occurred)
//                        write:
//                          0 = clear flag if it was set to 1
//                          1 = no effect
//      1   R/W  NMIF   NMI flag
//                        read: current status (0=no NMI, 1=NMI occurred)
//                        write:
//                          0 = clear flag if it was set to 1
//                          1 = no effect
//      0   R/W  DME    DMA master enable (0=disable all channels, 1=enable all channels)
union RegDMAOR {
    uint32 u32;
    struct {
        uint32 DME : 1;
        uint32 NMIF : 1;
        uint32 AE : 1;
        uint32 PR : 1;
    };
};

// --- BSC module ---
//
// 1E0  R/W  16,32    03F0      BCR1    Bus Control Register 1
union RegBCR1 {
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
};

// 1E4  R/W  16,32    00FC      BCR2    Bus Control Register 2
union RegBCR2 {
    uint16 u16;
    struct {
        uint16 _rsvd0 : 1;
        uint16 _rsvd1 : 1;
        uint16 A1SZn : 2;
        uint16 A2SZn : 2;
        uint16 A3SZn : 2;
    };
};

// 1E8  R/W  16,32    AAFF      WCR     Wait Control Register
union RegWCR {
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
};

// 1EC  R/W  16,32    0000      MCR     Individual Memory Control Register
union RegMCR {
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
};

// 1F0  R/W  16,32    0000      RTCSR   Refresh Timer Control/Status Register
union RegRTCSR {
    uint16 u16;
    struct {
        uint16 _rsvd0 : 1;
        uint16 _rsvd1 : 1;
        uint16 _rsvd2 : 1;
        uint16 CKSn : 3;
        uint16 CMIE : 1;
        uint16 CMF : 1;
    };
};

// 1F4  R/W  16,32    0000      RTCNT   Refresh Timer Counter
using RegRTCNT = uint8;

// 1F8  R/W  16,32    0000      RTCOR   Refresh Timer Constant Register
using RegRTCOR = uint8;

} // namespace satemu::sh2
