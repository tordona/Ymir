#pragma once

#include "sh2_defs.hpp"

#include <satemu/core_types.hpp>

#include <satemu/hw/hw_defs.hpp>

#include <fmt/format.h>

#include <array>

// -----------------------------------------------------------------------------
// Forward declarations

namespace satemu::sh2 {

class SH2Bus;

} // namespace satemu::sh2

// -----------------------------------------------------------------------------

namespace satemu::sh2 {

class SH2 {
public:
    SH2(SH2Bus &bus, bool master);

    void Reset(bool hard);

    void Step();

    void SetExternalInterrupt(uint8 level, uint8 vecNum);

private:
    // -------------------------------------------------------------------------
    // CPU state

    // R0 through R15.
    // R15 is also used as the hardware stack pointer (SP).
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

    template <mem_access_type T, bool instrFetch>
    T MemRead(uint32 address);

    template <mem_access_type T>
    void MemWrite(uint32 address, T value);

    uint16 FetchInstruction(uint32 address);

    uint8 MemReadByte(uint32 address);
    uint16 MemReadWord(uint32 address);
    uint32 MemReadLong(uint32 address);

    void MemWriteByte(uint32 address, uint8 value);
    void MemWriteWord(uint32 address, uint16 value);
    void MemWriteLong(uint32 address, uint32 value);

    // Returns 00 00 00 01 00 02 00 03 00 04 00 05 00 06 00 07 ... repeating
    template <mem_access_type T>
    T OpenBusSeqRead(uint32 address);

    // -------------------------------------------------------------------------
    // On-chip peripherals

    // --- SCI module ---

    // --- FRT module ---

    // --- INTC module ---

    IPRB_t IPRB;     // 060  R/W  8,16     0000      IPRB    Interrupt priority setting register B
    VCRA_t VCRA;     // 062  R/W  8,16     0000      VCRA    Vector number setting register A
    VCRB_t VCRB;     // 064  R/W  8,16     0000      VCRB    Vector number setting register B
    VCRC_t VCRC;     // 066  R/W  8,16     0000      VCRC    Vector number setting register C
    VCRD_t VCRD;     // 068  R/W  8,16     0000      VCRD    Vector number setting register D
    ICR_t ICR;       // 0E0  R/W  8,16     0000      ICR     Interrupt control register
    IPRA_t IPRA;     // 0E2  R/W  8,16     0000      IPRA    Interrupt priority setting register A
    VCRWDT_t VCRWDT; // 0E4  R/W  8,16     0000      VCRWDT  Vector number setting register WDT

    // --- DMAC module ---

    // --- WDT module ---

    // --- Power-down module ---

    // --- Cache module ---

    alignas(16) std::array<CacheEntry, kCacheEntries> cacheEntries;

    CCR_t CCR; // 092  R/W  8        00        CCR     Cache Control Register

    void WriteCCR(uint8 value);

    // --- DIVU module ---

    DVSR_t DVSR;     // 100  R/W  32       ud        DVSR    Divisor register
    DVDNT_t DVDNT;   // 104  R/W  32       ud        DVDNT   Dividend register L for 32-bit division
    DVCR_t DVCR;     // 108  R/W  16,32    00000000  DVCR    Division control register
    VCRDIV_t VCRDIV; // 10C  R/W  16,32    ud        VCRDIV  Vector number register setting DIV
    DVDNTH_t DVDNTH; // 110  R/W  32       ud        DVDNTH  Dividend register H
    DVDNTL_t DVDNTL; // 114  R/W  32       ud        DVDNTL  Dividend register L
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

    // --- DMAC module ---

    VCRDMA0_t VCRDMA0; // 1A0  R/W  32       ud        VCRDMA0 DMA vector number register 0
    VCRDMA1_t VCRDMA1; // 1A8  R/W  32       ud        VCRDMA1 DMA vector number register 1

    // --- BSC module ---

    BCR1_t BCR1;   // 1E0  R/W  16,32    03F0      BCR1    Bus Control Register 1
    BCR2_t BCR2;   // 1E4  R/W  16,32    00FC      BCR2    Bus Control Register 2
    WCR_t WCR;     // 1E8  R/W  16,32    AAFF      WCR     Wait Control Register
    MCR_t MCR;     // 1EC  R/W  16,32    0000      MCR     Individual Memory Control Register
    RTCSR_t RTCSR; // 1F0  R/W  16,32    0000      RTCSR   Refresh Timer Control/Status Register
    RTCNT_t RTCNT; // 1F4  R/W  16,32    0000      RTCNT   Refresh Timer Counter
    RTCOR_t RTCOR; // 1F8  R/W  16,32    0000      RTCOR   Refresh Timer Constant Register

    template <mem_access_type T>
    T OnChipRegRead(uint32 address);

    template <mem_access_type T>
    void OnChipRegWrite(uint32 address, T baseValue);

    // -------------------------------------------------------------------------
    // Interrupts

    uint8 m_pendingExternalIntrLevel;
    uint8 m_pendingExternalIntrVecNum;

    struct PendingInterruptInfo {
        uint8 priority;
        uint8 vecNum;
    } m_pendingInterrupt;

    bool CheckInterrupts();

    // -------------------------------------------------------------------------
    // Helper functions

    void SetupDelaySlot(uint32 targetAddress);
    void EnterException(uint8 vectorNumber);

    bool m_delaySlot;
    uint32 m_delaySlotTarget;

    // -------------------------------------------------------------------------
    // Interpreter

    void Execute(uint32 address);

    // -------------------------------------------------------------------------
    // Instruction interpreters

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
