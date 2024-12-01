#pragma once

#include "sh2_bus.hpp"

#include <satemu/core_types.hpp>

#include <satemu/util/inline.hpp>

namespace satemu::sh2 {

class SH2 {
public:
    SH2(SH2Bus &bus, bool master);

    void Reset(bool hard);

    void Step();

    void SetInterruptLevel(uint8 level);

    FORCE_INLINE uint32 GetPC() const {
        return PC;
    }

private:
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

    SH2Bus &m_bus;

    // -------------------------------------------------------------------------
    // Memory accessors

    // According to the SH7604 manual, the address space is divided into these areas:
    //
    // Address range            Space                           Memory
    // 0x00000000..0x01FFFFFF   CS0 space, cache area           Ordinary space or burst ROM
    // 0x02000000..0x03FFFFFF   CS1 space, cache area           Ordinary space
    // 0x04000000..0x05FFFFFF   CS2 space, cache area           Ordinary space or synchronous DRAM
    // 0x06000000..0x07FFFFFF   CS3 space, cache area           Ordinary space, synchronous SDRAM, DRAM or pseudo-DRAM
    // 0x08000000..0x1FFFFFFF   Reserved
    // 0x20000000..0x21FFFFFF   CS0 space, cache-through area   Ordinary space or burst ROM
    // 0x22000000..0x23FFFFFF   CS1 space, cache-through area   Ordinary space
    // 0x24000000..0x25FFFFFF   CS2 space, cache-through area   Ordinary space or synchronous DRAM
    // 0x26000000..0x27FFFFFF   CS3 space, cache-through area   Ordinary space, synchronous SDRAM, DRAM or pseudo-DRAM
    // 0x28000000..0x3FFFFFFF   Reserved
    // 0x40000000..0x47FFFFFF   Associative purge space
    // 0x48000000..0x5FFFFFFF   Reserved
    // 0x60000000..0x7FFFFFFF   Address array, read/write space
    // 0x80000000..0x9FFFFFFF   Reserved  [undocumented mirror of 0xC0000000..0xDFFFFFFF]
    // 0xA0000000..0xBFFFFFFF   Reserved  [undocumented mirror of 0x20000000..0x3FFFFFFF]
    // 0xC0000000..0xC0000FFF   Data array, read/write space
    // 0xC0001000..0xDFFFFFFF   Reserved
    // 0xE0000000..0xFFFF7FFF   Reserved
    // 0xFFFF8000..0xFFFFBFFF   For setting synchronous DRAM mode
    // 0xFFFFC000..0xFFFFFDFF   Reserved
    // 0xFFFFFE00..0xFFFFFFFF   On-chip peripheral modules
    //
    // The cache uses address bits 31..29 to specify its behavior:
    //    Bits  Partition                       Cache operation
    //    000   Cache area                      Cache used when CCR.CE=1
    //    001   Cache-through area              Cache bypassed
    //    010   Associative purge area          Purge accessed cache lines (reads return 0x2312)
    //    011   Address array read/write area   Cache addresses acessed directly (1 KiB, mirrored)
    //    100   [undocumented, same as 110]
    //    101   [undocumented, same as 001]
    //    110   Data array read/write area      Cache data acessed directly (4 KiB, mirrored)
    //    111   I/O area (on-chip registers)    Cache bypassed

    template <mem_access_type T>
    T MemRead(uint32 address);

    template <mem_access_type T>
    void MemWrite(uint32 address, T value);

    uint8 MemReadByte(uint32 address);
    uint16 MemReadWord(uint32 address);
    uint32 MemReadLong(uint32 address);

    void MemWriteByte(uint32 address, uint8 value);
    void MemWriteWord(uint32 address, uint16 value);
    void MemWriteLong(uint32 address, uint32 value);

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
    alignas(16) std::array<CacheEntry, kCacheEntries> m_cacheEntries;

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
    T OnChipRegRead(uint32 address);

    template <mem_access_type T>
    void OnChipRegWrite(uint32 address, T baseValue);

    // -------------------------------------------------------------------------
    // Interrupts

    uint8 m_pendingIRL;

    struct PendingInterruptInfo {
        uint8 priority;
        uint8 vecNum;
    } m_pendingInterrupt;

    void CheckInterrupts();

    // -------------------------------------------------------------------------
    // Execution

    template <bool delaySlot>
    void Execute(uint32 address);

    void EnterException(uint8 vectorNumber);

    void NOP(); // nop

    void SLEEP(); // sleep

    void MOV(uint16 rm, uint16 rn);                 // mov   Rm, Rn
    void MOVBL(uint16 rm, uint16 rn);               // mov.b @Rm, Rn
    void MOVWL(uint16 rm, uint16 rn);               // mov.w @Rm, Rn
    void MOVLL(uint16 rm, uint16 rn);               // mov.l @Rm, Rn
    void MOVBL0(uint16 rm, uint16 rn);              // mov.b @(R0,Rm), Rn
    void MOVWL0(uint16 rm, uint16 rn);              // mov.w @(R0,Rm), Rn
    void MOVLL0(uint16 rm, uint16 rn);              // mov.l @(R0,Rm), Rn
    void MOVBL4(uint16 rm, uint16 disp);            // mov.b @(disp,Rm), R0
    void MOVWL4(uint16 rm, uint16 disp);            // mov.w @(disp,Rm), R0
    void MOVLL4(uint16 rm, uint16 disp, uint16 rn); // mov.l @(disp,Rm), Rn
    void MOVBLG(uint16 disp);                       // mov.b @(disp,GBR), R0
    void MOVWLG(uint16 disp);                       // mov.w @(disp,GBR), R0
    void MOVLLG(uint16 disp);                       // mov.l @(disp,GBR), R0
    void MOVBM(uint16 rm, uint16 rn);               // mov.b Rm, @-Rn
    void MOVWM(uint16 rm, uint16 rn);               // mov.w Rm, @-Rn
    void MOVLM(uint16 rm, uint16 rn);               // mov.l Rm, @-Rn
    void MOVBP(uint16 rm, uint16 rn);               // mov.b @Rm+, Rn
    void MOVWP(uint16 rm, uint16 rn);               // mov.w @Rm+, Rn
    void MOVLP(uint16 rm, uint16 rn);               // mov.l @Rm+, Rn
    void MOVBS(uint16 rm, uint16 rn);               // mov.b Rm, @Rn
    void MOVWS(uint16 rm, uint16 rn);               // mov.w Rm, @Rn
    void MOVLS(uint16 rm, uint16 rn);               // mov.l Rm, @Rn
    void MOVBS0(uint16 rm, uint16 rn);              // mov.b Rm, @(R0,Rn)
    void MOVWS0(uint16 rm, uint16 rn);              // mov.w Rm, @(R0,Rn)
    void MOVLS0(uint16 rm, uint16 rn);              // mov.l Rm, @(R0,Rn)
    void MOVBS4(uint16 disp, uint16 rn);            // mov.b R0, @(disp,Rn)
    void MOVWS4(uint16 disp, uint16 rn);            // mov.w R0, @(disp,Rn)
    void MOVLS4(uint16 rm, uint16 disp, uint16 rn); // mov.l Rm, @(disp,Rn)
    void MOVBSG(uint16 disp);                       // mov.b R0, @(disp,GBR)
    void MOVWSG(uint16 disp);                       // mov.w R0, @(disp,GBR)
    void MOVLSG(uint16 disp);                       // mov.l R0, @(disp,GBR)
    void MOVI(uint16 imm, uint16 rn);               // mov   #imm, Rn
    void MOVWI(uint16 disp, uint16 rn);             // mov.w @(disp,PC), Rn
    void MOVLI(uint16 disp, uint16 rn);             // mov.l @(disp,PC), Rn
    void MOVA(uint16 disp);                         // mova  @(disp,PC), R0
    void MOVT(uint16 rn);                           // movt  Rn
    void CLRT();                                    // clrt
    void SETT();                                    // sett

    void EXTSB(uint16 rm, uint16 rn); // exts.b Rm, Rn
    void EXTSW(uint16 rm, uint16 rn); // exts.w Rm, Rn
    void EXTUB(uint16 rm, uint16 rn); // extu.b Rm, Rn
    void EXTUW(uint16 rm, uint16 rn); // extu.w Rm, Rn

    void LDCGBR(uint16 rm);   // ldc   Rm, GBR
    void LDCSR(uint16 rm);    // ldc   Rm, SR
    void LDCVBR(uint16 rm);   // ldc   Rm, VBR
    void LDCMGBR(uint16 rm);  // ldc.l @Rm+, GBR
    void LDCMSR(uint16 rm);   // ldc.l @Rm+, SR
    void LDCMVBR(uint16 rm);  // ldc.l @Rm+, VBR
    void LDSMACH(uint16 rm);  // lds   Rm, MACH
    void LDSMACL(uint16 rm);  // lds   Rm, MACL
    void LDSPR(uint16 rm);    // lds   Rm, PR
    void LDSMMACH(uint16 rm); // lds.l @Rm+, MACH
    void LDSMMACL(uint16 rm); // lds.l @Rm+, MACL
    void LDSMPR(uint16 rm);   // lds.l @Rm+, PR
    void STCGBR(uint16 rn);   // stc   GBR, Rn
    void STCSR(uint16 rn);    // stc   SR, Rn
    void STCVBR(uint16 rn);   // stc   VBR, Rn
    void STCMGBR(uint16 rn);  // stc.l GBR, @-Rn
    void STCMSR(uint16 rn);   // stc.l SR, @-Rn
    void STCMVBR(uint16 rn);  // stc.l VBR, @-Rn
    void STSMACH(uint16 rn);  // sts   MACH, Rn
    void STSMACL(uint16 rn);  // sts   MACL, Rn
    void STSPR(uint16 rn);    // sts   PR, Rn
    void STSMMACH(uint16 rn); // sts.l MACH, @-Rn
    void STSMMACL(uint16 rn); // sts.l MACL, @-Rn
    void STSMPR(uint16 rn);   // sts.l PR, @-Rn

    void SWAPB(uint16 rm, uint16 rn); // swap.b Rm, Rn
    void SWAPW(uint16 rm, uint16 rn); // swap.w Rm, Rn
    void XTRCT(uint16 rm, uint16 rn); // xtrct  Rm, Rn

    void ADD(uint16 rm, uint16 rn);   // add    Rm, Rn
    void ADDI(uint16 imm, uint16 rn); // add    imm, Rn
    void ADDC(uint16 rm, uint16 rn);  // addc   Rm, Rn
    void ADDV(uint16 rm, uint16 rn);  // addv   Rm, Rn
    void AND(uint16 rm, uint16 rn);   // and    Rm, Rn
    void ANDI(uint16 imm);            // and    imm, R0
    void ANDM(uint16 imm);            // and.   b imm, @(R0,GBR)
    void NEG(uint16 rm, uint16 rn);   // neg    Rm, Rn
    void NEGC(uint16 rm, uint16 rn);  // negc   Rm, Rn
    void NOT(uint16 rm, uint16 rn);   // not    Rm, Rn
    void OR(uint16 rm, uint16 rn);    // or     Rm, Rn
    void ORI(uint16 imm);             // or     imm, Rn
    void ORM(uint16 imm);             // or.b   imm, @(R0,GBR)
    void ROTCL(uint16 rn);            // rotcl  Rn
    void ROTCR(uint16 rn);            // rotcr  Rn
    void ROTL(uint16 rn);             // rotl   Rn
    void ROTR(uint16 rn);             // rotr   Rn
    void SHAL(uint16 rn);             // shal   Rn
    void SHAR(uint16 rn);             // shar   Rn
    void SHLL(uint16 rn);             // shll   Rn
    void SHLL2(uint16 rn);            // shll2  Rn
    void SHLL8(uint16 rn);            // shll8  Rn
    void SHLL16(uint16 rn);           // shll16 Rn
    void SHLR(uint16 rn);             // shlr   Rn
    void SHLR2(uint16 rn);            // shlr2  Rn
    void SHLR8(uint16 rn);            // shlr8  Rn
    void SHLR16(uint16 rn);           // shlr16 Rn
    void SUB(uint16 rm, uint16 rn);   // sub    Rm, Rn
    void SUBC(uint16 rm, uint16 rn);  // subc   Rm, Rn
    void SUBV(uint16 rm, uint16 rn);  // subv   Rm, Rn
    void XOR(uint16 rm, uint16 rn);   // xor    Rm, Rn
    void XORI(uint16 imm);            // xor    imm, Rn
    void XORM(uint16 imm);            // xor.b  imm, @(R0,GBR)

    void DT(uint16 rn); // dt Rn

    void CLRMAC();                    // clrmac
    void MACW(uint16 rm, uint16 rn);  // mac.w   @Rm+, @Rn+
    void MACL(uint16 rm, uint16 rn);  // mac.l   @Rm+, @Rn+
    void MULL(uint16 rm, uint16 rn);  // mul.l   Rm, Rn
    void MULS(uint16 rm, uint16 rn);  // muls.w  Rm, Rn
    void MULU(uint16 rm, uint16 rn);  // mulu.w  Rm, Rn
    void DMULS(uint16 rm, uint16 rn); // dmuls.l Rm, Rn
    void DMULU(uint16 rm, uint16 rn); // dmulu.l Rm, Rn

    void DIV0S(uint16 rm, uint16 rn); // div0s Rm, Rn
    void DIV0U();                     // div0u
    void DIV1(uint16 rm, uint16 rn);  // div1  Rm, Rn

    void CMPIM(uint16 imm);            // cmp/eq  imm, R0
    void CMPEQ(uint16 rm, uint16 rn);  // cmp/eq  Rm, Rn
    void CMPGE(uint16 rm, uint16 rn);  // cmp/ge  Rm, Rn
    void CMPGT(uint16 rm, uint16 rn);  // cmp/gt  Rm, Rn
    void CMPHI(uint16 rm, uint16 rn);  // cmp/hi  Rm, Rn
    void CMPHS(uint16 rm, uint16 rn);  // cmp/hs  Rm, Rn
    void CMPPL(uint16 rn);             // cmp/pl  Rn
    void CMPPZ(uint16 rn);             // cmp/pz  Rn
    void CMPSTR(uint16 rm, uint16 rn); // cmp/str Rm, Rn
    void TAS(uint16 rn);               // tas.b   @Rn
    void TST(uint16 rm, uint16 rn);    // tst     Rm, Rn
    void TSTI(uint16 imm);             // tst     imm, R0
    void TSTM(uint16 imm);             // tst.b   imm, @(R0,GBR)

    void BF(uint16 disp);   // bf    disp
    void BFS(uint16 disp);  // bf/s  disp
    void BT(uint16 disp);   // bt    disp
    void BTS(uint16 disp);  // bt/s  disp
    void BRA(uint16 disp);  // bra   disp
    void BRAF(uint16 rm);   // braf  Rm
    void BSR(uint16 disp);  // bsr   disp
    void BSRF(uint16 rm);   // bsrf  Rm
    void JMP(uint16 rm);    // jmp   @Rm
    void JSR(uint16 rm);    // jsr   @Rm
    void TRAPA(uint16 imm); // trapa imm

    void RTE(); // rte
    void RTS(); // rts
};

} // namespace satemu::sh2
