#pragma once

#include "sh2_bus.hpp"
#include "sh2_state.hpp"

#include <satemu/util/unreachable.hpp>

#include <fmt/format.h>

#include <span>
#include <type_traits>

namespace satemu::sh2 {

class SH2 {
public:
    SH2(scu::SCU &scu, smpc::SMPC &smpc);

    void Reset(bool hard);

    void LoadIPL(std::span<uint8, kIPLSize> ipl);

    void Step();

    void SetExternalInterruptCallback(sh2::CBAcknowledgeExternalInterrupt callback);
    void SetExternalInterrupt(uint8 level, uint8 vecNum);

private:
    SH2State m_masterState{true};
    SH2State m_slaveState{false};
    SH2Bus m_bus;

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
    T MemRead(SH2State &state, uint32 address) {
        const uint32 partition = (address >> 29u) & 0b111;
        if (address & static_cast<uint32>(sizeof(T) - 1)) {
            fmt::println("WARNING: misaligned {}-bit read from {:08X}", sizeof(T) * 8, address);
            // TODO: address error (misaligned access)
            // - might have to store data in a class member instead of returning
        }

        switch (partition) {
        case 0b000: // cache
            if (state.CCR.CE) {
                // TODO: use cache
            }
            // fallthrough
        case 0b001:
        case 0b101: // cache-through
            return m_bus.Read<T>(address & 0x7FFFFFF);
        case 0b010: // associative purge

            // TODO: implement
            fmt::println("unhandled {}-bit SH-2 associative purge read from {:08X}", sizeof(T) * 8, address);
            return (address & 1) ? static_cast<T>(0x12231223) : static_cast<T>(0x23122312);
        case 0b011: { // cache address array
            uint32 entry = (address >> 4u) & 0x3F;
            return state.cacheEntries[entry].tag[state.CCR.Wn]; // TODO: include LRU data
        }
        case 0b100:
        case 0b110: // cache data array

            // TODO: implement
            fmt::println("unhandled {}-bit SH-2 cache data array read from {:08X}", sizeof(T) * 8, address);
            return 0;
        case 0b111: // I/O area
            if ((address & 0xE0004000) == 0xE0004000) {
                // bits 31-29 and 14 must be set
                // bits 8-0 index the register
                // bits 28 and 12 must be both set to access the lower half of the registers
                if ((address & 0x100) || (address & 0x10001000) == 0x10001000) {
                    return state.OnChipRegRead<T>(address & 0x1FF);
                } else {
                    return OpenBusSeqRead<T>(address);
                }
            } else {
                // TODO: implement
                fmt::println("unhandled {}-bit SH-2 I/O area read from {:08X}", sizeof(T) * 8, address);
                return 0;
            }
        }

        util::unreachable();
    }

    template <mem_access_type T>
    void MemWrite(SH2State &state, uint32 address, T value) {
        const uint32 partition = address >> 29u;
        if (address & static_cast<uint32>(sizeof(T) - 1)) {
            fmt::println("WARNING: misaligned {}-bit write to {:08X} = {:X}", sizeof(T) * 8, address, value);
            // TODO: address error (misaligned access)
        }

        switch (partition) {
        case 0b000: // cache
            if (state.CCR.CE) {
                // TODO: use cache
            }
            // fallthrough
        case 0b001:
        case 0b101: // cache-through
            m_bus.Write<T>(address & 0x7FFFFFF, value);
            break;
        case 0b010: // associative purge
            // TODO: implement
            fmt::println("unhandled {}-bit SH-2 associative purge write to {:08X} = {:X}", sizeof(T) * 8, address,
                         value);
            break;
        case 0b011: { // cache address array
            uint32 entry = (address >> 4u) & 0x3F;
            state.cacheEntries[entry].tag[state.CCR.Wn] = address & 0x1FFFFC04;
            // TODO: update LRU data
            break;
        }
        case 0b100:
        case 0b110: // cache data array
            // TODO: implement
            fmt::println("unhandled {}-bit SH-2 cache data array write to {:08X} = {:X}", sizeof(T) * 8, address,
                         value);
            break;
        case 0b111: // I/O area
            if ((address & 0xE0004000) == 0xE0004000) {
                // bits 31-29 and 14 must be set
                // bits 8-0 index the register
                // bits 28 and 12 must be both set to access the lower half of the registers
                if ((address & 0x100) || (address & 0x10001000) == 0x10001000) {
                    state.OnChipRegWrite<T>(address & 0x1FF, value);
                }
            } else if ((address >> 12u) == 0xFFFF8) {
                // DRAM setup stuff
                switch (address) {
                case 0xFFFF8426: fmt::println("16-bit CAS latency 1"); break;
                case 0xFFFF8446: fmt::println("16-bit CAS latency 2"); break;
                case 0xFFFF8466: fmt::println("16-bit CAS latency 3"); break;
                case 0xFFFF8848: fmt::println("32-bit CAS latency 1"); break;
                case 0xFFFF8888: fmt::println("32-bit CAS latency 2"); break;
                case 0xFFFF88C8: fmt::println("32-bit CAS latency 3"); break;
                default: fmt::println("unhandled {}-bit SH-2 I/O area read from {:08X}", sizeof(T) * 8, address); break;
                }
            } else {
                // TODO: implement
                fmt::println("unhandled {}-bit SH-2 I/O area write to {:08X} = {:X}", sizeof(T) * 8, address, value);
            }
            break;
        }
    }

    FLATTEN FORCE_INLINE uint8 MemReadByte(SH2State &state, uint32 address) {
        return MemRead<uint8>(state, address);
    }

    FLATTEN FORCE_INLINE uint16 MemReadWord(SH2State &state, uint32 address) {
        return MemRead<uint16>(state, address);
    }

    FLATTEN FORCE_INLINE uint32 MemReadLong(SH2State &state, uint32 address) {
        return MemRead<uint32>(state, address);
    }

    FLATTEN FORCE_INLINE void MemWriteByte(SH2State &state, uint32 address, uint8 value) {
        MemWrite<uint8>(state, address, value);
    }

    FLATTEN FORCE_INLINE void MemWriteWord(SH2State &state, uint32 address, uint16 value) {
        MemWrite<uint16>(state, address, value);
    }

    FLATTEN FORCE_INLINE void MemWriteLong(SH2State &state, uint32 address, uint32 value) {
        MemWrite<uint32>(state, address, value);
    }

    // Returns 00 00 00 01 00 02 00 03 00 04 00 05 00 06 00 07 ... repeating
    template <mem_access_type T>
    T OpenBusSeqRead(uint32 address) {
        if constexpr (std::is_same_v<T, uint8>) {
            return (address & 1u) * ((address >> 1u) & 0x7);
            // return OpenBusSeqRead<uint16>(address) >> (((address & 1) ^ 1) * 8);
        } else if constexpr (std::is_same_v<T, uint16>) {
            return (address >> 1u) & 0x7;
        } else if constexpr (std::is_same_v<T, uint32>) {
            return (OpenBusSeqRead<uint16>(address + 1) << 16u) | OpenBusSeqRead<uint16>(address);
        }
    }

    // -------------------------------------------------------------------------
    // Helper functions

    void EnterException(SH2State &state, uint8 vectorNumber);

    // -------------------------------------------------------------------------
    // Interpreter

    void Step(SH2State &state);

    template <bool delaySlot>
    void Execute(SH2State &state, uint32 address);

    // -------------------------------------------------------------------------
    // Instruction interpreters

    void NOP(); // nop

    void SLEEP(SH2State &state); // sleep

    void MOV(SH2State &state, uint16 rm, uint16 rn);                 // mov   Rm, Rn
    void MOVBL(SH2State &state, uint16 rm, uint16 rn);               // mov.b @Rm, Rn
    void MOVWL(SH2State &state, uint16 rm, uint16 rn);               // mov.w @Rm, Rn
    void MOVLL(SH2State &state, uint16 rm, uint16 rn);               // mov.l @Rm, Rn
    void MOVBL0(SH2State &state, uint16 rm, uint16 rn);              // mov.b @(R0,Rm), Rn
    void MOVWL0(SH2State &state, uint16 rm, uint16 rn);              // mov.w @(R0,Rm), Rn
    void MOVLL0(SH2State &state, uint16 rm, uint16 rn);              // mov.l @(R0,Rm), Rn
    void MOVBL4(SH2State &state, uint16 rm, uint16 disp);            // mov.b @(disp,Rm), R0
    void MOVWL4(SH2State &state, uint16 rm, uint16 disp);            // mov.w @(disp,Rm), R0
    void MOVLL4(SH2State &state, uint16 rm, uint16 disp, uint16 rn); // mov.l @(disp,Rm), Rn
    void MOVBLG(SH2State &state, uint16 disp);                       // mov.b @(disp,GBR), R0
    void MOVWLG(SH2State &state, uint16 disp);                       // mov.w @(disp,GBR), R0
    void MOVLLG(SH2State &state, uint16 disp);                       // mov.l @(disp,GBR), R0
    void MOVBM(SH2State &state, uint16 rm, uint16 rn);               // mov.b Rm, @-Rn
    void MOVWM(SH2State &state, uint16 rm, uint16 rn);               // mov.w Rm, @-Rn
    void MOVLM(SH2State &state, uint16 rm, uint16 rn);               // mov.l Rm, @-Rn
    void MOVBP(SH2State &state, uint16 rm, uint16 rn);               // mov.b @Rm+, Rn
    void MOVWP(SH2State &state, uint16 rm, uint16 rn);               // mov.w @Rm+, Rn
    void MOVLP(SH2State &state, uint16 rm, uint16 rn);               // mov.l @Rm+, Rn
    void MOVBS(SH2State &state, uint16 rm, uint16 rn);               // mov.b Rm, @Rn
    void MOVWS(SH2State &state, uint16 rm, uint16 rn);               // mov.w Rm, @Rn
    void MOVLS(SH2State &state, uint16 rm, uint16 rn);               // mov.l Rm, @Rn
    void MOVBS0(SH2State &state, uint16 rm, uint16 rn);              // mov.b Rm, @(R0,Rn)
    void MOVWS0(SH2State &state, uint16 rm, uint16 rn);              // mov.w Rm, @(R0,Rn)
    void MOVLS0(SH2State &state, uint16 rm, uint16 rn);              // mov.l Rm, @(R0,Rn)
    void MOVBS4(SH2State &state, uint16 disp, uint16 rn);            // mov.b R0, @(disp,Rn)
    void MOVWS4(SH2State &state, uint16 disp, uint16 rn);            // mov.w R0, @(disp,Rn)
    void MOVLS4(SH2State &state, uint16 rm, uint16 disp, uint16 rn); // mov.l Rm, @(disp,Rn)
    void MOVBSG(SH2State &state, uint16 disp);                       // mov.b R0, @(disp,GBR)
    void MOVWSG(SH2State &state, uint16 disp);                       // mov.w R0, @(disp,GBR)
    void MOVLSG(SH2State &state, uint16 disp);                       // mov.l R0, @(disp,GBR)
    void MOVI(SH2State &state, uint16 imm, uint16 rn);               // mov   #imm, Rn
    void MOVWI(SH2State &state, uint16 disp, uint16 rn);             // mov.w @(disp,PC), Rn
    void MOVLI(SH2State &state, uint16 disp, uint16 rn);             // mov.l @(disp,PC), Rn
    void MOVA(SH2State &state, uint16 disp);                         // mova  @(disp,PC), R0
    void MOVT(SH2State &state, uint16 rn);                           // movt  Rn
    void CLRT(SH2State &state);                                      // clrt
    void SETT(SH2State &state);                                      // sett

    void EXTSB(SH2State &state, uint16 rm, uint16 rn); // exts.b Rm, Rn
    void EXTSW(SH2State &state, uint16 rm, uint16 rn); // exts.w Rm, Rn
    void EXTUB(SH2State &state, uint16 rm, uint16 rn); // extu.b Rm, Rn
    void EXTUW(SH2State &state, uint16 rm, uint16 rn); // extu.w Rm, Rn

    void LDCGBR(SH2State &state, uint16 rm);   // ldc   Rm, GBR
    void LDCSR(SH2State &state, uint16 rm);    // ldc   Rm, SR
    void LDCVBR(SH2State &state, uint16 rm);   // ldc   Rm, VBR
    void LDCMGBR(SH2State &state, uint16 rm);  // ldc.l @Rm+, GBR
    void LDCMSR(SH2State &state, uint16 rm);   // ldc.l @Rm+, SR
    void LDCMVBR(SH2State &state, uint16 rm);  // ldc.l @Rm+, VBR
    void LDSMACH(SH2State &state, uint16 rm);  // lds   Rm, MACH
    void LDSMACL(SH2State &state, uint16 rm);  // lds   Rm, MACL
    void LDSPR(SH2State &state, uint16 rm);    // lds   Rm, PR
    void LDSMMACH(SH2State &state, uint16 rm); // lds.l @Rm+, MACH
    void LDSMMACL(SH2State &state, uint16 rm); // lds.l @Rm+, MACL
    void LDSMPR(SH2State &state, uint16 rm);   // lds.l @Rm+, PR
    void STCGBR(SH2State &state, uint16 rn);   // stc   GBR, Rn
    void STCSR(SH2State &state, uint16 rn);    // stc   SR, Rn
    void STCVBR(SH2State &state, uint16 rn);   // stc   VBR, Rn
    void STCMGBR(SH2State &state, uint16 rn);  // stc.l GBR, @-Rn
    void STCMSR(SH2State &state, uint16 rn);   // stc.l SR, @-Rn
    void STCMVBR(SH2State &state, uint16 rn);  // stc.l VBR, @-Rn
    void STSMACH(SH2State &state, uint16 rn);  // sts   MACH, Rn
    void STSMACL(SH2State &state, uint16 rn);  // sts   MACL, Rn
    void STSPR(SH2State &state, uint16 rn);    // sts   PR, Rn
    void STSMMACH(SH2State &state, uint16 rn); // sts.l MACH, @-Rn
    void STSMMACL(SH2State &state, uint16 rn); // sts.l MACL, @-Rn
    void STSMPR(SH2State &state, uint16 rn);   // sts.l PR, @-Rn

    void SWAPB(SH2State &state, uint16 rm, uint16 rn); // swap.b Rm, Rn
    void SWAPW(SH2State &state, uint16 rm, uint16 rn); // swap.w Rm, Rn
    void XTRCT(SH2State &state, uint16 rm, uint16 rn); // xtrct  Rm, Rn

    void ADD(SH2State &state, uint16 rm, uint16 rn);   // add    Rm, Rn
    void ADDI(SH2State &state, uint16 imm, uint16 rn); // add    imm, Rn
    void ADDC(SH2State &state, uint16 rm, uint16 rn);  // addc   Rm, Rn
    void ADDV(SH2State &state, uint16 rm, uint16 rn);  // addv   Rm, Rn
    void AND(SH2State &state, uint16 rm, uint16 rn);   // and    Rm, Rn
    void ANDI(SH2State &state, uint16 imm);            // and    imm, R0
    void ANDM(SH2State &state, uint16 imm);            // and.   b imm, @(R0,GBR)
    void NEG(SH2State &state, uint16 rm, uint16 rn);   // neg    Rm, Rn
    void NEGC(SH2State &state, uint16 rm, uint16 rn);  // negc   Rm, Rn
    void NOT(SH2State &state, uint16 rm, uint16 rn);   // not    Rm, Rn
    void OR(SH2State &state, uint16 rm, uint16 rn);    // or     Rm, Rn
    void ORI(SH2State &state, uint16 imm);             // or     imm, Rn
    void ORM(SH2State &state, uint16 imm);             // or.b   imm, @(R0,GBR)
    void ROTCL(SH2State &state, uint16 rn);            // rotcl  Rn
    void ROTCR(SH2State &state, uint16 rn);            // rotcr  Rn
    void ROTL(SH2State &state, uint16 rn);             // rotl   Rn
    void ROTR(SH2State &state, uint16 rn);             // rotr   Rn
    void SHAL(SH2State &state, uint16 rn);             // shal   Rn
    void SHAR(SH2State &state, uint16 rn);             // shar   Rn
    void SHLL(SH2State &state, uint16 rn);             // shll   Rn
    void SHLL2(SH2State &state, uint16 rn);            // shll2  Rn
    void SHLL8(SH2State &state, uint16 rn);            // shll8  Rn
    void SHLL16(SH2State &state, uint16 rn);           // shll16 Rn
    void SHLR(SH2State &state, uint16 rn);             // shlr   Rn
    void SHLR2(SH2State &state, uint16 rn);            // shlr2  Rn
    void SHLR8(SH2State &state, uint16 rn);            // shlr8  Rn
    void SHLR16(SH2State &state, uint16 rn);           // shlr16 Rn
    void SUB(SH2State &state, uint16 rm, uint16 rn);   // sub    Rm, Rn
    void SUBC(SH2State &state, uint16 rm, uint16 rn);  // subc   Rm, Rn
    void SUBV(SH2State &state, uint16 rm, uint16 rn);  // subv   Rm, Rn
    void XOR(SH2State &state, uint16 rm, uint16 rn);   // xor    Rm, Rn
    void XORI(SH2State &state, uint16 imm);            // xor    imm, Rn
    void XORM(SH2State &state, uint16 imm);            // xor.b  imm, @(R0,GBR)

    void DT(SH2State &state, uint16 rn); // dt Rn

    void CLRMAC(SH2State &state);                      // clrmac
    void MACW(SH2State &state, uint16 rm, uint16 rn);  // mac.w   @Rm+, @Rn+
    void MACL(SH2State &state, uint16 rm, uint16 rn);  // mac.l   @Rm+, @Rn+
    void MULL(SH2State &state, uint16 rm, uint16 rn);  // mul.l   Rm, Rn
    void MULS(SH2State &state, uint16 rm, uint16 rn);  // muls.w  Rm, Rn
    void MULU(SH2State &state, uint16 rm, uint16 rn);  // mulu.w  Rm, Rn
    void DMULS(SH2State &state, uint16 rm, uint16 rn); // dmuls.l Rm, Rn
    void DMULU(SH2State &state, uint16 rm, uint16 rn); // dmulu.l Rm, Rn

    void DIV0S(SH2State &state, uint16 rm, uint16 rn); // div0s Rm, Rn
    void DIV0U(SH2State &state);                       // div0u
    void DIV1(SH2State &state, uint16 rm, uint16 rn);  // div1  Rm, Rn

    void CMPIM(SH2State &state, uint16 imm);            // cmp/eq  imm, R0
    void CMPEQ(SH2State &state, uint16 rm, uint16 rn);  // cmp/eq  Rm, Rn
    void CMPGE(SH2State &state, uint16 rm, uint16 rn);  // cmp/ge  Rm, Rn
    void CMPGT(SH2State &state, uint16 rm, uint16 rn);  // cmp/gt  Rm, Rn
    void CMPHI(SH2State &state, uint16 rm, uint16 rn);  // cmp/hi  Rm, Rn
    void CMPHS(SH2State &state, uint16 rm, uint16 rn);  // cmp/hs  Rm, Rn
    void CMPPL(SH2State &state, uint16 rn);             // cmp/pl  Rn
    void CMPPZ(SH2State &state, uint16 rn);             // cmp/pz  Rn
    void CMPSTR(SH2State &state, uint16 rm, uint16 rn); // cmp/str Rm, Rn
    void TAS(SH2State &state, uint16 rn);               // tas.b   @Rn
    void TST(SH2State &state, uint16 rm, uint16 rn);    // tst     Rm, Rn
    void TSTI(SH2State &state, uint16 imm);             // tst     imm, R0
    void TSTM(SH2State &state, uint16 imm);             // tst.b   imm, @(R0,GBR)

    void BF(SH2State &state, uint16 disp);   // bf    disp
    void BFS(SH2State &state, uint16 disp);  // bf/s  disp
    void BT(SH2State &state, uint16 disp);   // bt    disp
    void BTS(SH2State &state, uint16 disp);  // bt/s  disp
    void BRA(SH2State &state, uint16 disp);  // bra   disp
    void BRAF(SH2State &state, uint16 rm);   // braf  Rm
    void BSR(SH2State &state, uint16 disp);  // bsr   disp
    void BSRF(SH2State &state, uint16 rm);   // bsrf  Rm
    void JMP(SH2State &state, uint16 rm);    // jmp   @Rm
    void JSR(SH2State &state, uint16 rm);    // jsr   @Rm
    void TRAPA(SH2State &state, uint16 imm); // trapa imm

    void RTE(SH2State &state); // rte
    void RTS(SH2State &state); // rts
};

} // namespace satemu::sh2
