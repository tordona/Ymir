#pragma once

#include "sh2_decode.hpp"
#include "sh2_defs.hpp"

#include <satemu/core/types.hpp>
#include <satemu/hw/hw_defs.hpp>

#include <satemu/debug/debug_tracer.hpp>

#include <satemu/util/debug_print.hpp>

#include <array>
#include <new>
#include <vector>

// -----------------------------------------------------------------------------
// Forward declarations

namespace satemu::debug {

struct SH2DebugProbe;

} // namespace satemu::debug

namespace satemu::sh2 {

class SH2Bus;

} // namespace satemu::sh2

// -----------------------------------------------------------------------------

namespace satemu::sh2 {

inline constexpr dbg::Level sh2DebugLevel = dbg::debugLevel;

enum class SH2BranchType { JSR, BSR, TRAPA, Exception, UserCapture };

struct SH2Regs {
    std::array<uint32, 16> R;
    uint32 PC;
    uint32 PR;
    uint32 SR;
    uint32 GBR;
    uint32 VBR;
    uint64 MAC;
};

class NullSH2Tracer {
public:
    NullSH2Tracer(bool) {}

    void Reset() {}
    void Dump() {}

    void ExecTrace(SH2Regs) {}

    void JSR(SH2Regs) {}
    void BSR(SH2Regs) {}
    void TRAPA(SH2Regs) {}
    void Exception(SH2Regs, uint8) {}
    void UserCapture(SH2Regs) {}

    void RTE(SH2Regs) {}
    void RTS(SH2Regs) {}
};

class RealSH2Tracer {
public:
    RealSH2Tracer(bool master);

    void Reset();
    void Dump();

    void ExecTrace(SH2Regs regs);

    void JSR(SH2Regs regs);
    void BSR(SH2Regs regs);
    void TRAPA(SH2Regs regs);
    void Exception(SH2Regs regs, uint8 vec);
    void UserCapture(SH2Regs regs);

    void RTE(SH2Regs regs);
    void RTS(SH2Regs regs);

private:
    struct Entry {
        SH2BranchType type;
        SH2Regs regs;
        uint8 vec;
    };

    bool m_master;
    std::vector<Entry> m_entries;
    std::array<SH2Regs, 256> m_execTrace;
    std::size_t m_execTraceHead;
    std::size_t m_execTraceCount;
};

using SH2Tracer = NullSH2Tracer;
// using SH2Tracer = RealSH2Tracer;

// -----------------------------------------------------------------------------

class SH2 {
public:
    SH2(SH2Bus &bus, bool master, debug::TracerContext &debugTracer);

    void Reset(bool hard, bool watchdogInitiated = false);

    template <bool debug>
    void Advance(uint64 cycles);

    std::array<uint32, 16> &GetGPRs() {
        return R;
    }

    const std::array<uint32, 16> &GetGPRs() const {
        return R;
    }

    uint32 GetPC() const {
        return PC;
    }

    void SetPC(uint32 pc) {
        PC = pc;
    }

    void SetExternalInterrupt(uint8 level, uint8 vecNum);

    bool GetNMI() const;
    void SetNMI();

    void TriggerFRTInputCapture();

private:
    // -------------------------------------------------------------------------
    // CPU state

    // R0 through R15.
    // R15 is also used as the hardware stack pointer (SP).
    alignas(std::hardware_destructive_interference_size) std::array<uint32, 16> R;

    uint32 PC;
    uint32 PR;

    union RegMAC {
        uint64 u64;
        struct {
            uint32 L;
            uint32 H;
        };
    } MAC;

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
    } SR;
    uint32 GBR;
    uint32 VBR;

    uint32 m_delaySlotTarget;
    bool m_delaySlot;

    // -------------------------------------------------------------------------
    // Debugger

    friend struct debug::SH2DebugProbe;

    SH2Tracer m_tracer;
    debug::TracerContext &m_debugTracer;

    const dbg::Category<sh2DebugLevel> &m_log;

    // -------------------------------------------------------------------------
    // Memory accessors

    SH2Bus &m_bus;

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

    template <mem_primitive T, bool instrFetch>
    T MemRead(uint32 address);

    template <mem_primitive T>
    void MemWrite(uint32 address, T value);

    uint16 FetchInstruction(uint32 address);

    uint8 MemReadByte(uint32 address);
    uint16 MemReadWord(uint32 address);
    uint32 MemReadLong(uint32 address);

    void MemWriteByte(uint32 address, uint8 value);
    void MemWriteWord(uint32 address, uint16 value);
    void MemWriteLong(uint32 address, uint32 value);

    // Returns 00 00 00 01 00 02 00 03 00 04 00 05 00 06 00 07 ... repeating
    template <mem_primitive T>
    T OpenBusSeqRead(uint32 address);

    // -------------------------------------------------------------------------
    // On-chip peripherals

    template <mem_primitive T>
    T OnChipRegRead(uint32 address);

    uint8 OnChipRegReadByte(uint32 address);
    uint16 OnChipRegReadWord(uint32 address);
    uint32 OnChipRegReadLong(uint32 address);

    template <mem_primitive T>
    void OnChipRegWrite(uint32 address, T value);

    void OnChipRegWriteByte(uint32 address, uint8 value);
    void OnChipRegWriteWord(uint32 address, uint16 value);
    void OnChipRegWriteLong(uint32 address, uint32 value);

    // --- SCI module ---
    // TODO

    // --- BSC module ---

    RegBCR1 BCR1;   // 1E0  R/W  16,32    03F0      BCR1    Bus Control Register 1
    RegBCR2 BCR2;   // 1E4  R/W  16,32    00FC      BCR2    Bus Control Register 2
    RegWCR WCR;     // 1E8  R/W  16,32    AAFF      WCR     Wait Control Register
    RegMCR MCR;     // 1EC  R/W  16,32    0000      MCR     Individual Memory Control Register
    RegRTCSR RTCSR; // 1F0  R/W  16,32    0000      RTCSR   Refresh Timer Control/Status Register
    RegRTCNT RTCNT; // 1F4  R/W  16,32    0000      RTCNT   Refresh Timer Counter
    RegRTCOR RTCOR; // 1F8  R/W  16,32    0000      RTCOR   Refresh Timer Constant Register

    // --- DMAC module ---

    RegDMAOR DMAOR;
    std::array<DMAChannel, 2> m_dmaChannels;

    // Determines if a DMA transfer is active for the specified channel.
    // A transfer is active if DE = 1, DME = 1, TE = 0, NMIF = 0 and AE = 0.
    bool IsDMATransferActive(const DMAChannel &ch) const;

    void RunDMAC(uint32 channel);

    // --- WDT module ---

    WatchdogTimer WDT;

    void AdvanceWDT(uint64 cycles);

    // --- Power-down module ---

    RegSBYCR SBYCR; // 091  R/W  8        00        SBYCR   Standby Control Register

    // --- DIVU module ---

    RegDVSR DVSR;       // 100  R/W  32       ud        DVSR    Divisor register
    RegDVDNT DVDNT;     // 104  R/W  32       ud        DVDNT   Dividend register L for 32-bit division
    RegDVCR DVCR;       // 108  R/W  16,32    00000000  DVCR    Division control register
                        // 10C  R/W  16,32    ud        VCRDIV  Vector number register setting DIV
    RegDVDNTH DVDNTH;   // 110  R/W  32       ud        DVDNTH  Dividend register H
    RegDVDNTL DVDNTL;   // 114  R/W  32       ud        DVDNTL  Dividend register L
    RegDVDNTUH DVDNTUH; // 118  R/W  32       ud        DVDNTUH Undocumented dividend register H
    RegDVDNTUL DVDNTUL; // 11C  R/W  32       ud        DVDNTUL Undocumented dividend register L

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

    // --- UBC module (channel B) ---

    // --- FRT module ---

    FreeRunningTimer FRT;

    void AdvanceFRT(uint64 cycles);

    // -------------------------------------------------------------------------
    // Interrupts

    RegICR ICR; // 0E0  R/W  8,16     0000      ICR     Interrupt control register

    // Interrupt sources, sorted by default priority from lowest to highest
    enum class InterruptSource : uint8 {
        None,          // Priority       Vector
        FRT_OVI,       // IPRB.FRTIPn    VCRD.FOVVn
        FRT_OCI,       // IPRB.FRTIPn    VCRC.FOCVn
        FRT_ICI,       // IPRB.FRTIPn    VCRC.FICVn
        SCI_TEI,       // IPRB.SCIIPn    VCRB.STEVn
        SCI_TXI,       // IPRB.SCIIPn    VCRB.STXVn
        SCI_RXI,       // IPRB.SCIIPn    VCRA.SRXVn
        SCI_ERI,       // IPRB.SCIIPn    VCRA.SERVn
        BSC_REF_CMI,   // IPRA.WDTIPn    VCRWDT
        WDT_ITI,       // IPRA.WDTIPn    VCRWDT
        DMAC1_XferEnd, // IPRA.DMACIPn   VCRDMA1
        DMAC0_XferEnd, // IPRA.DMACIPn   VCRDMA0
        DIVU_OVFI,     // IPRA.DIVUIPn   VCRDIV
        IRL,           // 15-1           0x40 + (level >> 1)
        UserBreak,     // 15             0x0C
        NMI            // 16             0x0B
    };

    std::array<uint8, 16> m_intrLevels;
    std::array<uint8, 16> m_intrVectors;

    struct PendingInterruptInfo {
        InterruptSource source;
        uint8 level;
    } m_pendingInterrupt;

    bool m_NMI;
    uint8 m_externalIntrVector;

    // Gets the interrupt vector number for the specified interrupt source.
    uint8 GetInterruptVector(InterruptSource source);

    // Sets the interrupt vector number for the specified interrupt source.
    void SetInterruptVector(InterruptSource source, uint8 vector);

    // Gets the interrupt level for the specified interrupt source.
    uint8 GetInterruptLevel(InterruptSource source);

    // Sets the interrupt level for the specified interrupt source.
    void SetInterruptLevel(InterruptSource source, uint8 level);

    // Raises the interrupt signal of the specified source.
    void RaiseInterrupt(InterruptSource source);

    // Lowers the interrupt signal of the specified source.
    void LowerInterrupt(InterruptSource source);

    // Updates the pending interrupt level if it matches one of the specified sources.
    template <InterruptSource source, InterruptSource... sources>
    void UpdateInterruptLevels();

    // Recalculates the highest priority interrupt to be serviced.
    void RecalcInterrupts();

    // Checks if the CPU should service an interrupt.
    bool CheckInterrupts();

    // -------------------------------------------------------------------------
    // Helper functions

    void SetupDelaySlot(uint32 targetAddress);
    void EnterException(uint8 vectorNumber);

    // -------------------------------------------------------------------------
    // Cache

    RegCCR CCR; // 092  R/W  8        00        CCR     Cache Control Register

    alignas(16) std::array<CacheEntry, kCacheEntries> m_cacheEntries;
    alignas(16) std::array<uint8, kCacheEntries> m_cacheLRU;
    uint8 m_cacheReplaceANDMask;
    std::array<uint8, 2> m_cacheReplaceORMask; // [0]=data, [1]=code

    alignas(16) static constexpr std::array<CacheLRUUpdateBits, 4> kCacheLRUUpdateBits = {{
        // AND mask       OR mask
        {0b111000u, 0b000000u}, // way 0
        {0b011001u, 0b100000u}, // way 1
        {0b101010u, 0b010100u}, // way 2
        {0b110100u, 0b001011u}, // way 3
    }};

    alignas(16) static constexpr auto kCacheLRUWaySelect = [] {
        std::array<sint8, 64> arr{};
        arr.fill(-1);
        for (uint8 i = 0; i < 8; i++) {
            arr[0b111000 | bit::scatter<0b000111>(i)] = 0; // way 0: 111...
            arr[0b000110 | bit::scatter<0b011001>(i)] = 1; // way 1: 0..11.
            arr[0b000001 | bit::scatter<0b101010>(i)] = 2; // way 2: .0.0.1
            arr[0b000000 | bit::scatter<0b110100>(i)] = 3; // way 3: ..0.00
        }
        return arr;
    }();

    void WriteCCR(uint8 value);

    // -------------------------------------------------------------------------
    // Interpreter

    template <bool debug>
    void Execute();

    // -------------------------------------------------------------------------
    // Instruction interpreters

    void NOP(); // nop

    void SLEEP(); // sleep

    void MOV(const DecodedArgs &args);    // mov   Rm, Rn
    void MOVBL(const DecodedArgs &args);  // mov.b @Rm, Rn
    void MOVWL(const DecodedArgs &args);  // mov.w @Rm, Rn
    void MOVLL(const DecodedArgs &args);  // mov.l @Rm, Rn
    void MOVBL0(const DecodedArgs &args); // mov.b @(R0,Rm), Rn
    void MOVWL0(const DecodedArgs &args); // mov.w @(R0,Rm), Rn
    void MOVLL0(const DecodedArgs &args); // mov.l @(R0,Rm), Rn
    void MOVBL4(const DecodedArgs &args); // mov.b @(disp,Rm), R0
    void MOVWL4(const DecodedArgs &args); // mov.w @(disp,Rm), R0
    void MOVLL4(const DecodedArgs &args); // mov.l @(disp,Rm), Rn
    void MOVBLG(const DecodedArgs &args); // mov.b @(disp,GBR), R0
    void MOVWLG(const DecodedArgs &args); // mov.w @(disp,GBR), R0
    void MOVLLG(const DecodedArgs &args); // mov.l @(disp,GBR), R0
    void MOVBM(const DecodedArgs &args);  // mov.b Rm, @-Rn
    void MOVWM(const DecodedArgs &args);  // mov.w Rm, @-Rn
    void MOVLM(const DecodedArgs &args);  // mov.l Rm, @-Rn
    void MOVBP(const DecodedArgs &args);  // mov.b @Rm+, Rn
    void MOVWP(const DecodedArgs &args);  // mov.w @Rm+, Rn
    void MOVLP(const DecodedArgs &args);  // mov.l @Rm+, Rn
    void MOVBS(const DecodedArgs &args);  // mov.b Rm, @Rn
    void MOVWS(const DecodedArgs &args);  // mov.w Rm, @Rn
    void MOVLS(const DecodedArgs &args);  // mov.l Rm, @Rn
    void MOVBS0(const DecodedArgs &args); // mov.b Rm, @(R0,Rn)
    void MOVWS0(const DecodedArgs &args); // mov.w Rm, @(R0,Rn)
    void MOVLS0(const DecodedArgs &args); // mov.l Rm, @(R0,Rn)
    void MOVBS4(const DecodedArgs &args); // mov.b R0, @(disp,Rn)
    void MOVWS4(const DecodedArgs &args); // mov.w R0, @(disp,Rn)
    void MOVLS4(const DecodedArgs &args); // mov.l Rm, @(disp,Rn)
    void MOVBSG(const DecodedArgs &args); // mov.b R0, @(disp,GBR)
    void MOVWSG(const DecodedArgs &args); // mov.w R0, @(disp,GBR)
    void MOVLSG(const DecodedArgs &args); // mov.l R0, @(disp,GBR)
    void MOVI(const DecodedArgs &args);   // mov   #imm, Rn
    void MOVWI(const DecodedArgs &args);  // mov.w @(disp,PC), Rn
    void MOVLI(const DecodedArgs &args);  // mov.l @(disp,PC), Rn
    void MOVA(const DecodedArgs &args);   // mova  @(disp,PC), R0
    void MOVT(const DecodedArgs &args);   // movt  Rn
    void CLRT();                          // clrt
    void SETT();                          // sett

    void EXTSB(const DecodedArgs &args); // exts.b Rm, Rn
    void EXTSW(const DecodedArgs &args); // exts.w Rm, Rn
    void EXTUB(const DecodedArgs &args); // extu.b Rm, Rn
    void EXTUW(const DecodedArgs &args); // extu.w Rm, Rn
    void SWAPB(const DecodedArgs &args); // swap.b Rm, Rn
    void SWAPW(const DecodedArgs &args); // swap.w Rm, Rn
    void XTRCT(const DecodedArgs &args); // xtrct  Rm, Rn

    void LDCGBR(const DecodedArgs &args);   // ldc   Rm, GBR
    void LDCSR(const DecodedArgs &args);    // ldc   Rm, SR
    void LDCVBR(const DecodedArgs &args);   // ldc   Rm, VBR
    void LDCMGBR(const DecodedArgs &args);  // ldc.l @Rm+, GBR
    void LDCMSR(const DecodedArgs &args);   // ldc.l @Rm+, SR
    void LDCMVBR(const DecodedArgs &args);  // ldc.l @Rm+, VBR
    void LDSMACH(const DecodedArgs &args);  // lds   Rm, MACH
    void LDSMACL(const DecodedArgs &args);  // lds   Rm, MACL
    void LDSPR(const DecodedArgs &args);    // lds   Rm, PR
    void LDSMMACH(const DecodedArgs &args); // lds.l @Rm+, MACH
    void LDSMMACL(const DecodedArgs &args); // lds.l @Rm+, MACL
    void LDSMPR(const DecodedArgs &args);   // lds.l @Rm+, PR
    void STCGBR(const DecodedArgs &args);   // stc   GBR, Rn
    void STCSR(const DecodedArgs &args);    // stc   SR, Rn
    void STCVBR(const DecodedArgs &args);   // stc   VBR, Rn
    void STCMGBR(const DecodedArgs &args);  // stc.l GBR, @-Rn
    void STCMSR(const DecodedArgs &args);   // stc.l SR, @-Rn
    void STCMVBR(const DecodedArgs &args);  // stc.l VBR, @-Rn
    void STSMACH(const DecodedArgs &args);  // sts   MACH, Rn
    void STSMACL(const DecodedArgs &args);  // sts   MACL, Rn
    void STSPR(const DecodedArgs &args);    // sts   PR, Rn
    void STSMMACH(const DecodedArgs &args); // sts.l MACH, @-Rn
    void STSMMACL(const DecodedArgs &args); // sts.l MACL, @-Rn
    void STSMPR(const DecodedArgs &args);   // sts.l PR, @-Rn

    void ADD(const DecodedArgs &args);    // add    Rm, Rn
    void ADDI(const DecodedArgs &args);   // add    imm, Rn
    void ADDC(const DecodedArgs &args);   // addc   Rm, Rn
    void ADDV(const DecodedArgs &args);   // addv   Rm, Rn
    void AND(const DecodedArgs &args);    // and    Rm, Rn
    void ANDI(const DecodedArgs &args);   // and    imm, R0
    void ANDM(const DecodedArgs &args);   // and.   b imm, @(R0,GBR)
    void NEG(const DecodedArgs &args);    // neg    Rm, Rn
    void NEGC(const DecodedArgs &args);   // negc   Rm, Rn
    void NOT(const DecodedArgs &args);    // not    Rm, Rn
    void OR(const DecodedArgs &args);     // or     Rm, Rn
    void ORI(const DecodedArgs &args);    // or     imm, Rn
    void ORM(const DecodedArgs &args);    // or.b   imm, @(R0,GBR)
    void ROTCL(const DecodedArgs &args);  // rotcl  Rn
    void ROTCR(const DecodedArgs &args);  // rotcr  Rn
    void ROTL(const DecodedArgs &args);   // rotl   Rn
    void ROTR(const DecodedArgs &args);   // rotr   Rn
    void SHAL(const DecodedArgs &args);   // shal   Rn
    void SHAR(const DecodedArgs &args);   // shar   Rn
    void SHLL(const DecodedArgs &args);   // shll   Rn
    void SHLL2(const DecodedArgs &args);  // shll2  Rn
    void SHLL8(const DecodedArgs &args);  // shll8  Rn
    void SHLL16(const DecodedArgs &args); // shll16 Rn
    void SHLR(const DecodedArgs &args);   // shlr   Rn
    void SHLR2(const DecodedArgs &args);  // shlr2  Rn
    void SHLR8(const DecodedArgs &args);  // shlr8  Rn
    void SHLR16(const DecodedArgs &args); // shlr16 Rn
    void SUB(const DecodedArgs &args);    // sub    Rm, Rn
    void SUBC(const DecodedArgs &args);   // subc   Rm, Rn
    void SUBV(const DecodedArgs &args);   // subv   Rm, Rn
    void XOR(const DecodedArgs &args);    // xor    Rm, Rn
    void XORI(const DecodedArgs &args);   // xor    imm, Rn
    void XORM(const DecodedArgs &args);   // xor.b  imm, @(R0,GBR)

    void DT(const DecodedArgs &args); // dt Rn

    void CLRMAC();                       // clrmac
    void MACW(const DecodedArgs &args);  // mac.w   @Rm+, @Rn+
    void MACL(const DecodedArgs &args);  // mac.l   @Rm+, @Rn+
    void MULL(const DecodedArgs &args);  // mul.l   Rm, Rn
    void MULS(const DecodedArgs &args);  // muls.w  Rm, Rn
    void MULU(const DecodedArgs &args);  // mulu.w  Rm, Rn
    void DMULS(const DecodedArgs &args); // dmuls.l Rm, Rn
    void DMULU(const DecodedArgs &args); // dmulu.l Rm, Rn

    void DIV0S(const DecodedArgs &args); // div0s Rm, Rn
    void DIV0U();                        // div0u
    void DIV1(const DecodedArgs &args);  // div1  Rm, Rn

    void CMPIM(const DecodedArgs &args);  // cmp/eq  imm, R0
    void CMPEQ(const DecodedArgs &args);  // cmp/eq  Rm, Rn
    void CMPGE(const DecodedArgs &args);  // cmp/ge  Rm, Rn
    void CMPGT(const DecodedArgs &args);  // cmp/gt  Rm, Rn
    void CMPHI(const DecodedArgs &args);  // cmp/hi  Rm, Rn
    void CMPHS(const DecodedArgs &args);  // cmp/hs  Rm, Rn
    void CMPPL(const DecodedArgs &args);  // cmp/pl  Rn
    void CMPPZ(const DecodedArgs &args);  // cmp/pz  Rn
    void CMPSTR(const DecodedArgs &args); // cmp/str Rm, Rn
    void TAS(const DecodedArgs &args);    // tas.b   @Rn
    void TST(const DecodedArgs &args);    // tst     Rm, Rn
    void TSTI(const DecodedArgs &args);   // tst     imm, R0
    void TSTM(const DecodedArgs &args);   // tst.b   imm, @(R0,GBR)

    void BF(const DecodedArgs &args);    // bf    disp
    void BFS(const DecodedArgs &args);   // bf/s  disp
    void BT(const DecodedArgs &args);    // bt    disp
    void BTS(const DecodedArgs &args);   // bt/s  disp
    void BRA(const DecodedArgs &args);   // bra   disp
    void BRAF(const DecodedArgs &args);  // braf  Rm
    void BSR(const DecodedArgs &args);   // bsr   disp
    void BSRF(const DecodedArgs &args);  // bsrf  Rm
    void JMP(const DecodedArgs &args);   // jmp   @Rm
    void JSR(const DecodedArgs &args);   // jsr   @Rm
    void TRAPA(const DecodedArgs &args); // trapa imm

    void RTE(); // rte
    void RTS(); // rts
};

} // namespace satemu::sh2
