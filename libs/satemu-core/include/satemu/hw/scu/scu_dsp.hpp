#pragma once

#include <satemu/core/types.hpp>

#include <satemu/sys/bus.hpp>

#include <satemu/util/bit_ops.hpp>
#include <satemu/util/callback.hpp>
#include <satemu/util/debug_print.hpp>

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

    void WriteProgram(uint32 value) {
        // Cannot write while program is executing
        if (programExecuting) {
            return;
        }

        programRAM[PC++] = value;
    }

    uint32 ReadData() {
        // Cannot read while program is executing
        if (programExecuting) {
            return 0;
        }

        const uint8 bank = bit::extract<6, 7>(dataAddress);
        const uint8 offset = bit::extract<0, 5>(dataAddress);
        dataAddress++;
        return dataRAM[bank][offset];
    }

    void WriteData(uint32 value) {
        // Cannot write while program is executing
        if (programExecuting) {
            return;
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

private:
    sys::Bus &m_bus;

    CBTriggerDSPEnd m_cbTriggerDSPEnd;

    // X-Bus, Y-Bus and D1-Bus reads from [s]
    uint32 ReadSource(uint8 index);

    // D1-Bus writes to [d]
    void WriteD1Bus(uint8 index, uint32 value);

    // Immediate writes to [d]
    void WriteImm(uint8 index, uint32 value);

    // Checks if the current DSP flags pass the given condition
    bool CondCheck(uint8 cond) const;

    // Prepares a delayed jump to the given target address
    void DelayedJump(uint8 target);

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
