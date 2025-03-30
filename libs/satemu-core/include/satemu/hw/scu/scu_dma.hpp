#pragma once

#include <satemu/core/types.hpp>

namespace satemu::scu {

enum class DMATrigger {
    VBlankIN = 0,
    VBlankOUT = 1,
    HBlankIN = 2,
    Timer0 = 3,
    Timer1 = 4,
    SoundRequest = 5,
    SpriteDrawEnd = 6,
    Immediate = 7,
};

struct DMAChannel {
    DMAChannel() {
        Reset();
    }

    void Reset() {
        srcAddr = 0;   // initial value undefined
        dstAddr = 0;   // initial value undefined
        xferCount = 0; // initial value undefined
        srcAddrInc = 4;
        dstAddrInc = 2;
        updateSrcAddr = false;
        updateDstAddr = false;
        enabled = false;
        active = false;
        indirect = false;
        trigger = DMATrigger::Immediate;

        start = false;
        currSrcAddr = 0;
        currDstAddr = 0;
        currXferCount = 0;

        currIndirectSrc = 0;
        endIndirect = false;
    }

    uint32 srcAddr;     // DnR - Read address
    uint32 dstAddr;     // DnW - Write address
    uint32 xferCount;   // DnC - Transfer byte count (up to 1 MiB for level 0, 4 KiB for levels 1 and 2)
    uint32 srcAddrInc;  // DnAD.DnRA - Read address increment (0=0, 1=+4 bytes)
    uint32 dstAddrInc;  // DnAD.DnWA - Write address increment (+0,2,4,8,16,32,64,128 bytes)
    bool updateSrcAddr; // DnRUP - Update read address after transfer
    bool updateDstAddr; // DnWUP - Update write address after transfer
    bool enabled;       // DxEN - Enable
    bool active;        // Transfer active (triggered by trigger condition)
    bool indirect;      // DxMOD - Mode (false=direct, true=indirect)
    DMATrigger trigger; // DxFT2-0 - DMA Starting Factor

    bool start;            // Start transfer on next cycle
    uint32 currSrcAddr;    // Current read address
    uint32 currDstAddr;    // Current write address
    uint32 currXferCount;  // Current transfer count (stops when == xferCount)
    uint32 currSrcAddrInc; // Current read address increment
    uint32 currDstAddrInc; // Current write address increment

    uint32 currIndirectSrc; // Indirect data transfer source address
    bool endIndirect;       // Whether the end flag was sent on the current indirect transfer
};

} // namespace satemu::scu
