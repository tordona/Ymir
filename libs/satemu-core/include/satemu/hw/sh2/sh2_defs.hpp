#pragma once

#include <satemu/core_types.hpp>

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

// Represents a 16-bit register with each byte individually accessible
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
};

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
};

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
};

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
};

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
};

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
union IPRA_t {
    Reg16 val;
    struct {
        uint16 _rsvd0_3 : 4;
        uint16 WDTIPn : 4;
        uint16 DMACIPn : 4;
        uint16 DIVUIPn : 4;
    };
};

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
};

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

inline constexpr size_t kCacheWays = 4;
inline constexpr size_t kCacheEntries = 64;
inline constexpr size_t kCacheLineSize = 16;

struct CacheEntry {
    // Tag layout:
    //   28..10: tag
    //        2: valid bit
    // All other bits must be zero
    // This matches the address array structure
    std::array<uint32, kCacheWays> tag;
    std::array<std::array<uint8, kCacheLineSize>, kCacheWays> line;
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
};

// 0E0, 0E2, 0E4 are in INTC module above

// --- DIVU module ---
//
// 100  R/W  32       ud        DVSR    Divisor register
//
//   bits   r/w  code   description
//   31-0   R/W  -      Divisor number
using DVSR_t = uint32;

// 104  R/W  32       ud        DVDNT   Dividend register L for 32-bit division
//
//   bits   r/w  code   description
//   31-0   R/W  -      32-bit dividend number
using DVDNT_t = uint32;

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
};

// 10C  R/W  16,32    ud        VCRDIV  Vector number register setting DIV
//
//   bits   r/w  code   description
//  31-16   R    -      Reserved - must be zero
//   15-0   R/W  -      Interrupt Vector Number
using VCRDIV_t = uint16;

// 110  R/W  32       ud        DVDNTH  Dividend register H
//
//   bits   r/w  code   description
//   31-0   R/W  -      64-bit dividend number (upper half)
using DVDNTH_t = uint32;

// 114  R/W  32       ud        DVDNTL  Dividend register L
//
//   bits   r/w  code   description
//   31-0   R/W  -      64-bit dividend number (lower half)
using DVDNTL_t = uint32;

// 120..13F are mirrors of 100..11F

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
using VCRDMA0_t = uint8;

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
using VCRDMA1_t = uint8;

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
};

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
};

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
};

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
};

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
};

// 1F4  R/W  16,32    0000      RTCNT   Refresh Timer Counter
using RTCNT_t = uint8;

// 1F8  R/W  16,32    0000      RTCOR   Refresh Timer Constant Register
using RTCOR_t = uint8;

} // namespace satemu::sh2
