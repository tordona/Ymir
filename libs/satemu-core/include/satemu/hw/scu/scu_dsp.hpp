#pragma once

#include <satemu/core/types.hpp>

#include <satemu/sys/bus.hpp>

#include <satemu/util/bit_ops.hpp>
#include <satemu/util/callback.hpp>
#include <satemu/util/debug_print.hpp>
#include <satemu/util/inline.hpp>

#include <array>

namespace satemu::scu {

using CBTriggerDSPEnd = util::RequiredCallback<void()>;

class SCUDSP {
    static constexpr dbg::Category dspLog{"SCU-DSP"};

public:
    SCUDSP(sys::Bus &bus);

    void Reset(bool hard);

    void SetTriggerDSPEndCallback(CBTriggerDSPEnd callback) {
        m_cbTriggerDSPEnd = callback;
    }

    uint32 ReadProgram() {
        return programRAM[PC];
    }

    template <bool poke>
    void WriteProgram(uint32 value) {
        if constexpr (!poke) {
            // Cannot write while program is executing
            if (programExecuting) {
                return;
            }
        }

        programRAM[PC++] = value;
    }

    template <bool peek>
    uint32 ReadData() {
        if constexpr (!peek) {
            // Cannot read while program is executing
            if (programExecuting) {
                return 0;
            }
        }

        const uint8 bank = bit::extract<6, 7>(dataAddress);
        const uint8 offset = bit::extract<0, 5>(dataAddress);
        if constexpr (!peek) {
            dataAddress++;
        }
        return dataRAM[bank][offset];
    }

    template <bool poke>
    void WriteData(uint32 value) {
        if constexpr (!poke) {
            // Cannot write while program is executing
            if (programExecuting) {
                return;
            }
        }

        const uint8 bank = bit::extract<6, 7>(dataAddress);
        const uint8 offset = bit::extract<0, 5>(dataAddress);
        dataAddress++;
        dataRAM[bank][offset] = value;
    }

    void Run(uint64 cycles);
    void RunDMA(uint64 cycles);

    std::array<uint32, 256> programRAM;
    std::array<std::array<uint32, 64>, 4> dataRAM;

    bool programExecuting;
    bool programPaused;
    bool programEnded;
    bool programStep;

    uint8 PC; // program address
    uint8 dataAddress;

    uint32 nextPC;    // jump target
    uint8 jmpCounter; // when it reaches zero, perform the jump

    bool sign;
    bool zero;
    bool carry;
    bool overflow;

    // DSP data address
    std::array<uint8, 4> CT;
    std::array<bool, 4> incCT; // whether CT must be incremented after this iteration

    union Reg48 {
        uint64 u64 : 48;
        sint64 s64 : 48;
        struct {
            uint32 L;
            uint16 H;
        };
    };

    Reg48 ALU; // ALU operation output
    Reg48 AC;  // ALU operation input 1
    Reg48 P;   // ALU operation input 2 / Multiplication output
    sint32 RX; // Multiplication input 1
    sint32 RY; // Multiplication input 2

    uint8 loopTop;    // TOP
    uint16 loopCount; // LOP

    bool dmaRun;         // DMA transfer in progress (T0)
    bool dmaToD0;        // DMA transfer direction: false=D0 to DSP, true=DSP to D0
    bool dmaHold;        // DMA transfer hold address
    uint8 dmaCount;      // DMA transfer length
    uint8 dmaSrc;        // DMA source register (CT0-3 or program RAM)
    uint8 dmaDst;        // DMA destination register (CT0-3 or program RAM)
    uint32 dmaReadAddr;  // DMA read address (RA0)
    uint32 dmaWriteAddr; // DMA write address (WA0)
    uint32 dmaAddrInc;   // DMA address increment

    FORCE_INLINE void ALU_AND() {
        ALU.L = AC.L & P.L;
        zero = ALU.L == 0;
        sign = static_cast<sint32>(ALU.L) < 0;
        carry = false;
    }

    FORCE_INLINE void ALU_OR() {
        ALU.L = AC.L | P.L;
        zero = ALU.L == 0;
        sign = static_cast<sint32>(ALU.L) < 0;
        carry = false;
    }

    FORCE_INLINE void ALU_XOR() {
        ALU.L = AC.L ^ P.L;
        zero = ALU.L == 0;
        sign = static_cast<sint32>(ALU.L) < 0;
        carry = false;
    }

    FORCE_INLINE void ALU_ADD() {
        const uint64 op1 = AC.L;
        const uint64 op2 = P.L;
        const uint64 result = op1 + op2;
        ALU.L = result;
        zero = ALU.L == 0;
        sign = static_cast<sint32>(result) < 0;
        carry = bit::extract<32>(result);
        overflow = bit::extract<31>((~(op1 ^ op2)) & (op1 ^ result));
    }

    FORCE_INLINE void ALU_SUB() {
        const uint64 op1 = AC.L;
        const uint64 op2 = P.L;
        const uint64 result = op1 - op2;
        ALU.L = result;
        zero = ALU.L == 0;
        sign = static_cast<sint32>(result) < 0;
        carry = bit::extract<32>(result);
        overflow = bit::extract<31>((op1 ^ op2) & (op1 ^ result));
    }

    FORCE_INLINE void ALU_AD2() {
        const uint64 op1 = AC.u64;
        const uint64 op2 = P.u64;
        const uint64 result = op1 + op2;
        zero = (result << 16ull) == 0;
        sign = static_cast<sint64>(result << 16ull) < 0;
        carry = bit::extract<48>(result);
        overflow = bit::extract<47>((~(op1 ^ op2)) & (op1 ^ result));
        ALU.s64 = result;
    }

    FORCE_INLINE void ALU_SR() {
        carry = bit::extract<0>(AC.L);
        ALU.L = static_cast<sint32>(AC.L) >> 1;
        zero = ALU.L == 0;
        sign = false;
    }

    FORCE_INLINE void ALU_RR() {
        carry = bit::extract<0>(AC.L);
        ALU.L = std::rotr(AC.L, 1);
        zero = ALU.L == 0;
        sign = static_cast<sint32>(ALU.L) < 0;
    }

    FORCE_INLINE void ALU_SL() {
        carry = bit::extract<31>(AC.L);
        ALU.L = AC.L << 1u;
        zero = ALU.L == 0;
        sign = static_cast<sint32>(ALU.L) < 0;
    }

    FORCE_INLINE void ALU_RL() {
        carry = bit::extract<31>(AC.L);
        ALU.L = std::rotl(AC.L, 1);
        zero = ALU.L == 0;
        sign = static_cast<sint32>(ALU.L) < 0;
    }

    FORCE_INLINE void ALU_RL8() {
        carry = bit::extract<24>(AC.L);
        ALU.L = std::rotl(AC.L, 8);
        zero = ALU.L == 0;
        sign = static_cast<sint32>(ALU.L) < 0;
    }

    // X-Bus, Y-Bus and D1-Bus reads from [s]
    FORCE_INLINE uint32 ReadSource(uint8 index) {
        switch (index) {
        case 0b0000 ... 0b0111: {
            const uint8 ctIndex = bit::extract<0, 1>(index);
            const bool inc = bit::extract<2>(index);

            // Finish previous DMA transfer
            if (dmaRun) {
                RunDMA(1); // TODO: cycles?
            }

            incCT[ctIndex] |= inc;
            const uint32 ctAddr = CT[ctIndex];
            return dataRAM[ctIndex][ctAddr];
        }
        case 0b1001: return ALU.L;
        case 0b1010: return ALU.u64 >> 16ull;
        default: return ~0;
        }
    }

    // D1-Bus writes to [d]
    FORCE_INLINE void WriteD1Bus(uint8 index, uint32 value) {
        // Finish previous DMA transfer
        if (dmaRun) {
            RunDMA(1); // TODO: cycles?
        }

        switch (index) {
        case 0b0000 ... 0b0011: {
            const uint32 addr = CT[index];
            dataRAM[index][addr] = value;
            incCT[index] = true;
            break;
        }
        case 0b0100: RX = value; break;
        case 0b0101: P.s64 = static_cast<sint32>(value); break;
        case 0b0110: dmaReadAddr = (value << 2u) & 0x7FF'FFFC; break;
        case 0b0111: dmaWriteAddr = (value << 2u) & 0x7FF'FFFC; break;
        case 0b1010: loopCount = value & 0xFFF; break;
        case 0b1011: loopTop = value; break;
        case 0b1100 ... 0b1111:
            CT[index & 3] = value & 0x3F;
            incCT[index & 3] = false;
            break;
        }
    }

    // Immediate writes to [d]
    FORCE_INLINE void WriteImm(uint8 index, uint32 value) {
        // Finish previous DMA transfer
        if (dmaRun) {
            RunDMA(1); // TODO: cycles?
        }

        switch (index) {
        case 0b0000 ... 0b0011: {
            const uint32 addr = CT[index];
            dataRAM[index][addr] = value;
            incCT[index] = true;
            break;
        }
        case 0b0100: RX = value; break;
        case 0b0101: P.s64 = static_cast<sint32>(value); break;
        case 0b0110: dmaReadAddr = (value << 2u) & 0x7FF'FFFC; break;
        case 0b0111: dmaWriteAddr = (value << 2u) & 0x7FF'FFFC; break;
        case 0b1010: loopCount = value & 0xFFF; break;
        case 0b1100:
            loopTop = PC;
            DelayedJump(value);
            break;
        }
    }

    // Checks if the current DSP flags pass the given condition
    FORCE_INLINE bool CondCheck(uint8 cond) const {
        // 000001: NZ  (Z=0)
        // 000010: NS  (S=0)
        // 000011: NZS (Z=0 && S=0)
        // 000100: NC  (C=0)
        // 001000: NT0 (T0=0)
        // 100001: Z   (Z=1)
        // 100010: S   (S=1)
        // 100011: ZS  (Z=1 || S=1)
        // 100100: C   (C=1)
        // 101000: T0  (T0=1)
        switch (cond) {
        case 0b000001: return !zero;
        case 0b000010: return !sign;
        case 0b000011: return !zero && !sign;
        case 0b000100: return !carry;
        case 0b001000: return !dmaRun;

        case 0b100001: return zero;
        case 0b100010: return sign;
        case 0b100011: return zero || sign;
        case 0b100100: return carry;
        case 0b101000: return dmaRun;
        }
        return false;
    }

    // Prepares a delayed jump to the given target address
    FORCE_INLINE void DelayedJump(uint8 target) {
        nextPC = target & 0xFF;
        jmpCounter = 2;
    }

private:
    sys::Bus &m_bus;

    CBTriggerDSPEnd m_cbTriggerDSPEnd;

    // Command interpreters

    void Cmd_Operation(uint32 command);
    void Cmd_LoadImm(uint32 command);
    void Cmd_Special(uint32 command);
    void Cmd_Special_DMA(uint32 command);
    void Cmd_Special_Jump(uint32 command);
    void Cmd_Special_LoopBottom(uint32 command);
    void Cmd_Special_End(uint32 command);
};

} // namespace satemu::scu
