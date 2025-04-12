#pragma once

#include <satemu/core/types.hpp>

#include <array>
#include <vector>

namespace satemu::state {

inline namespace v1 {

    struct SCUDMAState {
        uint32 srcAddr;
        uint32 dstAddr;
        uint32 xferCount;
        uint32 srcAddrInc;
        uint32 dstAddrInc;
        bool updateSrcAddr;
        bool updateDstAddr;
        bool enabled;
        bool active;
        bool indirect;
        uint8 trigger;

        bool start;
        uint32 currSrcAddr;
        uint32 currDstAddr;
        uint32 currXferCount;
        uint32 currSrcAddrInc;
        uint32 currDstAddrInc;

        uint32 currIndirectSrc;
        bool endIndirect;
    };

    struct SCUDSPState {
        std::array<uint32, 256> programRAM;
        std::array<std::array<uint32, 64>, 4> dataRAM;

        bool programExecuting;
        bool programPaused;
        bool programEnded;
        bool programStep;

        uint8 PC;
        uint8 dataAddress;

        uint32 nextPC;
        uint8 jmpCounter;

        bool sign;
        bool zero;
        bool carry;
        bool overflow;

        std::array<uint8, 4> CT;

        uint64 ALU;
        uint64 AC;
        uint64 P;
        sint32 RX;
        sint32 RY;

        uint16 LOP;
        uint8 TOP;

        bool dmaRun;
        bool dmaToD0;
        bool dmaHold;
        uint8 dmaCount;
        uint8 dmaSrc;
        uint8 dmaDst;
        uint32 dmaReadAddr;
        uint32 dmaWriteAddr;
        uint32 dmaAddrInc;
    };

    struct SCUState {
        std::array<SCUDMAState, 3> dma;
        SCUDSPState dsp;

        enum class CartType { None, BackupMemory, DRAM8Mbit, DRAM32Mbit };
        CartType cartType;
        std::vector<uint8> dramCartData;

        uint32 intrMask;
        uint32 intrStatus;
        bool abusIntrAck;

        uint16 timer0Counter;
        uint16 timer0Compare;
        uint16 timer1Reload;
        bool timer1Enable;
        bool timer1Mode;

        bool wramSizeSelect;
    };

} // namespace v1

} // namespace satemu::state
