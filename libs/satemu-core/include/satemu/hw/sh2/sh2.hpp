#pragma once

#include "sh2_decode.hpp"
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

    union RegMAC {
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

    // --- SCI module ---

    // --- FRT module ---

    // --- INTC module ---

    RegIPRB IPRB;     // 060  R/W  8,16     0000      IPRB    Interrupt priority setting register B
    RegVCRA VCRA;     // 062  R/W  8,16     0000      VCRA    Vector number setting register A
    RegVCRB VCRB;     // 064  R/W  8,16     0000      VCRB    Vector number setting register B
    RegVCRC VCRC;     // 066  R/W  8,16     0000      VCRC    Vector number setting register C
    RegVCRD VCRD;     // 068  R/W  8,16     0000      VCRD    Vector number setting register D
    RegICR ICR;       // 0E0  R/W  8,16     0000      ICR     Interrupt control register
    RegIPRA IPRA;     // 0E2  R/W  8,16     0000      IPRA    Interrupt priority setting register A
    RegVCRWDT VCRWDT; // 0E4  R/W  8,16     0000      VCRWDT  Vector number setting register WDT

    // --- DMAC module ---

    std::array<DMAChannel, 2> dmaChannels;
    RegDMAOR DMAOR;

    // Determines if a DMA transfer is active for the specified channel.
    // A transfer is active if DE = 1, DME = 1, TE = 0, NMIF = 0 and AE = 0.
    bool IsDMATransferActive(const DMAChannel &ch) const;

    // --- WDT module ---

    // --- Power-down module ---

    // --- Cache module ---

    alignas(16) std::array<CacheEntry, kCacheEntries> cacheEntries;

    RegCCR CCR; // 092  R/W  8        00        CCR     Cache Control Register

    void WriteCCR(uint8 value);

    // --- DIVU module ---

    RegDVSR DVSR;     // 100  R/W  32       ud        DVSR    Divisor register
    RegDVDNT DVDNT;   // 104  R/W  32       ud        DVDNT   Dividend register L for 32-bit division
    RegDVCR DVCR;     // 108  R/W  16,32    00000000  DVCR    Division control register
    RegVCRDIV VCRDIV; // 10C  R/W  16,32    ud        VCRDIV  Vector number register setting DIV
    RegDVDNTH DVDNTH; // 110  R/W  32       ud        DVDNTH  Dividend register H
    RegDVDNTL DVDNTL; // 114  R/W  32       ud        DVDNTL  Dividend register L
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

    // --- BSC module ---

    RegBCR1 BCR1;   // 1E0  R/W  16,32    03F0      BCR1    Bus Control Register 1
    RegBCR2 BCR2;   // 1E4  R/W  16,32    00FC      BCR2    Bus Control Register 2
    RegWCR WCR;     // 1E8  R/W  16,32    AAFF      WCR     Wait Control Register
    RegMCR MCR;     // 1EC  R/W  16,32    0000      MCR     Individual Memory Control Register
    RegRTCSR RTCSR; // 1F0  R/W  16,32    0000      RTCSR   Refresh Timer Control/Status Register
    RegRTCNT RTCNT; // 1F4  R/W  16,32    0000      RTCNT   Refresh Timer Counter
    RegRTCOR RTCOR; // 1F8  R/W  16,32    0000      RTCOR   Refresh Timer Constant Register

    template <mem_primitive T>
    T OnChipRegRead(uint32 address);

    template <mem_primitive T>
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

    void MOV(InstrNM instr);     // mov   Rm, Rn
    void MOVBL(InstrNM instr);   // mov.b @Rm, Rn
    void MOVWL(InstrNM instr);   // mov.w @Rm, Rn
    void MOVLL(InstrNM instr);   // mov.l @Rm, Rn
    void MOVBL0(InstrNM instr);  // mov.b @(R0,Rm), Rn
    void MOVWL0(InstrNM instr);  // mov.w @(R0,Rm), Rn
    void MOVLL0(InstrNM instr);  // mov.l @(R0,Rm), Rn
    void MOVBL4(InstrMD instr);  // mov.b @(disp,Rm), R0
    void MOVWL4(InstrMD instr);  // mov.w @(disp,Rm), R0
    void MOVLL4(InstrNMD instr); // mov.l @(disp,Rm), Rn
    void MOVBLG(InstrD instr);   // mov.b @(disp,GBR), R0
    void MOVWLG(InstrD instr);   // mov.w @(disp,GBR), R0
    void MOVLLG(InstrD instr);   // mov.l @(disp,GBR), R0
    void MOVBM(InstrNM instr);   // mov.b Rm, @-Rn
    void MOVWM(InstrNM instr);   // mov.w Rm, @-Rn
    void MOVLM(InstrNM instr);   // mov.l Rm, @-Rn
    void MOVBP(InstrNM instr);   // mov.b @Rm+, Rn
    void MOVWP(InstrNM instr);   // mov.w @Rm+, Rn
    void MOVLP(InstrNM instr);   // mov.l @Rm+, Rn
    void MOVBS(InstrNM instr);   // mov.b Rm, @Rn
    void MOVWS(InstrNM instr);   // mov.w Rm, @Rn
    void MOVLS(InstrNM instr);   // mov.l Rm, @Rn
    void MOVBS0(InstrNM instr);  // mov.b Rm, @(R0,Rn)
    void MOVWS0(InstrNM instr);  // mov.w Rm, @(R0,Rn)
    void MOVLS0(InstrNM instr);  // mov.l Rm, @(R0,Rn)
    void MOVBS4(InstrND4 instr); // mov.b R0, @(disp,Rn)
    void MOVWS4(InstrND4 instr); // mov.w R0, @(disp,Rn)
    void MOVLS4(InstrNMD instr); // mov.l Rm, @(disp,Rn)
    void MOVBSG(InstrD instr);   // mov.b R0, @(disp,GBR)
    void MOVWSG(InstrD instr);   // mov.w R0, @(disp,GBR)
    void MOVLSG(InstrD instr);   // mov.l R0, @(disp,GBR)
    void MOVI(InstrNI instr);    // mov   #imm, Rn
    void MOVWI(InstrND8 instr);  // mov.w @(disp,PC), Rn
    void MOVLI(InstrND8 instr);  // mov.l @(disp,PC), Rn
    void MOVA(InstrD instr);     // mova  @(disp,PC), R0
    void MOVT(InstrN instr);     // movt  Rn
    void CLRT();                 // clrt
    void SETT();                 // sett

    void EXTSB(InstrNM instr); // exts.b Rm, Rn
    void EXTSW(InstrNM instr); // exts.w Rm, Rn
    void EXTUB(InstrNM instr); // extu.b Rm, Rn
    void EXTUW(InstrNM instr); // extu.w Rm, Rn
    void SWAPB(InstrNM instr); // swap.b Rm, Rn
    void SWAPW(InstrNM instr); // swap.w Rm, Rn
    void XTRCT(InstrNM instr); // xtrct  Rm, Rn

    void LDCGBR(InstrM instr);   // ldc   Rm, GBR
    void LDCSR(InstrM instr);    // ldc   Rm, SR
    void LDCVBR(InstrM instr);   // ldc   Rm, VBR
    void LDCMGBR(InstrM instr);  // ldc.l @Rm+, GBR
    void LDCMSR(InstrM instr);   // ldc.l @Rm+, SR
    void LDCMVBR(InstrM instr);  // ldc.l @Rm+, VBR
    void LDSMACH(InstrM instr);  // lds   Rm, MACH
    void LDSMACL(InstrM instr);  // lds   Rm, MACL
    void LDSPR(InstrM instr);    // lds   Rm, PR
    void LDSMMACH(InstrM instr); // lds.l @Rm+, MACH
    void LDSMMACL(InstrM instr); // lds.l @Rm+, MACL
    void LDSMPR(InstrM instr);   // lds.l @Rm+, PR
    void STCGBR(InstrN instr);   // stc   GBR, Rn
    void STCSR(InstrN instr);    // stc   SR, Rn
    void STCVBR(InstrN instr);   // stc   VBR, Rn
    void STCMGBR(InstrN instr);  // stc.l GBR, @-Rn
    void STCMSR(InstrN instr);   // stc.l SR, @-Rn
    void STCMVBR(InstrN instr);  // stc.l VBR, @-Rn
    void STSMACH(InstrN instr);  // sts   MACH, Rn
    void STSMACL(InstrN instr);  // sts   MACL, Rn
    void STSPR(InstrN instr);    // sts   PR, Rn
    void STSMMACH(InstrN instr); // sts.l MACH, @-Rn
    void STSMMACL(InstrN instr); // sts.l MACL, @-Rn
    void STSMPR(InstrN instr);   // sts.l PR, @-Rn

    void ADD(InstrNM instr);   // add    Rm, Rn
    void ADDI(InstrNI instr);  // add    imm, Rn
    void ADDC(InstrNM instr);  // addc   Rm, Rn
    void ADDV(InstrNM instr);  // addv   Rm, Rn
    void AND(InstrNM instr);   // and    Rm, Rn
    void ANDI(InstrI instr);   // and    imm, R0
    void ANDM(InstrI instr);   // and.   b imm, @(R0,GBR)
    void NEG(InstrNM instr);   // neg    Rm, Rn
    void NEGC(InstrNM instr);  // negc   Rm, Rn
    void NOT(InstrNM instr);   // not    Rm, Rn
    void OR(InstrNM instr);    // or     Rm, Rn
    void ORI(InstrI instr);    // or     imm, Rn
    void ORM(InstrI instr);    // or.b   imm, @(R0,GBR)
    void ROTCL(InstrN instr);  // rotcl  Rn
    void ROTCR(InstrN instr);  // rotcr  Rn
    void ROTL(InstrN instr);   // rotl   Rn
    void ROTR(InstrN instr);   // rotr   Rn
    void SHAL(InstrN instr);   // shal   Rn
    void SHAR(InstrN instr);   // shar   Rn
    void SHLL(InstrN instr);   // shll   Rn
    void SHLL2(InstrN instr);  // shll2  Rn
    void SHLL8(InstrN instr);  // shll8  Rn
    void SHLL16(InstrN instr); // shll16 Rn
    void SHLR(InstrN instr);   // shlr   Rn
    void SHLR2(InstrN instr);  // shlr2  Rn
    void SHLR8(InstrN instr);  // shlr8  Rn
    void SHLR16(InstrN instr); // shlr16 Rn
    void SUB(InstrNM instr);   // sub    Rm, Rn
    void SUBC(InstrNM instr);  // subc   Rm, Rn
    void SUBV(InstrNM instr);  // subv   Rm, Rn
    void XOR(InstrNM instr);   // xor    Rm, Rn
    void XORI(InstrI instr);   // xor    imm, Rn
    void XORM(InstrI instr);   // xor.b  imm, @(R0,GBR)

    void DT(InstrN instr); // dt Rn

    void CLRMAC();             // clrmac
    void MACW(InstrNM instr);  // mac.w   @Rm+, @Rn+
    void MACL(InstrNM instr);  // mac.l   @Rm+, @Rn+
    void MULL(InstrNM instr);  // mul.l   Rm, Rn
    void MULS(InstrNM instr);  // muls.w  Rm, Rn
    void MULU(InstrNM instr);  // mulu.w  Rm, Rn
    void DMULS(InstrNM instr); // dmuls.l Rm, Rn
    void DMULU(InstrNM instr); // dmulu.l Rm, Rn

    void DIV0S(InstrNM instr); // div0s Rm, Rn
    void DIV0U();              // div0u
    void DIV1(InstrNM instr);  // div1  Rm, Rn

    void CMPIM(InstrI instr);   // cmp/eq  imm, R0
    void CMPEQ(InstrNM instr);  // cmp/eq  Rm, Rn
    void CMPGE(InstrNM instr);  // cmp/ge  Rm, Rn
    void CMPGT(InstrNM instr);  // cmp/gt  Rm, Rn
    void CMPHI(InstrNM instr);  // cmp/hi  Rm, Rn
    void CMPHS(InstrNM instr);  // cmp/hs  Rm, Rn
    void CMPPL(InstrN instr);   // cmp/pl  Rn
    void CMPPZ(InstrN instr);   // cmp/pz  Rn
    void CMPSTR(InstrNM instr); // cmp/str Rm, Rn
    void TAS(InstrN instr);     // tas.b   @Rn
    void TST(InstrNM instr);    // tst     Rm, Rn
    void TSTI(InstrI instr);    // tst     imm, R0
    void TSTM(InstrI instr);    // tst.b   imm, @(R0,GBR)

    void BF(InstrD instr);    // bf    disp
    void BFS(InstrD instr);   // bf/s  disp
    void BT(InstrD instr);    // bt    disp
    void BTS(InstrD instr);   // bt/s  disp
    void BRA(InstrD12 instr); // bra   disp
    void BRAF(InstrM instr);  // braf  Rm
    void BSR(InstrD12 instr); // bsr   disp
    void BSRF(InstrM instr);  // bsrf  Rm
    void JMP(InstrM instr);   // jmp   @Rm
    void JSR(InstrM instr);   // jsr   @Rm
    void TRAPA(InstrI instr); // trapa imm

    void RTE(); // rte
    void RTS(); // rts
};

} // namespace satemu::sh2
